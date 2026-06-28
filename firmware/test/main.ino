#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ===== ตั้งค่าตรงนี้ =====
const char* ssid      = "PP";
const char* password  = "12345678";
const char* botToken  = "8654209805:AAHu1zNL6D740Mr_Gtwu7MRkydPOpsq8Cns";
const char* chatID    = "8217258151";
// ==========================

#define SENSOR_FENCE D6
#define SENSOR_POOL  D7
#define BUTTON_PIN   D5
#define BUZZER_PIN   D2

bool lastFence = HIGH;
bool lastPool  = HIGH;
bool curFence, curPool;
bool buttonState;

// --- ส่ง Telegram ---
void sendTelegram(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi ไม่ได้เชื่อมต่อ");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String url = "https://api.telegram.org/bot";
  url += botToken;
  url += "/sendMessage?chat_id=";
  url += chatID;
  url += "&text=";
  url += message;

  http.begin(client, url);
  int code = http.GET();
  Serial.println("Telegram sent — HTTP: " + String(code));
  http.end();
}

// --- บัซเซอร์ ---
void triggerBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

// --- แจ้งเตือนทั้งบัซเซอร์และ Telegram ---
void alert(String location) {
  triggerBuzzer();
  String msg = "%E2%9A%A0+Alert!+";   // ⚠ Alert!
  msg += location;
  sendTelegram(msg);
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_FENCE, INPUT);
  pinMode(SENSOR_POOL,  INPUT);
  pinMode(BUTTON_PIN,   INPUT_PULLUP);
  pinMode(BUZZER_PIN,   OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // เชื่อม WiFi
  Serial.print("กำลังเชื่อม WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());

  sendTelegram("System+Ready+%F0%9F%9F%A2");  // แจ้งว่าระบบพร้อม
  Serial.println("=== Security System Ready ===");
}

void loop() {
  buttonState = digitalRead(BUTTON_PIN);
  curFence    = digitalRead(SENSOR_FENCE);
  curPool     = digitalRead(SENSOR_POOL);

  bool systemArmed = (buttonState == HIGH);

  // --- เซนเซอร์รั้ว ---
  if (curFence != lastFence) {
    delay(50);
    curFence = digitalRead(SENSOR_FENCE);
    if (curFence != lastFence) {
      if (curFence == LOW) {
        Serial.println("[รั้ว] ⚠ มีคนเดินผ่านรั้ว!");
        if (systemArmed) {
          Serial.println("       🔔 แจ้งเตือน!");
          alert("Detected+at+FENCE");
        } else {
          Serial.println("       🔕 ระบบปิดอยู่");
        }
      } else {
        Serial.println("[รั้ว] ✅ ไม่มีคนเดินผ่านรั้ว");
      }
      lastFence = curFence;
    }
  }

  // --- เซนเซอร์สระว่ายน้ำ ---
  if (curPool != lastPool) {
    delay(50);
    curPool = digitalRead(SENSOR_POOL);
    if (curPool != lastPool) {
      if (curPool == LOW) {
        Serial.println("[สระ] ⚠ มีคนเดินผ่านสระว่ายน้ำ!");
        if (systemArmed) {
          Serial.println("       🔔 แจ้งเตือน!");
          alert("Detected+at+POOL");
        } else {
          Serial.println("       🔕 ระบบปิดอยู่");
        }
      } else {
        Serial.println("[สระ] ✅ ไม่มีคนเดินผ่านสระว่ายน้ำ");
      }
      lastPool = curPool;
    }
  }
}