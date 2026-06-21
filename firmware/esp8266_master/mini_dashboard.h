/*
  mini_dashboard.h — แดชบอร์ดในตัว ESP8266 มาสเตอร์

  เข้าถึงผ่าน WiFi วงเดียวกับ AP (ไม่ผ่านเน็ตจริง) — ใช้งานได้แม้เน็ตบ้าน/มือถือหลุด
  วิธีเข้า: ต่อ WiFi ชื่อ AP ของมาสเตอร์ แล้วเปิดเบราว์เซอร์ไป http://192.168.4.1/

  ออกแบบให้เบาที่สุด (เก็บ HTML ทั้งหมดใน PROGMEM, ไม่โหลด font/css จากเน็ตภายนอก
  เพราะวงนี้ไม่มีอินเทอร์เน็ตจริง) ใช้ JS เรียก endpoint /api/status ภายในบอร์ดเอง
  มา refresh หน้าทุก 3 วินาทีแบบ polling ธรรมดา (ไม่ใช้ WebSocket เพื่อความเรียบง่าย
  ของโมเดลจิ๋ว — ESP8266 รัน WebSocket ได้แต่เพิ่มความซับซ้อนโดยไม่จำเป็นในสเกลนี้)
*/

#ifndef MINI_DASHBOARD_H
#define MINI_DASHBOARD_H

#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "ultrasonic.h"

extern ESP8266WebServer webServer;

// ===== สถานะที่ dashboard ต้องโชว์ — อัปเดตจาก main .ino ทุกครั้งที่มีข้อมูลใหม่ =====
struct ShoeStatus {
  String deviceId;
  bool footIn;
  unsigned long lastSeenMs; // millis() ตอนได้รับข้อมูลล่าสุดจากรองเท้าตัวนี้
  bool everSeen;
};

ShoeStatus shoeLeft  = { "shoe01_left",  true, 0, false };
ShoeStatus shoeRight = { "shoe01_right", true, 0, false };

#define EVENT_LOG_SIZE 15
struct LogEntry {
  String text;
  unsigned long atMs;
};
LogEntry eventLogRing[EVENT_LOG_SIZE];
int eventLogHead = 0;
int eventLogCount = 0;

void pushEventLog(const String &text) {
  eventLogRing[eventLogHead] = { text, millis() };
  eventLogHead = (eventLogHead + 1) % EVENT_LOG_SIZE;
  if (eventLogCount < EVENT_LOG_SIZE) eventLogCount++;
}

const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="th"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Shoe — Mini Dashboard</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0;}
  body{font-family:-apple-system,sans-serif;background:#f4f1ea;color:#15201c;padding:1rem;}
  h1{font-size:16px;margin-bottom:.2rem;}
  p.sub{font-size:11.5px;color:#8b9590;margin-bottom:1rem;}
  .card{background:#fff;border:1px solid #dcd7c9;border-radius:8px;padding:.8rem 1rem;margin-bottom:.8rem;}
  .row{display:flex;justify-content:space-between;align-items:center;padding:.4rem 0;border-bottom:1px solid #e9e5d9;font-size:13px;}
  .row:last-child{border-bottom:none;}
  .badge{font-size:11px;font-weight:600;padding:.15rem .5rem;border-radius:10px;}
  .ok{background:#e3ede8;color:#1f4d3d;}
  .danger{background:#fbe6e0;color:#b5482f;}
  .offline{background:#eee;color:#888;}
  .log-item{font-size:12px;padding:.35rem 0;border-bottom:1px solid #e9e5d9;color:#4b5650;}
  .log-item:last-child{border-bottom:none;}
  h2{font-size:13px;margin-bottom:.5rem;color:#4b5650;}
</style></head>
<body>
  <h1>🩴 Smart Shoe Monitor — Mini Dashboard</h1>
  <p class="sub">อ่านข้อมูลจากมาสเตอร์โดยตรง (วง local ไม่ผ่านเน็ต) · <span id="cloudModeText"></span></p>

  <div class="card">
    <h2>สถานะรองเท้า</h2>
    <div id="shoeRows">กำลังโหลด...</div>
  </div>

  <div class="card">
    <h2>ระยะตรวจจับ ultrasonic (cm)</h2>
    <div id="zoneRows">กำลังโหลด...</div>
  </div>

  <div class="card">
    <h2>Event log ล่าสุด</h2>
    <div id="logRows">กำลังโหลด...</div>
  </div>

<script>
async function refresh(){
  try{
    const res = await fetch('/api/status');
    const data = await res.json();

    document.getElementById('cloudModeText').textContent =
      'cloud_mode: ' + (data.cloud_mode ? 'ON (ส่งขึ้น server)' : 'OFF (local-only)');

    document.getElementById('shoeRows').innerHTML = data.shoes.map(s => `
      <div class="row">
        <span>${s.device_id}</span>
        <span class="badge ${s.online ? (s.foot_in ? 'ok' : 'danger') : 'offline'}">
          ${s.online ? (s.foot_in ? 'ใส่รองเท้าอยู่' : 'ถอดรองเท้า') : 'ออฟไลน์'}
        </span>
      </div>`).join('');

    document.getElementById('zoneRows').innerHTML = data.zones.map(z => `
      <div class="row">
        <span>${z.name_th}</span>
        <span class="badge ${z.distance_cm >= 0 && z.distance_cm < data.alert_distance_cm ? 'danger' : 'ok'}">
          ${z.distance_cm >= 0 ? z.distance_cm + ' cm' : 'ไม่พบวัตถุ'}
        </span>
      </div>`).join('');

    document.getElementById('logRows').innerHTML = data.logs.length
      ? data.logs.map(l => `<div class="log-item">${l}</div>`).join('')
      : '<div class="log-item">ยังไม่มีเหตุการณ์</div>';

  }catch(e){
    console.error('โหลดข้อมูลไม่สำเร็จ', e);
  }
}
refresh();
setInterval(refresh, 3000);
</script>
</body></html>
)HTML";

void handleDashboardRoot() {
  webServer.send(200, "text/html; charset=utf-8", FPSTR(DASHBOARD_HTML));
}

// API ภายในบอร์ดเอง — ให้หน้า dashboard ดึงสถานะปัจจุบันมาโชว์ (polling ทุก 3 วิ)
void handleDashboardStatusApi(const MasterConfig &cfg) {
  DynamicJsonDocument doc(2048);

  doc["cloud_mode"] = cfg.cloudMode;
  doc["alert_distance_cm"] = ALERT_DISTANCE_CM;

  JsonArray shoesArr = doc.createNestedArray("shoes");
  ShoeStatus* shoeList[2] = { &shoeLeft, &shoeRight };
  for (int i = 0; i < 2; i++) {
    JsonObject s = shoesArr.createNestedObject();
    s["device_id"] = shoeList[i]->deviceId;
    s["foot_in"]   = shoeList[i]->footIn;
    // ถือว่าออฟไลน์ถ้าไม่เคยเห็นเลย หรือไม่ส่งสถานะมาเกิน 60 วิ (ควรได้ heartbeat ทุก 15 วิ)
    s["online"] = shoeList[i]->everSeen && (millis() - shoeList[i]->lastSeenMs < 60000);
  }

  JsonArray zonesArr = doc.createNestedArray("zones");
  for (int i = 0; i < 3; i++) {
    JsonObject z = zonesArr.createNestedObject();
    z["zone_id"]     = zones[i].nameSlug;
    z["name_th"]     = zones[i].nameTh;
    z["distance_cm"] = readDistanceCm(zones[i].trigPin, zones[i].echoPin);
  }

  JsonArray logsArr = doc.createNestedArray("logs");
  int idx = (eventLogHead - 1 + EVENT_LOG_SIZE) % EVENT_LOG_SIZE;
  for (int i = 0; i < eventLogCount; i++) {
    logsArr.add(eventLogRing[idx].text);
    idx = (idx - 1 + EVENT_LOG_SIZE) % EVENT_LOG_SIZE;
  }

  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
}

void registerDashboardRoutes() {
  webServer.on("/", handleDashboardRoot);
  // /api/status ลงทะเบียนใน .ino หลัก (ผ่านฟังก์ชัน handleApiStatus ที่เข้าถึง masterConfig
  // ในฐานะ global variable ได้ตรงๆ — ไม่ใช้ผ่านไฟล์นี้เพื่อเลี่ยงต้อง include storage.h ซ้ำที่นี่)
}

#endif
