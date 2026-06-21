/*
  config_portal.h — WiFi Config Portal ของ ESP8266 มาสเตอร์

  ตั้งค่าได้มากกว่า ESP-01 เพราะมาสเตอร์ต้องรู้จักทั้ง 2 วง WiFi (AP+STA)
  รวมถึง server endpoint, cloud_mode, และ LINE token

  เปิดให้ตั้งค่าได้ 5 นาทีหลังบูต เหมือน ESP-01 (ใช้พอร์ตเดียวกับ mini dashboard
  คือพอร์ต 80 — เข้าทางเดียวกัน แต่คนละ path)
*/

#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <ESP8266WebServer.h>
#include "storage.h"

extern ESP8266WebServer webServer; // ประกาศจริงอยู่ใน esp8266_master.ino (ใช้ตัวเดียวกันกับ dashboard)

bool masterPortalActive = false;
unsigned long masterPortalStartedAt = 0;

const char MASTER_PORTAL_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="th"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ตั้งค่ามาสเตอร์</title>
<style>
  body{font-family:sans-serif;background:#f4f1ea;color:#15201c;padding:1.5rem;max-width:460px;margin:0 auto;}
  h1{font-size:18px;margin-bottom:.2rem;}
  h3{font-size:13px;text-transform:uppercase;letter-spacing:.05em;color:#8b9590;margin:1.3rem 0 .4rem;border-top:1px solid #dcd7c9;padding-top:1rem;}
  p.sub{color:#4b5650;font-size:13px;margin-bottom:1rem;}
  label{display:block;font-size:13px;font-weight:600;margin:.7rem 0 .3rem;}
  input[type=text],input[type=password]{width:100%;padding:.6rem;border:1px solid #dcd7c9;border-radius:6px;font-size:14px;box-sizing:border-box;}
  .checkbox-row{display:flex;align-items:center;gap:.5rem;margin-top:.8rem;}
  button{margin-top:1.5rem;width:100%;padding:.7rem;background:#1f4d3d;color:#fff;border:none;border-radius:6px;font-size:15px;font-weight:600;}
  .note{font-size:11.5px;color:#8b9590;margin-top:.3rem;}
</style></head>
<body>
  <h1>ตั้งค่า ESP8266 มาสเตอร์</h1>
  <p class="sub">ตั้งค่าได้ภายใน 5 นาทีหลังบูต</p>
  <form action="/config/save" method="POST">

    <h3>WiFi ที่ปล่อยให้รองเท้าต่อ (AP)</h3>
    <label>AP SSID</label>
    <input type="text" name="ap_ssid" value="%AP_SSID%">
    <label>AP Password</label>
    <input type="text" name="ap_password" value="%AP_PASSWORD%">

    <h3>WiFi บ้าน/มือถือ ที่ใช้ขึ้นเน็ต (STA)</h3>
    <label>STA SSID</label>
    <input type="text" name="sta_ssid" value="%STA_SSID%">
    <label>STA Password</label>
    <input type="text" name="sta_password" value="%STA_PASSWORD%">

    <h3>Server ปลายทาง</h3>
    <label>Server URL (FastAPI)</label>
    <input type="text" name="server_url" value="%SERVER_URL%" placeholder="https://your-app.onrender.com">
    <div class="checkbox-row">
      <input type="checkbox" name="cloud_mode" id="cloud_mode" %CLOUD_MODE_CHECKED%>
      <label for="cloud_mode" style="margin:0;">เปิด cloud_mode (ส่งข้อมูลขึ้น server)</label>
    </div>
    <div class="note">ถ้าปิด = local-only ไม่ส่งข้อมูลออกไปไหน ดูได้แค่ใน mini dashboard นี้</div>

    <h3>LINE Notify</h3>
    <label>LINE Channel Access Token</label>
    <input type="text" name="line_token" value="%LINE_TOKEN%">
    <label>LINE User ID</label>
    <input type="text" name="line_user_id" value="%LINE_USER_ID%">

    <button type="submit">บันทึกและรีสตาร์ท</button>
  </form>
</body></html>
)HTML";

const char MASTER_SAVED_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="th"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>บันทึกแล้ว</title>
<style>body{font-family:sans-serif;background:#f4f1ea;color:#15201c;padding:2rem;text-align:center;}</style>
</head><body><h2>บันทึกสำเร็จ ✅</h2><p>อุปกรณ์กำลังรีสตาร์ท...</p></body></html>
)HTML";

void handleMasterConfigRoot() {
  MasterConfig cfg;
  loadMasterConfig(cfg);

  String page = FPSTR(MASTER_PORTAL_HTML);
  page.replace("%AP_SSID%", cfg.apSsid);
  page.replace("%AP_PASSWORD%", cfg.apPassword);
  page.replace("%STA_SSID%", cfg.staSsid);
  page.replace("%STA_PASSWORD%", cfg.staPassword);
  page.replace("%SERVER_URL%", cfg.serverUrl);
  page.replace("%CLOUD_MODE_CHECKED%", cfg.cloudMode ? "checked" : "");
  page.replace("%LINE_TOKEN%", cfg.lineToken);
  page.replace("%LINE_USER_ID%", cfg.lineUserId);

  webServer.send(200, "text/html; charset=utf-8", page);
}

void handleMasterConfigSave() {
  MasterConfig cfg;
  cfg.apSsid      = webServer.arg("ap_ssid");
  cfg.apPassword  = webServer.arg("ap_password");
  cfg.staSsid     = webServer.arg("sta_ssid");
  cfg.staPassword = webServer.arg("sta_password");
  cfg.serverUrl   = webServer.arg("server_url");
  cfg.cloudMode   = webServer.hasArg("cloud_mode"); // checkbox ส่งมาเฉพาะตอนติ๊ก
  cfg.lineToken   = webServer.arg("line_token");
  cfg.lineUserId  = webServer.arg("line_user_id");

  saveMasterConfig(cfg);

  webServer.send(200, "text/html; charset=utf-8", FPSTR(MASTER_SAVED_HTML));
  delay(1500);
  ESP.restart();
}

void registerConfigPortalRoutes() {
  webServer.on("/config", handleMasterConfigRoot);
  webServer.on("/config/save", HTTP_POST, handleMasterConfigSave);

  masterPortalActive = true;
  masterPortalStartedAt = millis();
  Serial.println("[Portal] เข้าตั้งค่าได้ที่ http://<IP ของมาสเตอร์>/config (ใช้ได้ 5 นาทีหลังบูต)");
}

// คืนค่า true ถ้ายังอยู่ในช่วง 5 นาทีแรก (ใช้เช็คว่าควร "เปิดไฟ" route /config อยู่หรือไม่)
// หมายเหตุ: ไม่ปิด webServer ทั้งตัวเพราะ mini dashboard ต้องใช้พอร์ตเดียวกันตลอดเวลา
// แค่ปิดเฉพาะสิทธิ์เข้าถึง /config หลังพ้น 5 นาที
bool isConfigPortalWindowOpen() {
  if (!masterPortalActive) return false;
  if (millis() - masterPortalStartedAt > CONFIG_PORTAL_TIMEOUT_MS) {
    if (masterPortalActive) {
      Serial.println("[Portal] ครบ 5 นาที ปิดสิทธิ์เข้าถึง /config แล้ว (mini dashboard ยังใช้งานได้ตามปกติ)");
      masterPortalActive = false;
    }
    return false;
  }
  return true;
}

#endif
