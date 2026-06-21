/*
  =====================================================================
  esp01_shoe.ino — เฟิร์มแวร์ฝั่งรองเท้า (ESP-01)
  =====================================================================

  หน้าที่ของบอร์ดนี้ (ตามเอกสารสถาปัตยกรรม ชั้นที่ 1):
  - เป็น "sensor บริสุทธิ์" เท่านั้น ไม่ตัดสินใจเรื่อง logic แจ้งเตือนใดๆ
  - อ่านปุ่มกดใต้รองเท้า 1 ตัว -> digital 0/1
      HIGH (1) = ไม่ได้เหยียบ = ถอดรองเท้า/ยกเท้าขึ้น
      LOW  (0) = เหยียบอยู่   = ใส่รองเท้าอยู่ปกติ
    (ใช้ INPUT_PULLUP โดยให้ปุ่มต่อลง GND เมื่อเหยียบ — แบบ active-low เหมือนโค้ดต้นแบบ main2.ino)
  - ส่งสถานะแบบ "event-based" คือส่งทันทีที่ state เปลี่ยน + ส่ง heartbeat เป็นระยะ
    เพื่อให้ฝั่งมาสเตอร์รู้ว่าบอร์ดนี้ยังออนไลน์อยู่ (ไม่ได้แบตหมด/หลุดจาก AP)
  - ใช้ไฟล์เดียวกัน flash ทั้งรองเท้าซ้ายและขวา ต่างกันแค่ device_id ที่ตั้งผ่าน Config Portal

  ฮาร์ดแวร์ที่ใช้: ESP-01 (ESP8266 รุ่น 2 ขา GPIO)
  ดูตารางต่อสายไฟทั้งหมดได้ใน README.md

  หมายเหตุสำคัญสำหรับ ESP-01:
  - ESP-01 ไม่มีพอร์ต USB ในตัว ต้องใช้ USB-to-TTL adapter ในการอัปโหลดโค้ด
  - ตอนอัปโหลดโค้ดต้องต่อ GPIO0 ลง GND ชั่วคราว (เข้าโหมด flash) แล้วถอดออกตอนรันจริง
  - ใช้ GPIO2 เท่านั้นสำหรับปุ่มกด เพราะ GPIO0 เป็นขากำหนดโหมดบูต ใช้งานเป็น input ทั่วไปไม่ได้
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "config.h"
#include "storage.h"
#include "config_portal.h"

DeviceConfig deviceConfig;

bool lastButtonState = HIGH;  // เริ่มต้นสมมติว่าใส่รองเท้าอยู่ (ปุ่มยังไม่ถูกเหยียบเปลี่ยนสถานะ)
unsigned long lastHeartbeatAt = 0;
unsigned long bootTime = 0;

// ---------------------------------------------------------------------
// ส่งสถานะไปยัง ESP8266 มาสเตอร์ ผ่าน HTTP POST (วง local ของ AP เท่านั้น)
// eventType: "state_change" หรือ "heartbeat"
// ---------------------------------------------------------------------
bool sendStatusToMaster(bool footIn, const String &eventType) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] ยังไม่ได้เชื่อมต่อกับมาสเตอร์ ข้ามการส่งรอบนี้");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://" + String(MASTER_HOST_IP) + ":" + String(MASTER_PORT) + MASTER_STATUS_PATH;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // payload ที่ส่งไปคือสถานะดิบเท่านั้น — มาสเตอร์เป็นคนตัดสินใจต่อว่าจะแจ้งเตือนหรือไม่
  String payload =
    "{"
      "\"device_id\":\"" + deviceConfig.deviceId + "\","
      "\"event\":\"" + eventType + "\","
      "\"foot_in_shoe\":" + String(footIn ? "true" : "false") + ","
      "\"uptime_ms\":" + String(millis())
    "}";

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.println("[Master] ส่งสำเร็จ (" + eventType + ") -> HTTP " + String(httpCode));
  } else {
    Serial.println("[Master] ส่งล้มเหลว: " + http.errorToString(httpCode));
  }

  http.end();
  return httpCode > 0;
}

void connectToMasterAP() {
  Serial.println("[WiFi] กำลังเชื่อมต่อ AP ของมาสเตอร์: " + deviceConfig.apSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(deviceConfig.apSsid.c_str(), deviceConfig.apPassword.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(WIFI_RETRY_DELAY_MS);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] เชื่อมต่อสำเร็จ — IP ของตัวเอง: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] เชื่อมต่อไม่สำเร็จ จะลองใหม่ใน loop()");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP-01 Shoe Sensor Booting ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // โหลดค่าตั้งค่าจาก EEPROM (หรือใช้ default ถ้ายังไม่เคยตั้งค่า)
  bool hasCustomConfig = loadConfig(deviceConfig);
  Serial.println("[Config] Device ID: " + deviceConfig.deviceId +
                  (hasCustomConfig ? " (ตั้งค่าเองแล้ว)" : " (ใช้ค่า default)"));

  // เปิด Config Portal คู่กันไปด้วยเสมอ 5 นาทีแรกหลังบูต (ตามเอกสารสถาปัตยกรรม)
  startConfigPortal();

  connectToMasterAP();

  lastButtonState = digitalRead(BUTTON_PIN);
  bootTime = millis();
  lastHeartbeatAt = millis();

  Serial.println("[Setup] สถานะเริ่มต้น: " + String(lastButtonState == LOW ? "ใส่รองเท้าอยู่" : "ไม่ได้ใส่รองเท้า"));
  Serial.println("=== Ready ===\n");
}

void loop() {
  // ให้ portal ทำงานคู่กันได้ตลอด 5 นาทีแรก ไม่ block logic หลัก
  handleConfigPortal();

  // ถ้า WiFi หลุดระหว่างทาง ลองเชื่อมต่อใหม่แบบไม่ block (เช็คทุกรอบ loop)
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastRetryAt = 0;
    if (millis() - lastRetryAt > 10000) {
      lastRetryAt = millis();
      Serial.println("[WiFi] หลุดการเชื่อมต่อ กำลังลองใหม่...");
      WiFi.begin(deviceConfig.apSsid.c_str(), deviceConfig.apPassword.c_str());
    }
  }

  bool currentButtonState = digitalRead(BUTTON_PIN);

  // --- ตรวจจับการเปลี่ยนสถานะแบบ debounce ---
  if (currentButtonState != lastButtonState) {
    delay(DEBOUNCE_MS);
    currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState != lastButtonState) {
      lastButtonState = currentButtonState;
      bool footIn = (currentButtonState == LOW); // LOW = เหยียบอยู่ = ใส่รองเท้า

      Serial.println(footIn ? "[Sensor] ใส่รองเท้าแล้ว" : "[Sensor] ถอดรองเท้า/ยกเท้าขึ้น");
      sendStatusToMaster(footIn, "state_change");
    }
  }

  // --- Heartbeat เป็นระยะ ---
  if (millis() - lastHeartbeatAt > HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = millis();
    bool footIn = (digitalRead(BUTTON_PIN) == LOW);
    sendStatusToMaster(footIn, "heartbeat");
  }
}
