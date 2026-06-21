/*
  config_portal.h — WiFi Config Portal ของ ESP-01

  ตามที่ตกลงในเอกสารสถาปัตยกรรม:
  - เปิดให้ตั้งค่าได้ 5 นาทีหลังบูต แล้วปิดอัตโนมัติ (ไม่สนใจว่ามีคนตั้งค่าอยู่หรือไม่)
  - ตั้งค่าได้: Device ID, WiFi SSID/Password ของ ESP8266 มาสเตอร์, Token แจ้งเตือน (เผื่ออนาคต)

  วิธีใช้งานจริง:
  1. เปิด ESP-01 ครั้งแรก (หรือกดรีเซ็ตภายใน 5 นาทีหลังบูตค้างไว้)
  2. มือถือ/คอมไปต่อ WiFi ชื่อ "ShoeConfig-XXXX" (ไม่ต้องใส่ password)
  3. เปิดเบราว์เซอร์ไป http://192.168.4.1
  4. กรอกฟอร์ม กดบันทึก -> บอร์ดรีสตาร์ทแล้วใช้ค่าใหม่ทันที
*/

#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "storage.h"

ESP8266WebServer configServer(80);
bool portalActive = false;
unsigned long portalStartedAt = 0;

const char PORTAL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="th"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ตั้งค่ารองเท้า</title>
<style>
  body{font-family:sans-serif;background:#f4f1ea;color:#15201c;padding:1.5rem;max-width:420px;margin:0 auto;}
  h1{font-size:18px;margin-bottom:.3rem;}
  p.sub{color:#4b5650;font-size:13px;margin-bottom:1.2rem;}
  label{display:block;font-size:13px;font-weight:600;margin:.9rem 0 .3rem;}
  input{width:100%;padding:.6rem;border:1px solid #dcd7c9;border-radius:6px;font-size:14px;box-sizing:border-box;}
  button{margin-top:1.3rem;width:100%;padding:.7rem;background:#1f4d3d;color:#fff;border:none;border-radius:6px;font-size:15px;font-weight:600;}
  .note{font-size:11.5px;color:#8b9590;margin-top:.4rem;}
</style></head>
<body>
  <h1>ตั้งค่า ESP-01 (รองเท้า)</h1>
  <p class="sub">ตั้งค่าได้ภายใน 5 นาทีหลังบูต หลังจากนั้นพอร์ทัลจะปิดอัตโนมัติ</p>
  <form action="/save" method="POST">
    <label>Device ID</label>
    <input type="text" name="device_id" value="%DEVICE_ID%" placeholder="shoe01_left หรือ shoe01_right">
    <div class="note">ต้องไม่ซ้ำกันระหว่างรองเท้าซ้าย/ขวา</div>

    <label>WiFi SSID ของ ESP8266 มาสเตอร์</label>
    <input type="text" name="ap_ssid" value="%AP_SSID%">

    <label>WiFi Password ของ ESP8266 มาสเตอร์</label>
    <input type="text" name="ap_password" value="%AP_PASSWORD%">

    <label>Token แจ้งเตือน (เผื่อใช้ในอนาคต — เว้นว่างได้)</label>
    <input type="text" name="notify_token" value="%NOTIFY_TOKEN%">

    <button type="submit">บันทึกและรีสตาร์ท</button>
  </form>
</body></html>
)HTML";

const char SAVED_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="th"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>บันทึกแล้ว</title>
<style>body{font-family:sans-serif;background:#f4f1ea;color:#15201c;padding:2rem;text-align:center;}</style>
</head><body><h2>บันทึกสำเร็จ ✅</h2><p>อุปกรณ์กำลังรีสตาร์ท...</p></body></html>
)HTML";

void handlePortalRoot() {
  DeviceConfig cfg;
  loadConfig(cfg);

  String page = FPSTR(PORTAL_HTML);
  page.replace("%DEVICE_ID%", cfg.deviceId);
  page.replace("%AP_SSID%", cfg.apSsid);
  page.replace("%AP_PASSWORD%", cfg.apPassword);
  page.replace("%NOTIFY_TOKEN%", cfg.notifyToken);

  configServer.send(200, "text/html; charset=utf-8", page);
}

void handlePortalSave() {
  DeviceConfig cfg;
  cfg.deviceId    = configServer.arg("device_id");
  cfg.apSsid      = configServer.arg("ap_ssid");
  cfg.apPassword  = configServer.arg("ap_password");
  cfg.notifyToken = configServer.arg("notify_token");

  saveConfig(cfg);

  configServer.send(200, "text/html; charset=utf-8", FPSTR(SAVED_HTML));
  delay(1500);
  ESP.restart();
}

void startConfigPortal() {
  String apName = "ShoeConfig-" + String(ESP.getChipId(), HEX);
  WiFi.mode(WIFI_AP_STA); // เปิด AP ตั้งค่าได้ พร้อมกับยังพยายามต่อ WiFi เดิมไปด้วย
  WiFi.softAP(apName.c_str()); // ไม่ตั้ง password เพื่อให้เข้าตั้งค่าง่ายตอนหน้างาน

  configServer.on("/", handlePortalRoot);
  configServer.on("/save", HTTP_POST, handlePortalSave);
  configServer.begin();

  portalActive = true;
  portalStartedAt = millis();

  Serial.println("[Portal] เปิดแล้ว: ต่อ WiFi ชื่อ \"" + apName + "\" แล้วเข้า http://192.168.4.1");
  Serial.println("[Portal] จะปิดอัตโนมัติใน 5 นาที");
}

// เรียกใน loop() ทุกรอบ — คืนค่า true ถ้าพอร์ทัลยัง active อยู่ (ให้ skip logic อื่น)
bool handleConfigPortal() {
  if (!portalActive) return false;

  configServer.handleClient();

  if (millis() - portalStartedAt > CONFIG_PORTAL_TIMEOUT_MS) {
    Serial.println("[Portal] ครบ 5 นาที ปิดพอร์ทัลอัตโนมัติ");
    configServer.stop();
    WiFi.softAPdisconnect(true);
    portalActive = false;
    return false;
  }
  return true;
}

#endif
