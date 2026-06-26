/*
 * ระบบตรวจจับ 2 จุด (HC-SR04 x2)
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
#define DETECT1_DIST_CM    40      // ระยะตรวจจับรั้วบ้าน
#define PULSIN1_TIMEOUT    8000    // µs (80cm × 58 ≈ 4,640 µs + margin)

// === เซ็นเซอร์ตัวที่ 2: สระว่ายน้ำ ===
#define TRIG2_PIN          D6
#define ECHO2_PIN          D7
#define DETECT2_DIST_CM    20      // ระยะตรวจจับสระ (20 ซม.)
#define PULSIN2_TIMEOUT    2500    // µs (20cm × 58 ≈ 1,160 µs + margin)

// === Buzzer ===
#define BUZZER_PIN         D5

// === Cooldown แยกกันแต่ละจุด ===
#define COOLDOWN_MS        5000

unsigned long lastAlert1 = 0;   // cooldown รั้วบ้าน
unsigned long lastAlert2 = 0;   // cooldown สระว่ายน้ำ

int timezone = 7 * 3600;
int dst = 0;

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

// อ่านระยะ — ส่ง trig/echo pin และ timeout เข้ามา
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

void triggerBuzzer() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < 2) delay(200);
  }
}

// แจ้งเตือน: รั้วบ้าน
void alertFence() {
  String msg = "❗❗ แจ้งเตือน ❗❗\n";
  msg += "📌 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ รั้วบ้าน🏚\n";
  msg += "🕐 เวลา: " + getTimestamp();

  Serial.println("[ALERT] รั้วบ้าน");
  triggerBuzzer();
  sendLine(msg);
  lastAlert1 = millis();
}

// แจ้งเตือน: สระว่ายน้ำ
void alertPool() {
  String msg = "⚡⚡ แจ้งเตือน ⚡⚡\n";
  msg += "💦 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ สระว่ายน้ำ🏊\n";
  msg += "🕐 เวลา: " + getTimestamp();

  Serial.println("[ALERT] สระว่ายน้ำ");
  triggerBuzzer();
  sendLine(msg);
  lastAlert2 = millis();
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

  // --- ตรวจจับ: รั้วบ้าน ---
  float dist1 = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
  if (dist1 > 0 && dist1 < DETECT1_DIST_CM) {
    Serial.println("[รั้วบ้าน] " + String(dist1, 1) + " cm");
    if (millis() - lastAlert1 >= COOLDOWN_MS) {
      alertFence();
    } else {
      Serial.println("[Cooldown รั้ว] รอ " +
        String((COOLDOWN_MS - (millis() - lastAlert1)) / 1000) + "s");
    }
  }

  // --- ตรวจจับ: สระว่ายน้ำ ---
  float dist2 = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
  if (dist2 > 0 && dist2 < DETECT2_DIST_CM) {
    Serial.println("[สระว่ายน้ำ] " + String(dist2, 1) + " cm");
    if (millis() - lastAlert2 >= COOLDOWN_MS) {
      alertPool();
    } else {
      Serial.println("[Cooldown สระ] รอ " +
        String((COOLDOWN_MS - (millis() - lastAlert2)) / 1000) + "s");
    }
  }
}