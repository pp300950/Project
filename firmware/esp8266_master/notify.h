/*
  notify.h — แจ้งเตือนผ่าน LINE Messaging API + สั่งบัซเซอร์

  ย้ายมาจากโค้ดต้นแบบ main2.ino (เดิมอยู่บน ESP8266 ตัวเดียวที่ทำทุกอย่าง)
  ตอนนี้แยกเป็นไฟล์ของตัวเองในมาสเตอร์ เพราะ ESP-01 ไม่ทำหน้าที่แจ้งเตือนแล้ว
  (ESP-01 ส่งแค่สถานะดิบ ส่วนการตัดสินใจแจ้งเตือน/บัซเซอร์ทั้งหมดอยู่ที่มาสเตอร์)
*/

#ifndef NOTIFY_H
#define NOTIFY_H

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "config.h"

String getTimestampStr() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  if (now < 86400) {
    return "ไม่สามารถดึงเวลาได้";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", timeinfo);
  return String(buffer);
}

String jsonEscapeStr(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

// ส่งข้อความผ่าน LINE Messaging API (push message)
// คืนค่า true ถ้าได้ HTTP response กลับมา (ไม่ได้แปลว่าสำเร็จ 100% ต้องดู code ด้วย)
bool sendLineMessage(const String &lineToken, const String &lineUserId, const String &message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LINE] STA WiFi ไม่ได้เชื่อมต่อ ข้ามการแจ้งเตือนรอบนี้");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // โมเดลจิ๋ว: ไม่ตรวจ cert ของ LINE — งานจริงควร pin certificate
  HTTPClient http;

  http.begin(client, "https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + lineToken);

  String jsonPayload =
    "{"
      "\"to\":\"" + lineUserId + "\","
      "\"messages\":["
        "{"
          "\"type\":\"text\","
          "\"text\":\"" + jsonEscapeStr(message) + "\""
        "}"
      "]"
    "}";

  int code = http.POST(jsonPayload);
  String response = http.getString();

  Serial.println("[LINE] HTTP Code: " + String(code));
  if (code != 200) {
    Serial.println("[LINE] Response: " + response);
  }

  http.end();
  return code > 0;
}

void triggerBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(BUZZER_DURATION_MS);
  digitalWrite(BUZZER_PIN, LOW);
}

// แจ้งเตือนเข้าใกล้จุดอันตราย — ดังบัซเซอร์ทันที (local, ไม่รอ LINE ตอบ) แล้วค่อยส่ง LINE
void alertZoneIntrusion(const String &lineToken, const String &lineUserId, const String &zoneNameTh) {
  triggerBuzzer(); // ดังก่อนเลย ไม่รอ network — งานจริงต้องเร็วที่สุด

  String msg = "🚨 [ตรวจสอบคนไข้!]\n";
  msg += "📌 พิกัด: " + zoneNameTh + "\n";
  msg += "⏰ เวลา: " + getTimestampStr();

  sendLineMessage(lineToken, lineUserId, msg);
}

#endif
