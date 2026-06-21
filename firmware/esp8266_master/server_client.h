/*
  server_client.h — ส่งข้อมูลขึ้น FastAPI server (ตอนนี้ชี้ไปที่ mock server)

  รองรับ cloud_mode (ตามเอกสารสถาปัตยกรรม ชั้นที่ 4):
  - cloud_mode = true  -> ส่งไปยัง serverUrl ที่ตั้งค่าไว้ (FastAPI on Render หรือ mock server)
  - cloud_mode = false -> ไม่ส่งออกไปไหน เก็บไว้แสดงผลใน mini dashboard ในตัวเท่านั้น
    (โหมด local-only ตามที่ออกแบบไว้ — งานจริงปลายทางจะกลายเป็น local server บนมือถือ/Termux)

  ทุก event ที่เกิดขึ้นจะถูกส่งแบบ JSON ไปที่ /api/v1/events
  มาสเตอร์เองก็ส่ง heartbeat ของตัวเองไปที่ /api/v1/heartbeat เป็นระยะ
*/

#ifndef SERVER_CLIENT_H
#define SERVER_CLIENT_H

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include "config.h"
#include "storage.h"

// ส่ง event ใดๆ ขึ้น server (shoe status, zone intrusion, ฯลฯ)
// payloadJson คือ JSON string ที่สร้างไว้แล้วจากฝั่งที่เรียกใช้ฟังก์ชันนี้
bool postEventToServer(const MasterConfig &cfg, const String &payloadJson) {
  if (!cfg.cloudMode) {
    Serial.println("[Server] cloud_mode = OFF -> ไม่ส่งขึ้น cloud (local-only)");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Server] STA WiFi ไม่ได้เชื่อมต่อ (เน็ตบ้าน/มือถือหลุด) ข้ามรอบนี้");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = cfg.serverUrl + SERVER_EVENT_PATH;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payloadJson);

  if (httpCode > 0) {
    Serial.println("[Server] POST " + url + " -> HTTP " + String(httpCode));
  } else {
    Serial.println("[Server] ส่งล้มเหลว: " + http.errorToString(httpCode));
  }

  http.end();
  return httpCode == 200 || httpCode == 201;
}

bool postHeartbeatToServer(const MasterConfig &cfg) {
  if (!cfg.cloudMode) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  HTTPClient http;

  String url = cfg.serverUrl + SERVER_HEARTBEAT_PATH;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  String payload =
    "{"
      "\"device_id\":\"master_01\","
      "\"uptime_ms\":" + String(millis()) + ","
      "\"free_heap\":" + String(ESP.getFreeHeap()) +
    "}";

  int httpCode = http.POST(payload);
  http.end();
  return httpCode == 200 || httpCode == 201;
}

#endif
