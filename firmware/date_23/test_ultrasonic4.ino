/*
 * ระบบตรวจจับ 2 จุด (HC-SR04 x2) + แจ้งเตือน LINE
 *
 * การต่อสายไฟ:
 *   === ตัวที่ 1: รั้วบ้าน ===
 *   HC-SR04 #1 TRIG → D1
 *   HC-SR04 #1 ECHO → D2
 *
 *   === ตัวที่ 2: สระว่ายน้ำ ===
 *   HC-SR04 #2 TRIG → D6
 *   HC-SR04 #2 ECHO → D7
 *
 *   Buzzer +  → D5
 *   Buzzer -  → GND
 *   VCC ทั้งคู่ → 3.3V (หรือ 5V ถ้าจับได้ไม่ดี)
 *   GND ทั้งคู่ → GND
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* ssid     = "PP";
const char* password = "12345678";

#define LINE_TOKEN "+WCQuf3X+jrVPNhe61leiOB9TSgimmcEtn2YeK2q5DUV8amp4mCVY8kNzNIfw5fQVvc5dwR4JwzhcogU1WDo6SresUC5D4IZYqWpGtODwWbUnwjPSaWj3k0fUoU/Bwus6FiyxKYib5zfAIWHu5jVygdB04t89/1O/w1cDnyilFU="
#define USER_ID    "Uf72325ea6cc9aee84558f42e01a07e33"

// === เซ็นเซอร์ตัวที่ 1: รั้วบ้าน ===
#define TRIG1_PIN          D1
#define ECHO1_PIN          D2
#define DETECT1_DIST_CM    16
#define PULSIN1_TIMEOUT    4000

// === เซ็นเซอร์ตัวที่ 2: สระว่ายน้ำ ===
#define TRIG2_PIN          D6
#define ECHO2_PIN          D7
#define DETECT2_DIST_CM    20
#define PULSIN2_TIMEOUT    4500

// === Buzzer ===
#define BUZZER_PIN         D5
#define BUZZER_MAX_MS      2000   // ดังได้ไม่เกิน 2 วินาที

// === Cooldown ===
#define COOLDOWN_MS        5000   // detect cooldown (buzzer/ระบบ)
#define LINE_SAME_CD_MS    30000  // ห้ามส่ง LINE ข้อความเดิมซ้ำภายใน 30 วิ

// === Confirm ติดต่อกันกี่ครั้งถึงทริก ===
#define DETECT_CONFIRM     3

// --- ติดตามสถานะ ---
unsigned long lastAlert1   = 0;   // cooldown buzzer/detect รั้ว
unsigned long lastAlert2   = 0;   // cooldown buzzer/detect สระ
unsigned long lastLine1    = 0;   // ครั้งล่าสุดที่ส่ง LINE รั้ว
unsigned long lastLine2    = 0;   // ครั้งล่าสุดที่ส่ง LINE สระ

int detectCount1 = 0;
int detectCount2 = 0;

int timezone = 7 * 3600;
int dst = 0;

// === Buzzer non-blocking ===
// pattern: ON 300ms → OFF 200ms → ON 300ms → OFF 200ms → ON 300ms → OFF (รวม ~1300ms < 2000ms)
unsigned long buzzerStartTime = 0;
int  buzzerStep   = 0;
bool buzzerActive = false;

// ================================================

String getTimestamp() {
  time_t now = time(nullptr);
  if (now < 86400) return "ไม่สามารถดึงเวลาได้";
  struct tm* t = localtime(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", t);
  return String(buf);
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

void sendLine(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, "https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(LINE_TOKEN));

  String payload =
    "{\"to\":\"" + String(USER_ID) + "\","
    "\"messages\":[{\"type\":\"text\",\"text\":\""
    + jsonEscape(message) + "\"}]}";

  int code = http.POST(payload);
  Serial.println("[LINE] HTTP " + String(code));
  http.end();
}

float readDistanceCM(int trigPin, int echoPin, unsigned long timeout_us) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, timeout_us);
  if (duration == 0) return -1.0;
  return duration * 0.034 / 2.0;
}

// --- Buzzer: เริ่มดังใหม่ (รีเซ็ต step) ---
void startBuzzer() {
  buzzerStep      = 0;
  buzzerActive    = true;
  buzzerStartTime = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

// --- Buzzer: อัปเดตทุก loop ---
// times[] = จุดเปลี่ยนสถานะ (ms): 300 OFF | 500 ON | 800 OFF | 1000 ON | 1300 OFF → จบ
// ทุก step ที่เป็นคี่ = LOW, คู่ = HIGH; step6 ปิดสนิท
// ระยะเวลารวมสูงสุด = 1300ms << BUZZER_MAX_MS (2000ms)
void updateBuzzer() {
  if (!buzzerActive) return;

  unsigned long elapsed = millis() - buzzerStartTime;

  // Hard-cut: ถ้าเกิน 2 วิให้ตัดทิ้งทันที รอ trigger ใหม่
  if (elapsed >= BUZZER_MAX_MS) {
    buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("[Buzzer] หยุด (ถึง 2 วิ)");
    return;
  }

  unsigned long times[] = {300, 500, 800, 1000, 1300};  // 5 จุดเปลี่ยน, 6 state

  if (buzzerStep < 5 && elapsed >= times[buzzerStep]) {
    buzzerStep++;
    // step1,3,5 = LOW (ระหว่างบี๊บ) | step2,4 = HIGH (บี๊บ)
    if (buzzerStep % 2 == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  // สิ้นสุด pattern ที่ step5 (elapsed>=1300ms) → ปิด
  if (buzzerStep >= 5 && elapsed >= times[4]) {
    buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ================================================

// แจ้งเตือน: รั้วบ้าน
void alertFence() {
  lastAlert1    = millis();
  detectCount1  = 0;

  Serial.println("[ALERT] รั้วบ้าน — " + getTimestamp());
  startBuzzer();

  // ตรวจว่าเพิ่งส่ง LINE รั้วภายใน 30 วิหรือยัง
  if (millis() - lastLine1 >= LINE_SAME_CD_MS) {
    String msg  = "❗❗ แจ้งเตือน ❗❗\n";
    msg        += "📌 ตรวจพบผู้ป่วย ณ\n";
    msg        += "บริเวณ รั้วบ้าน🏚\n";
    msg        += "🕐 เวลา: " + getTimestamp();
    sendLine(msg);
    lastLine1 = millis();
  } else {
    Serial.println("[LINE] รั้ว — ข้ามส่ง (cooldown 30s เหลือ "
      + String((LINE_SAME_CD_MS - (millis() - lastLine1)) / 1000) + "s)");
  }
}

// แจ้งเตือน: สระว่ายน้ำ
void alertPool() {
  lastAlert2    = millis();
  detectCount2  = 0;

  Serial.println("[ALERT] สระว่ายน้ำ — " + getTimestamp());
  startBuzzer();

  // ตรวจว่าเพิ่งส่ง LINE สระภายใน 30 วิหรือยัง
  if (millis() - lastLine2 >= LINE_SAME_CD_MS) {
    String msg  = "❗❗ แจ้งเตือน ❗❗\n";
    msg        += "📌 ตรวจพบผู้ป่วย ณ\n";
    msg        += "บริเวณ สระว่ายน้ำ🏊\n";
    msg        += "🕐 เวลา: " + getTimestamp();
    sendLine(msg);
    lastLine2 = millis();
  } else {
    Serial.println("[LINE] สระ — ข้ามส่ง (cooldown 30s เหลือ "
      + String((LINE_SAME_CD_MS - (millis() - lastLine2)) / 1000) + "s)");
  }
}

// ================================================

void setup() {
  Serial.begin(115200);

  pinMode(TRIG1_PIN,  OUTPUT);
  pinMode(ECHO1_PIN,  INPUT);
  pinMode(TRIG2_PIN,  OUTPUT);
  pinMode(ECHO2_PIN,  INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRIG1_PIN,  LOW);
  digitalWrite(TRIG2_PIN,  LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.print("กำลังเชื่อม WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());

  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 86400) { delay(500); now = time(nullptr); }
  Serial.println("เวลา: " + getTimestamp());

  sendLine("🟢 ระบบพร้อมทำงาน\n"
           "📌 รั้วบ้าน: " + String(DETECT1_DIST_CM) + " cm\n"
           "📌 สระว่ายน้ำ: " + String(DETECT2_DIST_CM) + " cm\n"
           "⏰ " + getTimestamp());

  Serial.println("=== Ready ===");
  Serial.println("รั้วบ้าน   : " + String(DETECT1_DIST_CM) + " cm");
  Serial.println("สระว่ายน้ำ: " + String(DETECT2_DIST_CM) + " cm");
}

void loop() {
  updateBuzzer();

  // --- ตรวจจับ: รั้วบ้าน ---
  float dist1 = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
  if (dist1 < 0) {
    detectCount1 = 0;
    Serial.println("[รั้วบ้าน] ไม่มีสัญญาณ (timeout)");
  } else {
    Serial.println("[รั้วบ้าน] ระยะ: " + String(dist1, 1) + " cm");
    if (dist1 < DETECT1_DIST_CM) {
      detectCount1++;
      Serial.println("[รั้วบ้าน] confirm: " + String(detectCount1) + "/" + String(DETECT_CONFIRM));
      if (detectCount1 >= DETECT_CONFIRM && millis() - lastAlert1 >= COOLDOWN_MS) {
        alertFence();
      }
    } else {
      detectCount1 = 0;
    }
  }

  // --- ตรวจจับ: สระว่ายน้ำ ---
  float dist2 = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
  if (dist2 < 0) {
    detectCount2 = 0;
    Serial.println("[สระว่ายน้ำ] ไม่มีสัญญาณ (timeout)");
  } else {
    Serial.println("[สระว่ายน้ำ] ระยะ: " + String(dist2, 1) + " cm");
    if (dist2 < DETECT2_DIST_CM) {
      detectCount2++;
      Serial.println("[สระว่ายน้ำ] confirm: " + String(detectCount2) + "/" + String(DETECT_CONFIRM));
      if (detectCount2 >= DETECT_CONFIRM && millis() - lastAlert2 >= COOLDOWN_MS) {
        alertPool();
      }
    } else {
      detectCount2 = 0;
    }
  }

  delay(50);
}
