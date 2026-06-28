/*
 * ระบบตรวจจับ 2 จุด (HC-SR04 x2) + แจ้งเตือน LINE + ส่งเสียงผ่าน Arduino
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
 *
 *   === ส่งคำสั่งไป Arduino ===
 *   ESP D8 (TX) → Arduino Pin10 (RX) ผ่าน 220Ω
 *   ESP D0 (RX) ← Arduino Pin11 (TX)
 *   GND ร่วมกัน
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>   // เพิ่ม
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
#define BUZZER_DURATION_MS 2000

// === Cooldown LINE 30 วิ ===
#define COOLDOWN_MS        30000

// === Confirm ติดต่อกันกี่ครั้งถึงทริก ===
#define DETECT_CONFIRM     3

// === Serial ไปหา Arduino ===
// D8=GPIO15 (TX ส่งออก), D0=GPIO16 (RX รับเข้า)
SoftwareSerial arduinoSerial(D0, D8); // RX=D0, TX=D8

unsigned long lastAlert1 = 0;
unsigned long lastAlert2 = 0;
int detectCount1 = 0;
int detectCount2 = 0;

bool triggered1 = false;
bool triggered2 = false;

int timezone = 7 * 3600;
int dst = 0;

unsigned long buzzerStartTime = 0;
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

// === ส่งคำสั่งให้ Arduino เล่นเสียง ===   // [เพิ่มใหม่]
void sendArduino(char cmd) {
  arduinoSerial.print(cmd);
  Serial.println("[ARDUINO] ส่งคำสั่ง: " + String(cmd));
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

void startBuzzer() {
  buzzerActive = true;
  buzzerStartTime = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

void updateBuzzer() {
  if (!buzzerActive) return;
  if (millis() - buzzerStartTime >= BUZZER_DURATION_MS) {
    buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void alertFence() {
  lastAlert1 = millis();
  triggered1 = true;
  detectCount1 = 0;

  String msg = "❗❗ แจ้งเตือน ❗❗\n";
  msg += "🏚 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ รั้วบ้าน🏚\n";
  msg += "🕐 เวลา: " + getTimestamp();

  Serial.println("[ALERT] รั้วบ้าน — " + getTimestamp());
  startBuzzer();
  sendLine(msg);
  sendArduino('1');   // [เพิ่มใหม่] สั่ง Arduino เล่น 001.mp3
}

void alertPool() {
  lastAlert2 = millis();
  triggered2 = true;
  detectCount2 = 0;

  String msg = "⚡⚡ แจ้งเตือน ⚡⚡\n";
  msg += "💦 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ สระว่ายน้ำ💦\n";
  msg += "🕐 เวลา: " + getTimestamp();

  Serial.println("[ALERT] สระว่ายน้ำ — " + getTimestamp());
  startBuzzer();
  sendLine(msg);
  sendArduino('2');   // [เพิ่มใหม่] สั่ง Arduino เล่น 002.mp3
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

  arduinoSerial.begin(9600);   // [เพิ่มใหม่]

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
      if (detectCount1 >= DETECT_CONFIRM
          && !triggered1
          && millis() - lastAlert1 >= COOLDOWN_MS) {
        alertFence();
      }
    } else {
      detectCount1 = 0;
      triggered1 = false;
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
      if (detectCount2 >= DETECT_CONFIRM
          && !triggered2
          && millis() - lastAlert2 >= COOLDOWN_MS) {
        alertPool();
      }
    } else {
      detectCount2 = 0;
      triggered2 = false;
    }
  }

  delay(50);
}