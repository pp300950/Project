/*
 * ระบบตรวจจับคนผ่านประตู (HC-SR04)
 *
 * การต่อสายไฟ:
 *   HC-SR04 TRIG → D1
 *   HC-SR04 ECHO → D2
 *   HC-SR04 VCC  → 3.3V
 *   HC-SR04 GND  → GND
 *   Buzzer +     → D5
 *   Buzzer -     → GND
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

const char* ssid     = "PP";
const char* password = "12345678";

#define LINE_TOKEN "+WCQuf3X+jrVPNhe61leiOB9TSgimmcEtn2YeK2q5DUV8amp4mCVY8kNzNIfw5fQVvc5dwR4JwzhcogU1WDo6SresUC5D4IZYqWpGtODwWbUnwjPSaWj3k0fUoU/Bwus6FiyxKYib5zfAIWHu5jVygdB04t89/1O/w1cDnyilFU="
#define USER_ID    "Uf72325ea6cc9aee84558f42e01a07e33"

#define TRIG_PIN   D1
#define ECHO_PIN   D2
#define BUZZER_PIN D5

#define DETECT_DISTANCE_CM  80
#define COOLDOWN_MS         5000

unsigned long lastAlertTime = 0;

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
  Serial.println("[LINE] " + String(code));
  http.end();
}

// ✅ แก้: timeout ลดจาก 30,000 → 8,000 µs
//    (ระยะ 80cm ใช้เวลาประมาณ 4,700 µs ก็พอแล้ว)
float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 8000); // ✅ 8,000 µs แทน 30,000
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

void alert(float dist) {
  // ✅ ข้อความ LINE ตามที่ต้องการ
  String msg = "❗❗ แจ้งเตือน ❗❗\n";
  msg += "📌 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ รั้วบ้าน🏚\n";
  msg += "🕐 เวลา: " + getTimestamp();

  Serial.println(msg);
  triggerBuzzer();  // ✅ buzzer ก่อน (ไม่รอ LINE)
  sendLine(msg);    // ✅ ส่ง LINE หลัง buzzer
  lastAlertTime = millis();
}

// ================================================

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(TRIG_PIN,   LOW);
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

  sendLine("🟢 ระบบพร้อมทำงาน\n⏰ " + getTimestamp());
  Serial.println("=== Ready | ระยะตรวจจับ: " + String(DETECT_DISTANCE_CM) + " cm ===");
}

void loop() {
  float dist = readDistanceCM();

  if (dist > 0 && dist < DETECT_DISTANCE_CM) {
    Serial.println("[Sensor] ตรวจพบ " + String(dist, 1) + " cm");

    if (millis() - lastAlertTime >= COOLDOWN_MS) {
      alert(dist);
    } else {
      Serial.println("[Cooldown] รอ " +
        String((COOLDOWN_MS - (millis() - lastAlertTime)) / 1000) + "s");
    }
  }

  // ✅ ลบ delay(100) ออกทั้งหมด → loop วนเร็วสุด
}