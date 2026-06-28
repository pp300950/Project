/*
การต่อสายไฟ

D5  → ปุ่มกด → GND
D6  → D0 เซนเซอร์รั้ว
D7  → D0 เซนเซอร์สระ
D2  → ขา + บัซเซอร์ → GND
3.3V → VCC เซนเซอร์ทั้งสอง
GND → GND เซนเซอร์ทั้งสอง
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ===== ตั้งค่าตรงนี้ =====
const char* ssid      = "PP";
const char* password  = "12345678";

// ===== LINE =====
#define LINE_TOKEN "+WCQuf3X+jrVPNhe61leiOB9TSgimmcEtn2YeK2q5DUV8amp4mCVY8kNzNIfw5fQVvc5dwR4JwzhcogU1WDo6SresUC5D4IZYqWpGtODwWbUnwjPSaWj3k0fUoU/Bwus6FiyxKYib5zfAIWHu5jVygdB04t89/1O/w1cDnyilFU="
#define USER_ID "Uf72325ea6cc9aee84558f42e01a07e33"

// ==========================

#define SENSOR_FENCE D6
#define SENSOR_POOL  D7
#define BUTTON_PIN   D5
#define BUZZER_PIN   D2

bool lastFence = HIGH;
bool lastPool  = HIGH;
bool curFence, curPool;
bool buttonState;

// ตั้งค่าโซนเวลาประเทศไทย (UTC +7 ชั่วโมง)
int timezone = 7 * 3600;
int dst = 0;

String getTimestamp() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (now < 86400) {
    return "ไม่สามารถดึงเวลาได้";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", timeinfo);
  return String(buffer);
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

void sendTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi ไม่ได้เชื่อมต่อ");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  http.begin(client, "https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(LINE_TOKEN));

  String jsonPayload =
    "{"
      "\"to\":\"" + String(USER_ID) + "\","
      "\"messages\":["
        "{"
          "\"type\":\"text\","
          "\"text\":\"" + jsonEscape(message) + "\""
        "}"
      "]"
    "}";

  int code = http.POST(jsonPayload);
  String response = http.getString();

  Serial.println("LINE HTTP Code: " + String(code));
  Serial.println("LINE Response: " + response);

  http.end();
}

void triggerBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

void alert(String location) {
  String currentTime = getTimestamp();

  String msg = "🚨 [ตรวจสอบคนไข้!]\n";
  msg += "📌 พิกัด: " + location + "\n";
  msg += "⏰ เวลา: " + currentTime;

  sendTelegram(msg);
  triggerBuzzer();
}

void setup() {
  Serial.begin(115200);

  // ถ้าเซนเซอร์เป็นแบบ active-low ให้ใช้ INPUT_PULLUP
  pinMode(SENSOR_FENCE, INPUT_PULLUP);
  pinMode(SENSOR_POOL,  INPUT_PULLUP);
  pinMode(BUTTON_PIN,   INPUT_PULLUP);
  pinMode(BUZZER_PIN,   OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.print("กำลังเชื่อม WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());

  Serial.println("กำลังซิงค์เวลาจากอินเทอร์เน็ต...");
  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);
  while (now < 86400) {
    delay(500);
    now = time(nullptr);
  }
  Serial.println("ซิงค์เวลาสำเร็จ: " + getTimestamp());

  // อ่านสถานะเริ่มต้นก่อนเข้า loop
  lastFence = digitalRead(SENSOR_FENCE);
  lastPool  = digitalRead(SENSOR_POOL);

  Serial.println("Initial Fence = " + String(lastFence));
  Serial.println("Initial Pool  = " + String(lastPool));

  sendTelegram("🟢 ระบบตรวจสอบคนไข้ พร้อมทำงาน!");
  Serial.println("=== Patient Monitoring System Ready ===");
}

void loop() {
  buttonState = digitalRead(BUTTON_PIN);
  curFence    = digitalRead(SENSOR_FENCE);
  curPool     = digitalRead(SENSOR_POOL);

  bool systemArmed = (buttonState == HIGH);  // ปุ่มยังไม่กด = พร้อมทำงาน

  // --- เซนเซอร์รั้ว ---
  if (curFence != lastFence) {
    delay(50);
    curFence = digitalRead(SENSOR_FENCE);

    if (curFence != lastFence) {
      lastFence = curFence;

      if (curFence == LOW) {
        Serial.println("[รั้ว] ⚠ มีคนเดินผ่าน!");
        if (systemArmed) {
          alert("บริเวณรั้ว");
        } else {
          Serial.println("ระบบยังไม่พร้อมทำงาน");
        }
      }
    }
  }

  // --- เซนเซอร์สระว่ายน้ำ ---
  if (curPool != lastPool) {
    delay(50);
    curPool = digitalRead(SENSOR_POOL);

    if (curPool != lastPool) {
      lastPool = curPool;

      if (curPool == LOW) {
        Serial.println("[สระ] ⚠ มีคนเดินผ่าน!");
        if (systemArmed) {
          alert("บริเวณสระว่ายน้ำ");
        } else {
          Serial.println("ระบบยังไม่พร้อมทำงาน");
        }
      }
    }
  }
}