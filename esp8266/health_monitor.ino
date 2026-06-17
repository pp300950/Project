/*
  Health Monitor — ESP8266 Sender
  ส่งข้อมูล steps, cadence, heart rate ไปยัง FastAPI ทุก 5 นาที

  Library ที่ต้องติดตั้ง:
    - ESP8266WiFi (built-in)
    - ESP8266HTTPClient (built-in)
    - ArduinoJson (v6)
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// ── WiFi Config ──────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ── Server Config ────────────────────────────
const char* SERVER_URL    = "https://your-app.onrender.com/api/device/data";
const char* API_KEY       = "change-this-to-your-esp-secret";  // ต้องตรงกับ .env
const char* PATIENT_CODE  = "PT-2024-001";
const char* DEVICE_ID     = "ESP-001";

// ── ส่งข้อมูลทุก 5 นาที ─────────────────────
const unsigned long SEND_INTERVAL_MS = 5UL * 60UL * 1000UL;
unsigned long lastSendTime = 0;

// ── ตัวแปรสะสมก้าว (ต่อ sensor จริง) ────────
int   totalSteps    = 0;
float cadenceNow    = 0.0;
int   heartRateNow  = 0;
float distanceM     = 0.0;
int   activeSec     = 0;

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n[BOOT] Health Monitor ESP8266");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  // อ่านค่า sensor จริงตรงนี้ เช่น MPU6050 สำหรับก้าว
  // readSensors();

  // จำลองข้อมูลสำหรับ test
  simulateSensorData();

  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL_MS || lastSendTime == 0) {
    lastSendTime = now;
    sendData();
  }

  delay(1000);
}

// ── จำลองข้อมูล (แทนด้วย sensor จริง) ──────
void simulateSensorData() {
  totalSteps   += random(10, 30);
  cadenceNow    = random(70, 95);
  heartRateNow  = random(65, 85);
  distanceM     = totalSteps * 0.75;  // ก้าวละ ~75 ซม.
  activeSec    += 60;
}

// ── ส่งข้อมูลไปยัง Server ───────────────────
void sendData() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected, skipping send");
    WiFi.reconnect();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // ข้าม SSL verify (สำหรับ dev)
  // client.setFingerprint(SERVER_FINGERPRINT);  // ใช้ production

  HTTPClient http;
  http.begin(client, SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Api-Key", API_KEY);

  // สร้าง JSON body
  StaticJsonDocument<256> doc;
  doc["patient_code"]          = PATIENT_CODE;
  doc["steps"]                 = totalSteps;
  doc["cadence"]               = cadenceNow;
  doc["heart_rate"]            = heartRateNow;
  doc["distance_m"]            = distanceM;
  doc["activity_duration_sec"] = activeSec;
  doc["device_id"]             = DEVICE_ID;

  String body;
  serializeJson(doc, body);

  Serial.printf("[HTTP] POST → %s\n", SERVER_URL);
  Serial.printf("[DATA] steps=%d cadence=%.1f hr=%d dist=%.1fm\n",
                totalSteps, cadenceNow, heartRateNow, distanceM);

  int httpCode = http.POST(body);

  if (httpCode > 0) {
    Serial.printf("[HTTP] Response: %d\n", httpCode);
    if (httpCode == 201) {
      Serial.println("[HTTP] ✓ Data sent successfully");
    }
  } else {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}
