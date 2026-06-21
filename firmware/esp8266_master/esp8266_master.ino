/*
  =====================================================================
  esp8266_master.ino — เฟิร์มแวร์ ESP8266 มาสเตอร์
  =====================================================================

  ทำงาน 4 อย่างพร้อมกันตามเอกสารสถาปัตยกรรม (ชั้นที่ 2):

  1. AP โหมด    — ปล่อย WiFi ของตัวเอง รับสถานะจากรองเท้า ESP-01 ทั้งสองข้าง
  2. STA โหมด   — ต่อ WiFi บ้าน/มือถือ เพื่อขึ้นเน็ตไปหา FastAPI server
                  (ทำงาน 2 โหมดพร้อมกันด้วย WIFI_AP_STA)
  3. Ultrasonic — อ่าน HC-SR04 x3 ตัว (สระ/ประตู/รั้ว) ทุก 300ms
                  ถ้าระยะ < ALERT_DISTANCE_CM -> บัซเซอร์ดังทันที (local, ไม่รอ server)
                  แล้วค่อยส่ง LINE + log ขึ้น server
  4. Mini dashboard — เสิร์ฟหน้าเว็บใน path "/" ผ่าน AP โดยตรง (ไม่ผ่านเน็ต)

  รับสถานะจากรองเท้า: POST /api/shoe-status (รองเท้าทั้งสองข้างยิงมาที่นี่)
  ตั้งค่าผ่านพอร์ทัล: GET/POST /config (เปิดได้ 5 นาทีแรกหลังบูตเท่านั้น)

  ดูตารางต่อสายไฟ HC-SR04 x3 + buzzer ทั้งหมดได้ใน README.md
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config.h"
#include "storage.h"

// ประกาศตัวแปร global ตัวนี้ไว้ก่อน include ไฟล์ที่ใช้ "extern ESP8266WebServer webServer"
// (config_portal.h และ mini_dashboard.h) เพื่อให้ลำดับการมองเห็นตัวแปรชัดเจนไม่กำกวม
ESP8266WebServer webServer(80); // ใช้ตัวเดียวกันทั้ง dashboard, config portal, และรับสถานะจากรองเท้า
MasterConfig masterConfig;

#include "ultrasonic.h"
#include "notify.h"
#include "server_client.h"
#include "config_portal.h"
#include "mini_dashboard.h"

unsigned long lastUltrasonicPollAt = 0;
unsigned long lastMasterHeartbeatAt = 0;

// ---------------------------------------------------------------------
// รับสถานะจากรองเท้า ESP-01 (ทั้งซ้ายและขวายิงมาที่ path เดียวกันนี้)
// ---------------------------------------------------------------------
void handleShoeStatus() {
  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"error\":\"missing body\"}");
    return;
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, webServer.arg("plain"));

  if (err) {
    webServer.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return;
  }

  String deviceId = doc["device_id"].as<String>();
  String eventType = doc["event"].as<String>();
  bool footIn = doc["foot_in_shoe"].as<bool>();

  // อัปเดตสถานะในหน่วยความจำ (ใช้แสดงผลใน mini dashboard)
  ShoeStatus* target = nullptr;
  if (deviceId == shoeLeft.deviceId) target = &shoeLeft;
  else if (deviceId == shoeRight.deviceId) target = &shoeRight;

  if (target != nullptr) {
    target->footIn = footIn;
    target->lastSeenMs = millis();
    target->everSeen = true;
  } else {
    Serial.println("[Shoe] ได้รับสถานะจาก device_id ที่ไม่รู้จัก: " + deviceId);
  }

  if (eventType == "state_change") {
    String logMsg = deviceId + (footIn ? " ใส่รองเท้าแล้ว" : " ถอดรองเท้า");
    Serial.println("[Shoe] " + logMsg);
    pushEventLog(getTimestampStr() + " — " + logMsg);

    // ส่งขึ้น server เฉพาะ event ที่มีความหมาย (ไม่ส่ง heartbeat ของรองเท้าขึ้น server ทุกครั้ง
    // เพื่อประหยัด bandwidth — heartbeat ใช้แค่ในวง local สำหรับเช็คว่ารองเท้ายังออนไลน์)
    String payload =
      "{"
        "\"event_type\":\"shoe_status\","
        "\"device_id\":\"" + deviceId + "\","
        "\"foot_in_shoe\":" + String(footIn ? "true" : "false") + ","
        "\"timestamp\":\"" + getTimestampStr() + "\""
      "}";
    postEventToServer(masterConfig, payload);
  }
  // ถ้าเป็น heartbeat: แค่อัปเดต lastSeenMs ไม่ต้อง log หรือส่งขึ้น server

  webServer.send(200, "application/json", "{\"status\":\"received\"}");
}

// ---------------------------------------------------------------------
// API ภายในให้ mini dashboard polling — ต้องอยู่ใน .ino หลักเพราะต้องใช้ masterConfig
// ---------------------------------------------------------------------
void handleApiStatus() {
  handleDashboardStatusApi(masterConfig);
}

// ---------------------------------------------------------------------
// อ่าน ultrasonic ทั้ง 3 ตัว ตัดสินใจแจ้งเตือน — ทำงาน local ไม่ต้องรอ server
// ---------------------------------------------------------------------
void pollUltrasonicZones() {
  for (int i = 0; i < 3; i++) {
    long distance = readDistanceCm(zones[i].trigPin, zones[i].echoPin);
    bool inAlertRange = (distance >= 0 && distance < ALERT_DISTANCE_CM);

    if (inAlertRange && !zones[i].wasTriggered) {
      // เพิ่งเข้าระยะอันตราย (rising edge) -- กัน cooldown ไม่ให้แจ้งเตือนถี่เกินไป
      if (millis() - zones[i].lastAlertAt > ALERT_COOLDOWN_MS) {
        Serial.println("[Zone] ⚠ " + String(zones[i].nameTh) + " — ระยะ " + String(distance) + " cm");

        alertZoneIntrusion(masterConfig.lineToken, masterConfig.lineUserId, zones[i].nameTh);

        String logMsg = String(zones[i].nameTh) + " — มีคนเดินเข้าใกล้ (" + String(distance) + " cm)";
        pushEventLog(getTimestampStr() + " — " + logMsg);

        String payload =
          "{"
            "\"event_type\":\"zone_intrusion\","
            "\"zone_id\":\"" + String(zones[i].nameSlug) + "\","
            "\"distance_cm\":" + String(distance) + ","
            "\"timestamp\":\"" + getTimestampStr() + "\""
          "}";
        postEventToServer(masterConfig, payload);

        zones[i].lastAlertAt = millis();
      }
    }

    zones[i].wasTriggered = inAlertRange;
  }
}

void connectStaWifi() {
  Serial.println("[WiFi-STA] กำลังเชื่อมต่อ: " + masterConfig.staSsid);
  WiFi.begin(masterConfig.staSsid.c_str(), masterConfig.staPassword.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi-STA] เชื่อมต่อสำเร็จ — IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi-STA] เชื่อมต่อไม่สำเร็จ (เน็ตบ้าน/มือถือ) — บอร์ดจะยังทำงาน local ได้ปกติ");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP8266 Master Booting ===");

  setupUltrasonicPins();
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  bool hasCustomConfig = loadMasterConfig(masterConfig);
  Serial.println("[Config] " + String(hasCustomConfig ? "ใช้ค่าที่ตั้งเองแล้ว" : "ใช้ค่า default"));
  Serial.println("[Config] cloud_mode: " + String(masterConfig.cloudMode ? "ON" : "OFF"));

  // --- เปิด AP+STA พร้อมกัน ---
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(masterConfig.apSsid.c_str(), masterConfig.apPassword.c_str());
  Serial.println("[WiFi-AP] ปล่อยสัญญาณ: " + masterConfig.apSsid + " — IP: " + WiFi.softAPIP().toString());

  connectStaWifi();

  // --- ซิงค์เวลา NTP (ใช้สำหรับ timestamp ใน log และ LINE) ---
  configTime(TZ_OFFSET_SEC, TZ_DST, "pool.ntp.org", "time.nist.gov");
  Serial.println("[NTP] กำลังซิงค์เวลา (ถ้า STA ไม่ติดเน็ตจะข้ามไปเรื่อยๆ ไม่ block การทำงานหลัก)");

  // --- ลงทะเบียน routes ทั้งหมดบน webServer ตัวเดียว ---
  registerDashboardRoutes();      // GET /
  webServer.on("/api/status", handleApiStatus);          // GET /api/status (dashboard polling)
  webServer.on("/api/shoe-status", HTTP_POST, handleShoeStatus); // POST จากรองเท้า ESP-01
  registerConfigPortalRoutes();   // GET/POST /config

  webServer.begin();
  Serial.println("[Server] Web server (dashboard + API + config) พร้อมที่พอร์ต 80");

  if (masterConfig.cloudMode) {
    postEventToServer(masterConfig,
      "{\"event_type\":\"system_online\",\"message\":\"ESP8266 master booted\"}");
  }

  Serial.println("=== Ready ===\n");
}

void loop() {
  webServer.handleClient();
  isConfigPortalWindowOpen(); // เช็คเงื่อนไขปิดพอร์ทัลอัตโนมัติหลัง 5 นาที (ผลข้างเคียงคือ log ตอนปิด)

  // --- เช็ค STA WiFi หลุดแล้วลองใหม่แบบไม่ block ---
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastStaRetryAt = 0;
    if (millis() - lastStaRetryAt > 15000) {
      lastStaRetryAt = millis();
      Serial.println("[WiFi-STA] หลุดการเชื่อมต่อ กำลังลองใหม่...");
      WiFi.begin(masterConfig.staSsid.c_str(), masterConfig.staPassword.c_str());
    }
  }

  // --- อ่าน ultrasonic ทุก ULTRASONIC_POLL_INTERVAL_MS ---
  if (millis() - lastUltrasonicPollAt > ULTRASONIC_POLL_INTERVAL_MS) {
    lastUltrasonicPollAt = millis();
    pollUltrasonicZones();
  }

  // --- ส่ง heartbeat ของมาสเตอร์เองขึ้น server เป็นระยะ ---
  if (millis() - lastMasterHeartbeatAt > MASTER_HEARTBEAT_INTERVAL_MS) {
    lastMasterHeartbeatAt = millis();
    postHeartbeatToServer(masterConfig);
  }
}
