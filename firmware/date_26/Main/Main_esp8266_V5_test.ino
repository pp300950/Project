/*
  Smart Monitor — ESP8266 Firmware v2.1 (FIXED)
  ─────────────────────────────────────
  แก้บั๊กจาก v2.0:
    1) \u{1F550} และ \u{1F7E2} เป็น escape syntax ของ JavaScript/Swift
       ไม่ใช่ C++ → ทำให้ JSON payload ที่ส่งไป LINE เพี้ยน
       → LINE API ปฏิเสธ (HTTP 400) → การ์ดไม่ขึ้นในมือถือเลย
       แก้โดยใส่ emoji ตรงๆในสตริง (UTF-8 literal) แทน
    2) lambda (auto distRow = [](...){...}) ใน sendBootCard()
       ความเสี่ยงเรื่อง compile/capture บน ESP8266 core บางเวอร์ชัน
       → แยกออกมาเป็นฟังก์ชันธรรมดา buildDistRow()
    3) เพิ่ม Serial debug log HTTP response ของ LINE Flex ให้ชัดเจนขึ้น
       เพื่อ debug ง่ายขึ้นในอนาคตถ้ามีปัญหาอีก

  คงไว้ทั้งหมดเหมือนเดิม:
    • ตรรกะเซนเซอร์ (DETECT_CONFIRM, COOLDOWN_MS, pinout ทุกพิน)
    • alertFence() / alertPool() / alertSensor3() + ส่งคำสั่งไป Arduino ผ่าน arSerial
    • buzzer non-blocking
    • LINE Flex Message (การ์ดสวยงาม)
    • WiFi AP_STA โหมด (AP="ABC" + STA="PP")
    • captive portal / DNS redirect
    • Dashboard + Settings page
    • POST event ไป Render server + offline queue
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <time.h>
#include <ArduinoJson.h>

// ─── WiFi AP (Web Dashboard) ──────────────
const char* AP_SSID = "ABC";
const char* AP_PASS = "12345678";

// ─── WiFi Station (LINE + Server) ────────
const char* STA_SSID = "PP";
const char* STA_PASS = "12345678";

// ─── LINE ────────────────────────────────
#define LINE_TOKEN "+WCQuf3X+jrVPNhe61leiOB9TSgimmcEtn2YeK2q5DUV8amp4mCVY8kNzNIfw5fQVvc5dwR4JwzhcogU1WDo6SresUC5D4IZYqWpGtODwWbUnwjPSaWj3k0fUoU/Bwus6FiyxKYib5zfAIWHu5jVygdB04t89/1O/w1cDnyilFU="
#define USER_ID    "Uf72325ea6cc9aee84558f42e01a07e33"

// ─── Render Server ────────────────────────
const char* SERVER_HOST = "smart-shoe-monitor.onrender.com";
const char* SERVER_PATH = "/api/event";
const char* DEVICE_ID   = "esp8266_home_01";

// ─── Server / DNS ─────────────────────────
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer server(80);

// ─── Serial to Arduino ────────────────────
SoftwareSerial arSerial(-1, D4);  // TX=D4

// ─── Sensor 1: รั้วบ้าน ───────────────────
#define TRIG1_PIN        D1
#define ECHO1_PIN        D2
#define DETECT1_DIST_CM  16
#define PULSIN1_TIMEOUT  4000

// ─── Sensor 2: สระว่ายน้ำ ─────────────────
#define TRIG2_PIN        D6
#define ECHO2_PIN        D7
#define DETECT2_DIST_CM  20
#define PULSIN2_TIMEOUT  4500

// ─── Sensor 3: เซนเซอร์เสริม ─────────────
#define TRIG3_PIN        D3
#define ECHO3_PIN        D0
#define DETECT3_DIST_CM  20
#define PULSIN3_TIMEOUT  4500

// ─── Buzzer ───────────────────────────────
#define BUZZER_PIN         D5
#define BUZZER_DURATION_MS 2000

// ─── Cooldown / Confirm ───────────────────
#define COOLDOWN_MS    30000
#define DETECT_CONFIRM 3

unsigned long lastAlert1 = 0;
unsigned long lastAlert2 = 0;
unsigned long lastAlert3 = 0;
int detectCount1 = 0;
int detectCount2 = 0;
int detectCount3 = 0;
bool triggered1  = false;
bool triggered2  = false;
bool triggered3  = false;

// ─── Buzzer non-blocking ──────────────────
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;

// ─── EEPROM Layout ────────────────────────
#define EEPROM_SIZE   96
#define ZONE1_ADDR     0
#define ZONE2_ADDR    32
#define ZONE3_ADDR    64
#define ZONE_NAME_LEN 32

char zone1Name[ZONE_NAME_LEN] = "รั้วบ้าน";
char zone2Name[ZONE_NAME_LEN] = "สระว่ายน้ำ";
char zone3Name[ZONE_NAME_LEN] = "เซนเซอร์ 3";

// ─── Alert stats (วันนี้) ─────────────────
struct ZoneStat {
  int count;
  unsigned long lastAlertMs;
  String lastAlertTime;
};
ZoneStat stat1 = {0, 0, "-"};
ZoneStat stat2 = {0, 0, "-"};
ZoneStat stat3 = {0, 0, "-"};

// ─── Offline queue ────────────────────────
#define QUEUE_MAX 10
struct PendingEvent {
  String zone;
  float  dist;
  String ts;
  bool   used;
};
PendingEvent pendingQueue[QUEUE_MAX];
int queueCount = 0;

// ─── Server sync state ────────────────────
bool lastSyncOK = false;
String lastSyncMsg = "ยังไม่เคย sync";

// ─── WiFi reconnect timer ─────────────────
unsigned long lastWifiCheck = 0;
#define WIFI_CHECK_INTERVAL 30000

// ═══════════════════════════════════════════
// EEPROM helpers
// ═══════════════════════════════════════════

void loadZoneNames() {
  EEPROM.begin(EEPROM_SIZE);
  if ((byte)EEPROM.read(ZONE1_ADDR) != 0xFF && (byte)EEPROM.read(ZONE1_ADDR) != 0) {
    for (int i = 0; i < ZONE_NAME_LEN; i++) zone1Name[i] = (char)EEPROM.read(ZONE1_ADDR + i);
    for (int i = 0; i < ZONE_NAME_LEN; i++) zone2Name[i] = (char)EEPROM.read(ZONE2_ADDR + i);
    for (int i = 0; i < ZONE_NAME_LEN; i++) zone3Name[i] = (char)EEPROM.read(ZONE3_ADDR + i);
    zone1Name[ZONE_NAME_LEN-1] = '\0';
    zone2Name[ZONE_NAME_LEN-1] = '\0';
    zone3Name[ZONE_NAME_LEN-1] = '\0';
    Serial.println("[EEPROM] โหลดชื่อโซนจาก EEPROM");
  } else {
    Serial.println("[EEPROM] ใช้ชื่อ default");
  }
}

void saveZoneName(int addr, const char* name) {
  EEPROM.begin(EEPROM_SIZE);
  int len = strlen(name);
  if (len >= ZONE_NAME_LEN) len = ZONE_NAME_LEN - 1;
  for (int i = 0; i < len; i++) EEPROM.write(addr + i, name[i]);
  for (int i = len; i < ZONE_NAME_LEN; i++) EEPROM.write(addr + i, 0);
  EEPROM.commit();
}

// ═══════════════════════════════════════════
// ฟังก์ชันช่วย
// ═══════════════════════════════════════════

String getTimestamp() {
  time_t now = time(nullptr);
  if (now < 86400) return "--:--:--";
  struct tm* t = localtime(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", t);
  return String(buf);
}

String getTimeOnly() {
  time_t now = time(nullptr);
  if (now < 86400) return "--:--";
  struct tm* t = localtime(&now);
  char buf[10];
  strftime(buf, sizeof(buf), "%H:%M:%S", t);
  return String(buf);
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

// ═══════════════════════════════════════════
// LINE Flex Message
// ═══════════════════════════════════════════

void sendLineFlex(String altText, String jsonContents) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LINE] WiFi ไม่ได้เชื่อมต่อ — ข้าม");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(LINE_TOKEN));
  http.setTimeout(10000);
  String payload =
    "{\"to\":\"" + String(USER_ID) + "\","
    "\"messages\":[{\"type\":\"flex\","
    "\"altText\":\"" + jsonEscape(altText) + "\","
    "\"contents\":" + jsonContents + "}]}";

  int code = http.POST(payload);

  // ─── DEBUG: แสดง HTTP code + รายละเอียด error ถ้าพลาด ───
  Serial.println("[LINE-FLEX] HTTP " + String(code));
  if (code != 200) {
    String resp = http.getString();
    Serial.println("[LINE-FLEX] Response: " + resp);
  }
  http.end();
}

// ── การ์ดแจ้งเตือนอันตราย ─────────────────
// FIX: เปลี่ยน \u{1F550} (นาฬิกา) ให้เป็น emoji ตรงๆในสตริง (UTF-8)
//      \u{...} เป็น syntax ของ JS/Swift ไม่ใช่ C++ ทำให้ JSON ที่ส่งออกไปเพี้ยน
void sendAlertCard(String zone, float dist, String ts,
                   String headerColor, String icon) {
  String distStr = dist > 0 ? String(dist, 1) + " cm" : "ไม่ทราบ";
  String flex =
    "{"
      "\"type\":\"bubble\","
      "\"size\":\"kilo\","
      "\"header\":{"
        "\"type\":\"box\","
        "\"layout\":\"horizontal\","
        "\"backgroundColor\":\"" + headerColor + "\","
        "\"paddingAll\":\"16px\","
        "\"contents\":["
          "{\"type\":\"text\","
          "\"text\":\"" + icon + " แจ้งเตือนด่วน!\","
          "\"color\":\"#FFFFFF\","
          "\"weight\":\"bold\","
          "\"size\":\"lg\","
          "\"flex\":1}"
        "]"
      "},"
      "\"body\":{"
        "\"type\":\"box\","
        "\"layout\":\"vertical\","
        "\"spacing\":\"md\","
        "\"paddingAll\":\"16px\","
        "\"contents\":["
          "{\"type\":\"box\","
          "\"layout\":\"horizontal\","
          "\"contents\":["
            "{\"type\":\"text\",\"text\":\"โซน\","
            "\"color\":\"#888888\",\"size\":\"sm\",\"flex\":2},"
            "{\"type\":\"text\","
            "\"text\":\"" + jsonEscape(zone) + "\","
            "\"weight\":\"bold\",\"size\":\"sm\",\"flex\":5,"
            "\"color\":\"#222222\"}"
          "]},"
          "{\"type\":\"box\","
          "\"layout\":\"horizontal\","
          "\"contents\":["
            "{\"type\":\"text\",\"text\":\"ระยะ\","
            "\"color\":\"#888888\",\"size\":\"sm\",\"flex\":2},"
            "{\"type\":\"text\","
            "\"text\":\"" + distStr + "\","
            "\"weight\":\"bold\",\"size\":\"sm\",\"flex\":5,"
            "\"color\":\"" + headerColor + "\"}"
          "]},"
          "{\"type\":\"separator\"},"
          "{\"type\":\"box\","
          "\"layout\":\"horizontal\","
          "\"contents\":["
            "{\"type\":\"text\",\"text\":\"🕐\","
            "\"size\":\"sm\",\"flex\":1},"
            "{\"type\":\"text\","
            "\"text\":\"" + jsonEscape(ts) + "\","
            "\"color\":\"#888888\",\"size\":\"sm\",\"flex\":6}"
          "]}"
        "]"
      "},"
      "\"footer\":{"
        "\"type\":\"box\","
        "\"layout\":\"vertical\","
        "\"contents\":["
          "{\"type\":\"button\","
          "\"style\":\"primary\","
          "\"color\":\"" + headerColor + "\","
          "\"height\":\"sm\","
          "\"action\":{"
            "\"type\":\"uri\","
            "\"label\":\"ดู Dashboard\","
            "\"uri\":\"http://192.168.4.1\""
          "}}"
        "]"
      "}"
    "}";
  sendLineFlex(icon + " แจ้งเตือน " + zone, flex);
}

// ── ฟังก์ชันสร้าง 1 แถวเซนเซอร์ใน boot card ──────────
// FIX: เดิมเป็น lambda (auto distRow = [](...){...}) ในตัว sendBootCard()
//      ย้ายออกมาเป็นฟังก์ชันธรรมดาแยกต่างหาก เพื่อลดความเสี่ยง
//      เรื่อง compile/capture บน ESP8266 core บางเวอร์ชัน
String buildDistRow(String label, float d, int threshold) {
  String val   = d > 0 ? String(d, 1) + " cm" : "ไม่มีสัญญาณ";
  String color = (d > 0 && d < threshold) ? "#e02020" : "#27ae60";
  return
    "{\"type\":\"box\","
    "\"layout\":\"horizontal\","
    "\"spacing\":\"sm\","
    "\"contents\":["
      "{\"type\":\"text\","
      "\"text\":\"" + label + "\","
      "\"color\":\"#555555\",\"size\":\"sm\",\"flex\":4},"
      "{\"type\":\"text\","
      "\"text\":\"" + val + "\","
      "\"weight\":\"bold\",\"size\":\"sm\","
      "\"flex\":3,\"color\":\"" + color + "\",\"align\":\"end\"},"
      "{\"type\":\"text\","
      "\"text\":\"< " + String(threshold) + " cm\","
      "\"color\":\"#aaaaaa\",\"size\":\"xs\","
      "\"flex\":3,\"align\":\"end\"}"
    "]}";
}

// ── การ์ด boot พร้อมทำงาน ─────────────────
// FIX: เปลี่ยน \u{1F7E2} (จุดเขียว) เป็น emoji ตรงๆในสตริง
void sendBootCard(float b1, float b2, float b3) {
  String flex =
    "{"
      "\"type\":\"bubble\","
      "\"size\":\"kilo\","
      "\"header\":{"
        "\"type\":\"box\","
        "\"layout\":\"vertical\","
        "\"backgroundColor\":\"#27ae60\","
        "\"paddingAll\":\"16px\","
        "\"contents\":["
          "{\"type\":\"text\","
          "\"text\":\"🟢 SmartGuard Online\","
          "\"color\":\"#FFFFFF\",\"weight\":\"bold\",\"size\":\"lg\"},"
          "{\"type\":\"text\","
          "\"text\":\"" + jsonEscape(getTimestamp()) + "\","
          "\"color\":\"#ccffcc\",\"size\":\"xs\",\"margin\":\"sm\"}"
        "]"
      "},"
      "\"body\":{"
        "\"type\":\"box\","
        "\"layout\":\"vertical\","
        "\"spacing\":\"md\","
        "\"paddingAll\":\"16px\","
        "\"contents\":["
          "{\"type\":\"text\","
          "\"text\":\"ค่าเซนเซอร์ปัจจุบัน\","
          "\"weight\":\"bold\",\"size\":\"sm\","
          "\"color\":\"#333333\",\"margin\":\"none\"},"
          "{\"type\":\"separator\",\"margin\":\"sm\"}," +
          buildDistRow(String(zone1Name), b1, DETECT1_DIST_CM) + "," +
          buildDistRow(String(zone2Name), b2, DETECT2_DIST_CM) + "," +
          buildDistRow(String(zone3Name), b3, DETECT3_DIST_CM) +
        "]"
      "},"
      "\"footer\":{"
        "\"type\":\"box\","
        "\"layout\":\"vertical\","
        "\"contents\":["
          "{\"type\":\"button\","
          "\"style\":\"primary\","
          "\"color\":\"#27ae60\","
          "\"height\":\"sm\","
          "\"action\":{"
            "\"type\":\"uri\","
            "\"label\":\"เปิด Dashboard\","
            "\"uri\":\"http://192.168.4.1\""
          "}}"
        "]"
      "}"
    "}";
  sendLineFlex("SmartGuard พร้อมทำงานแล้ว", flex);
}

// ═══════════════════════════════════════════
// Server POST (Render)
// ═══════════════════════════════════════════

bool postEventToServer(String zone, float dist, String ts) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://" + String(SERVER_HOST) + String(SERVER_PATH);
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);
  String body = "{";
  body += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  body += "\"event_type\":\"zone_breach\",";
  body += "\"zone\":\"" + jsonEscape(zone) + "\",";
  body += "\"distance_cm\":" + String(dist, 1) + ",";
  body += "\"raw_json\":{\"ts_local\":\"" + jsonEscape(ts) + "\"}";
  body += "}";
  int code = http.POST(body);
  http.end();
  Serial.printf("[SERVER] POST %s → HTTP %d\n", zone.c_str(), code);
  return (code == 200 || code == 201);
}

void enqueueEvent(String zone, float dist, String ts) {
  if (queueCount >= QUEUE_MAX) {
    for (int i = 0; i < QUEUE_MAX - 1; i++) pendingQueue[i] = pendingQueue[i+1];
    queueCount = QUEUE_MAX - 1;
  }
  pendingQueue[queueCount].zone = zone;
  pendingQueue[queueCount].dist = dist;
  pendingQueue[queueCount].ts   = ts;
  pendingQueue[queueCount].used = true;
  queueCount++;
  Serial.printf("[QUEUE] เพิ่ม %s queue=%d\n", zone.c_str(), queueCount);
}

int flushQueue() {
  if (queueCount == 0) return 0;
  if (WiFi.status() != WL_CONNECTED) return 0;
  int sent = 0;
  for (int i = 0; i < queueCount; i++) {
    if (postEventToServer(pendingQueue[i].zone, pendingQueue[i].dist, pendingQueue[i].ts)) {
      sent++;
      pendingQueue[i].used = false;
    }
  }
  int newCount = 0;
  for (int i = 0; i < queueCount; i++) {
    if (pendingQueue[i].used) pendingQueue[newCount++] = pendingQueue[i];
  }
  queueCount = newCount;
  return sent;
}

void reportEvent(String zone, float dist) {
  String ts = getTimestamp();
  if (!postEventToServer(zone, dist, ts)) {
    enqueueEvent(zone, dist, ts);
    lastSyncOK  = false;
    lastSyncMsg = "ส่งไม่สำเร็จ queue=" + String(queueCount);
  } else {
    lastSyncOK  = true;
    lastSyncMsg = "ส่งสำเร็จ " + getTimeOnly();
    if (queueCount > 0) flushQueue();
  }
}

// ═══════════════════════════════════════════
// Sensor
// ═══════════════════════════════════════════

float readDistanceCM(int trigPin, int echoPin, unsigned long timeout_us) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, timeout_us);
  if (duration == 0) return -1.0;
  return duration * 0.034 / 2.0;
}

void startBuzzer() {
  buzzerActive    = true;
  buzzerStartTime = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

void updateBuzzer() {
  if (!buzzerActive) return;
  if (millis() - buzzerStartTime >= BUZZER_DURATION_MS) {
    buzzerActive = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ═══════════════════════════════════════════
// Alert functions
// ═══════════════════════════════════════════

void alertFence() {
  lastAlert1   = millis();
  triggered1   = true;
  detectCount1 = 0;

  arSerial.write('2');
  Serial.println("[ALERT→UNO] ส่ง '2' รั้วบ้าน");
  startBuzzer();

  float d = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
  sendAlertCard(String(zone1Name), d > 0 ? d : 0.0,
                getTimestamp(), "#e02020", "🚨");

  stat1.count++;
  stat1.lastAlertMs   = millis();
  stat1.lastAlertTime = getTimestamp();
  reportEvent(String(zone1Name), d > 0 ? d : 0.0);
}

void alertPool() {
  lastAlert2   = millis();
  triggered2   = true;
  detectCount2 = 0;

  arSerial.write('3');
  Serial.println("[ALERT→UNO] ส่ง '3' สระว่ายน้ำ");
  startBuzzer();

  float d = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
  sendAlertCard(String(zone2Name), d > 0 ? d : 0.0,
                getTimestamp(), "#1565c0", "💧");

  stat2.count++;
  stat2.lastAlertMs   = millis();
  stat2.lastAlertTime = getTimestamp();
  reportEvent(String(zone2Name), d > 0 ? d : 0.0);
}

void alertSensor3(float dist) {
  arSerial.write('3');
  Serial.println("[ALERT→UNO] ส่ง '3' เซนเซอร์ 3");
  startBuzzer();

  sendAlertCard(String(zone3Name), dist > 0 ? dist : 0.0,
                getTimestamp(), "#e02020", "🚨");

  stat3.count++;
  stat3.lastAlertMs   = millis();
  stat3.lastAlertTime = getTimestamp();
  reportEvent(String(zone3Name), dist > 0 ? dist : 0.0);
}

// ═══════════════════════════════════════════
// HTML Pages (PROGMEM)
// ═══════════════════════════════════════════

const char HTML_DASH[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>แดชบอร์ด · SmartGuard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh;padding:20px 16px 40px}
h1{font-size:19px;font-weight:700;color:#58a6ff;letter-spacing:1px;margin-bottom:4px}
.subtitle{font-size:12px;color:#8b949e;margin-bottom:20px}
nav{display:flex;gap:8px;margin-bottom:22px}
nav a{padding:7px 16px;border-radius:8px;font-size:13px;font-weight:600;text-decoration:none;background:#21262d;color:#8b949e}
nav a.active{background:#1f6feb;color:#fff}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:18px}
.card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px 14px}
.card .label{font-size:11px;color:#8b949e;letter-spacing:.8px;text-transform:uppercase;margin-bottom:8px}
.card .value{font-size:32px;font-weight:700;color:#58a6ff}
.card .sub{font-size:11px;color:#6e7681;margin-top:4px}
.card.warn .value{color:#f78166}
.card.ok   .value{color:#3fb950}
.zone-table{width:100%;border-collapse:collapse;margin-bottom:18px}
.zone-table th,.zone-table td{padding:10px 10px;text-align:left;border-bottom:1px solid #21262d;font-size:13px}
.zone-table th{font-size:11px;color:#8b949e;text-transform:uppercase;letter-spacing:.8px}
.badge{display:inline-block;padding:3px 10px;border-radius:20px;font-size:12px;font-weight:600}
.badge.fence{background:#3d1a00;color:#f0883e}
.badge.pool {background:#00204a;color:#58a6ff}
.wifi-box{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:14px 16px;margin-bottom:14px}
.wifi-box .label{font-size:11px;color:#8b949e;letter-spacing:.8px;text-transform:uppercase;margin-bottom:6px}
.wifi-row{display:flex;align-items:center;gap:8px;font-size:14px}
.dot{width:10px;height:10px;border-radius:50%;flex-shrink:0}
.dot.green{background:#3fb950}.dot.red{background:#f78166}.dot.yellow{background:#d29922}
.sync-btn{display:block;width:100%;padding:13px;background:#1f6feb;color:#fff;border:none;border-radius:10px;font-size:15px;font-weight:600;cursor:pointer;margin-bottom:10px;transition:.15s}
.sync-btn:active{opacity:.8}
.sync-status{font-size:12px;color:#8b949e;text-align:center;margin-bottom:18px;min-height:18px}
.recent-title{font-size:12px;color:#8b949e;letter-spacing:.8px;text-transform:uppercase;margin-bottom:10px}
.event-list{display:flex;flex-direction:column;gap:6px}
.ev-row{background:#161b22;border:1px solid #21262d;border-radius:8px;padding:10px 12px;display:flex;justify-content:space-between;align-items:center}
.ev-zone{font-size:13px;font-weight:600;color:#e6edf3}
.ev-time{font-size:11px;color:#8b949e}
.ev-dist{font-size:11px;color:#58a6ff}
.empty{text-align:center;color:#6e7681;padding:20px;font-size:13px}
</style>
</head>
<body>
<h1>🛡 SmartGuard</h1>
<div class="subtitle" id="ts">กำลังโหลด...</div>
<nav>
  <a href="/" class="active">แดชบอร์ด</a>
  <a href="/settings">ตั้งค่า</a>
</nav>

<div class="wifi-box">
  <div class="label">สถานะเครือข่าย</div>
  <div class="wifi-row"><span class="dot" id="dot-ap"></span><span id="wifi-ap">-</span></div>
  <div class="wifi-row" style="margin-top:6px"><span class="dot" id="dot-sta"></span><span id="wifi-sta">-</span></div>
  <div class="wifi-row" style="margin-top:6px"><span class="dot" id="dot-srv"></span><span id="wifi-srv">-</span></div>
</div>

<div class="cards">
  <div class="card warn">
    <div class="label">แจ้งเตือนวันนี้</div>
    <div class="value" id="total-today">-</div>
    <div class="sub">ทุกโซนรวมกัน</div>
  </div>
  <div class="card">
    <div class="label">Queue รอส่ง</div>
    <div class="value" id="queue-count">-</div>
    <div class="sub">รายการในหน่วยความจำ</div>
  </div>
</div>

<table class="zone-table">
  <thead><tr><th>โซน</th><th>วันนี้</th><th>ล่าสุด</th></tr></thead>
  <tbody id="zone-tbody"><tr><td colspan="3" class="empty">กำลังโหลด...</td></tr></tbody>
</table>

<button class="sync-btn" onclick="sync()">🔄 Sync ย้อนหลัง</button>
<div class="sync-status" id="sync-status"></div>

<div class="recent-title">เหตุการณ์ล่าสุด</div>
<div class="event-list" id="event-list"><div class="empty">กำลังโหลด...</div></div>

<script>
function load(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('ts').textContent='อัปเดต: '+d.ts;
    var ap=d.wifi_ap,sta=d.wifi_sta,srv=d.server_ok;
    document.getElementById('dot-ap').className='dot '+(ap?'green':'red');
    document.getElementById('wifi-ap').textContent='AP: '+d.ap_ssid+(ap?' ('+d.ap_clients+' เครื่อง)':'');
    document.getElementById('dot-sta').className='dot '+(sta?'green':'red');
    document.getElementById('wifi-sta').textContent='Internet: '+(sta?d.sta_ssid+' ('+d.sta_ip+')':'ไม่ได้เชื่อมต่อ');
    document.getElementById('dot-srv').className='dot '+(srv?'green':(d.queue>0?'yellow':'red'));
    document.getElementById('wifi-srv').textContent='เซิร์ฟเวอร์: '+d.last_sync;
    var poolTotal=d.z2_count+d.z3_count;
    var total=d.z1_count+poolTotal;
    document.getElementById('total-today').textContent=total;
    document.getElementById('queue-count').textContent=d.queue;
    var poolLast=d.z2_last;
    if(d.z3_last!=='-'&&d.z2_last==='-') poolLast=d.z3_last;
    else if(d.z3_last!=='-'&&d.z2_last!=='-') poolLast=d.z3_last>d.z2_last?d.z3_last:d.z2_last;
    var rows='';
    rows+=zoneRow(d.z1_name,d.z1_count,d.z1_last,'fence');
    rows+=zoneRow(d.z2_name,poolTotal,poolLast,'pool');
    document.getElementById('zone-tbody').innerHTML=rows;
    var evHtml='';
    if(d.events&&d.events.length>0){
      d.events.forEach(function(e){
        evHtml+='<div class="ev-row">'
          +'<div><div class="ev-zone">'+e.zone+'</div><div class="ev-time">'+e.ts+'</div></div>'
          +'<div class="ev-dist">'+(e.dist>0?e.dist.toFixed(0)+' cm':'')+'</div>'
          +'</div>';
      });
    } else {
      evHtml='<div class="empty">ยังไม่มีเหตุการณ์วันนี้</div>';
    }
    document.getElementById('event-list').innerHTML=evHtml;
  }).catch(function(){document.getElementById('ts').textContent='โหลดข้อมูลไม่สำเร็จ';});
}
function zoneRow(name,count,last,cls){
  return '<tr><td><span class="badge '+cls+'">'+name+'</span></td>'
    +'<td style="font-weight:700;font-size:18px;color:'+(count>0?'#f0883e':'#8b949e')+'">'+count+'</td>'
    +'<td style="font-size:12px;color:#8b949e">'+last+'</td></tr>';
}
function sync(){
  var btn=document.querySelector('.sync-btn');
  btn.disabled=true;btn.textContent='กำลัง Sync...';
  document.getElementById('sync-status').textContent='';
  fetch('/api/sync').then(r=>r.json()).then(function(d){
    btn.disabled=false;btn.textContent='🔄 Sync ย้อนหลัง';
    document.getElementById('sync-status').textContent=d.msg;
    load();
  }).catch(function(){
    btn.disabled=false;btn.textContent='🔄 Sync ย้อนหลัง';
    document.getElementById('sync-status').textContent='เชื่อมต่อไม่ได้';
  });
}
load();
setInterval(load,15000);
</script>
</body>
</html>
)rawliteral";

const char HTML_SETTINGS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ตั้งค่า · SmartGuard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh;padding:20px 16px 40px}
h1{font-size:19px;font-weight:700;color:#58a6ff;letter-spacing:1px;margin-bottom:4px}
.subtitle{font-size:12px;color:#8b949e;margin-bottom:20px}
nav{display:flex;gap:8px;margin-bottom:22px}
nav a{padding:7px 16px;border-radius:8px;font-size:13px;font-weight:600;text-decoration:none;background:#21262d;color:#8b949e}
nav a.active{background:#1f6feb;color:#fff}
.section{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:18px 16px;margin-bottom:14px}
.section h2{font-size:13px;color:#8b949e;letter-spacing:.8px;text-transform:uppercase;margin-bottom:14px}
.field{margin-bottom:12px}
.field label{display:block;font-size:12px;color:#8b949e;margin-bottom:5px}
.field input{width:100%;background:#0d1117;border:1px solid #30363d;border-radius:8px;padding:10px 12px;color:#e6edf3;font-size:15px;outline:none}
.field input:focus{border-color:#58a6ff}
.save-btn{display:block;width:100%;padding:13px;background:#238636;color:#fff;border:none;border-radius:10px;font-size:15px;font-weight:600;cursor:pointer;margin-top:4px;transition:.15s}
.save-btn:active{opacity:.8}
.msg{text-align:center;font-size:13px;margin-top:10px;min-height:20px;color:#3fb950}
.info-box{background:#0d1117;border:1px solid #21262d;border-radius:8px;padding:12px 14px;font-size:12px;color:#8b949e;line-height:1.7}
.info-box span{color:#58a6ff;font-weight:600}
</style>
</head>
<body>
<h1>🛡 SmartGuard</h1>
<div class="subtitle">ตั้งค่าระบบ</div>
<nav>
  <a href="/">แดชบอร์ด</a>
  <a href="/settings" class="active">ตั้งค่า</a>
</nav>
<div class="section">
  <h2>ชื่อโซนตรวจจับ</h2>
  <div class="field"><label>โซน 1 (เซนเซอร์ D1/D2)</label><input id="z1" type="text" maxlength="30"></div>
  <div class="field"><label>โซน 2 (เซนเซอร์ D6/D7)</label><input id="z2" type="text" maxlength="30"></div>
  <div class="field"><label>โซน 3 (เซนเซอร์ D3/D0)</label><input id="z3" type="text" maxlength="30"></div>
  <button class="save-btn" onclick="saveNames()">💾 บันทึกชื่อโซน</button>
  <div class="msg" id="save-msg"></div>
</div>
<div class="section">
  <h2>ข้อมูลระบบ</h2>
  <div class="info-box" id="info-box">กำลังโหลด...</div>
</div>
<script>
function loadNames(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('z1').value=d.z1_name;
    document.getElementById('z2').value=d.z2_name;
    document.getElementById('z3').value=d.z3_name;
    document.getElementById('info-box').innerHTML=
      '<b>Device ID:</b> <span>'+d.device_id+'</span><br>'
      +'<b>Server:</b> <span>'+d.server_host+'</span><br>'
      +'<b>Uptime:</b> <span>'+d.uptime+'</span><br>'
      +'<b>Heap ว่าง:</b> <span>'+d.heap+' bytes</span><br>'
      +'<b>NTP:</b> <span>'+d.ts+'</span>';
  });
}
function saveNames(){
  var z1=document.getElementById('z1').value.trim();
  var z2=document.getElementById('z2').value.trim();
  var z3=document.getElementById('z3').value.trim();
  if(!z1||!z2||!z3){document.getElementById('save-msg').textContent='กรุณากรอกชื่อโซนให้ครบ';return;}
  var msg=document.getElementById('save-msg');
  msg.textContent='กำลังบันทึก...';msg.style.color='#8b949e';
  fetch('/api/set-zones?z1='+encodeURIComponent(z1)+'&z2='+encodeURIComponent(z2)+'&z3='+encodeURIComponent(z3))
    .then(r=>r.json()).then(function(d){
      msg.textContent=d.msg;msg.style.color='#3fb950';
      setTimeout(function(){msg.textContent='';},3000);
    }).catch(function(){msg.textContent='บันทึกไม่สำเร็จ';msg.style.color='#f78166';});
}
loadNames();
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════
// Recent event log (in-RAM, 20 entries)
// ═══════════════════════════════════════════
#define EVENT_LOG_MAX 20
struct EventLog {
  String zone;
  float  dist;
  String ts;
};
EventLog eventLog[EVENT_LOG_MAX];
int eventLogHead  = 0;
int eventLogCount = 0;

void logEvent(String zone, float dist) {
  eventLog[eventLogHead].zone = zone;
  eventLog[eventLogHead].dist = dist;
  eventLog[eventLogHead].ts   = getTimestamp();
  eventLogHead = (eventLogHead + 1) % EVENT_LOG_MAX;
  if (eventLogCount < EVENT_LOG_MAX) eventLogCount++;
}

// ═══════════════════════════════════════════
// API handlers
// ═══════════════════════════════════════════

void handleApiStatus() {
  bool staOK = (WiFi.status() == WL_CONNECTED);
  unsigned long upSec = millis() / 1000;
  String upStr = String(upSec / 3600) + "h " + String((upSec % 3600) / 60) + "m";
  String evJson = "[";
  int shown = 0;
  for (int i = 0; i < eventLogCount && shown < 10; i++) {
    int idx = ((eventLogHead - 1 - i) + EVENT_LOG_MAX) % EVENT_LOG_MAX;
    if (shown > 0) evJson += ",";
    evJson += "{\"zone\":\"" + jsonEscape(eventLog[idx].zone) + "\","
              "\"dist\":" + String(eventLog[idx].dist, 1) + ","
              "\"ts\":\"" + jsonEscape(eventLog[idx].ts) + "\"}";
    shown++;
  }
  evJson += "]";
  String json = "{";
  json += "\"ts\":\"" + getTimestamp() + "\",";
  json += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"server_host\":\"" + String(SERVER_HOST) + "\",";
  json += "\"uptime\":\"" + upStr + "\",";
  json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"wifi_ap\":true,";
  json += "\"ap_ssid\":\"" + String(AP_SSID) + "\",";
  json += "\"ap_clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  json += "\"wifi_sta\":" + String(staOK ? "true" : "false") + ",";
  json += "\"sta_ssid\":\"" + String(staOK ? STA_SSID : "") + "\",";
  json += "\"sta_ip\":\"" + String(staOK ? WiFi.localIP().toString() : "") + "\",";
  json += "\"server_ok\":" + String(lastSyncOK ? "true" : "false") + ",";
  json += "\"last_sync\":\"" + jsonEscape(lastSyncMsg) + "\",";
  json += "\"queue\":" + String(queueCount) + ",";
  json += "\"z1_name\":\"" + jsonEscape(String(zone1Name)) + "\",";
  json += "\"z1_count\":" + String(stat1.count) + ",";
  json += "\"z1_last\":\"" + jsonEscape(stat1.lastAlertTime) + "\",";
  json += "\"z2_name\":\"" + jsonEscape(String(zone2Name)) + "\",";
  json += "\"z2_count\":" + String(stat2.count) + ",";
  json += "\"z2_last\":\"" + jsonEscape(stat2.lastAlertTime) + "\",";
  json += "\"z3_name\":\"" + jsonEscape(String(zone3Name)) + "\",";
  json += "\"z3_count\":" + String(stat3.count) + ",";
  json += "\"z3_last\":\"" + jsonEscape(stat3.lastAlertTime) + "\",";
  json += "\"events\":" + evJson;
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiSync() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(200, "application/json",
      "{\"ok\":false,\"msg\":\"ไม่มีอินเทอร์เน็ต ไม่สามารถ Sync ได้\"}");
    return;
  }
  if (queueCount == 0) {
    server.send(200, "application/json",
      "{\"ok\":true,\"msg\":\"ไม่มีข้อมูลค้างอยู่\"}");
    return;
  }
  int total = queueCount;
  int sent  = flushQueue();
  lastSyncOK  = (sent == total);
  lastSyncMsg = "Sync สำเร็จ " + String(sent) + "/" + String(total) + " รายการ " + getTimeOnly();
  server.send(200, "application/json",
    "{\"ok\":true,\"sent\":" + String(sent) + ",\"total\":" + String(total)
    + ",\"msg\":\"" + jsonEscape(lastSyncMsg) + "\"}");
}

void handleApiSetZones() {
  if (server.hasArg("z1")) {
    String v = server.arg("z1");
    v.toCharArray(zone1Name, ZONE_NAME_LEN);
    saveZoneName(ZONE1_ADDR, zone1Name);
  }
  if (server.hasArg("z2")) {
    String v = server.arg("z2");
    v.toCharArray(zone2Name, ZONE_NAME_LEN);
    saveZoneName(ZONE2_ADDR, zone2Name);
  }
  if (server.hasArg("z3")) {
    String v = server.arg("z3");
    v.toCharArray(zone3Name, ZONE_NAME_LEN);
    saveZoneName(ZONE3_ADDR, zone3Name);
  }
  server.send(200, "application/json",
    "{\"ok\":true,\"msg\":\"บันทึกชื่อโซนแล้ว (รอด reboot)\"}");
  Serial.printf("[SETTINGS] zone1=%s zone2=%s zone3=%s\n",
                zone1Name, zone2Name, zone3Name);
}

void handleCaptive() {
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

// ═══════════════════════════════════════════
// setup()
// ═══════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  arSerial.begin(9600);

  pinMode(TRIG1_PIN,  OUTPUT);
  pinMode(ECHO1_PIN,  INPUT);
  pinMode(TRIG2_PIN,  OUTPUT);
  pinMode(ECHO2_PIN,  INPUT);
  pinMode(TRIG3_PIN,  OUTPUT);
  pinMode(ECHO3_PIN,  INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(TRIG1_PIN,  LOW);
  digitalWrite(TRIG2_PIN,  LOW);
  digitalWrite(TRIG3_PIN,  LOW);
  digitalWrite(BUZZER_PIN, LOW);

  for (int i = 0; i < QUEUE_MAX; i++) pendingQueue[i].used = false;

  loadZoneNames();

  Serial.println(F("\n=============================="));
  Serial.println(F(" ESP8266 SmartGuard v2.1 (FIXED)"));
  Serial.println(F("=============================="));

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("[WiFi] AP '" + String(AP_SSID) + "' เริ่มแล้ว → 192.168.4.1");

  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("[WiFi] กำลังเชื่อม '" + String(STA_SSID) + "'");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] " + String(STA_SSID) + " IP: " + WiFi.localIP().toString());
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    int t = 0;
    while (now < 86400 && t < 20) { delay(500); now = time(nullptr); t++; }
    Serial.println("[NTP] " + getTimestamp());

    // อ่านค่าเซนเซอร์ก่อน boot แล้วส่งการ์ด LINE
    float b1 = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
    float b2 = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
    float b3 = readDistanceCM(TRIG3_PIN, ECHO3_PIN, PULSIN3_TIMEOUT);
    sendBootCard(b1, b2, b3);

  } else {
    Serial.println("\n[WiFi] '" + String(STA_SSID) + "' เชื่อมไม่ได้ — LINE/Server ไม่ทำงาน");
  }

  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/",         []() { server.send(200, "text/html; charset=utf-8", String(FPSTR(HTML_DASH))); });
  server.on("/settings", []() { server.send(200, "text/html; charset=utf-8", String(FPSTR(HTML_SETTINGS))); });
  server.on("/api/status",    handleApiStatus);
  server.on("/api/sync",      handleApiSync);
  server.on("/api/set-zones", handleApiSetZones);

  server.on("/generate_204",              handleCaptive);
  server.on("/fwlink",                    handleCaptive);
  server.on("/hotspot-detect.html",       handleCaptive);
  server.on("/library/test/success.html", handleCaptive);
  server.on("/success.txt",               handleCaptive);
  server.onNotFound([]() { server.send(200, "text/html; charset=utf-8", String(FPSTR(HTML_DASH))); });

  server.begin();
  Serial.println(F("[WEB] Server เริ่มแล้ว → http://192.168.4.1"));
  Serial.println(F("==============================\n"));
}

// ═══════════════════════════════════════════
// loop()
// ═══════════════════════════════════════════
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateBuzzer();

  // ─── โซน 1: รั้วบ้าน ──────────────────
  float dist1 = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
  if (dist1 < 0) {
    detectCount1 = 0;
  } else {
    if (dist1 < DETECT1_DIST_CM) {
      detectCount1++;
      if (detectCount1 >= DETECT_CONFIRM
          && !triggered1
          && millis() - lastAlert1 >= COOLDOWN_MS) {
        logEvent(String(zone1Name), dist1);
        alertFence();
      }
    } else {
      detectCount1 = 0;
      triggered1   = false;
    }
  }

  // ─── โซน 2: สระว่ายน้ำ ────────────────
  float dist2 = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
  if (dist2 < 0) {
    detectCount2 = 0;
  } else {
    if (dist2 < DETECT2_DIST_CM) {
      detectCount2++;
      if (detectCount2 >= DETECT_CONFIRM
          && !triggered2
          && millis() - lastAlert2 >= COOLDOWN_MS) {
        logEvent(String(zone2Name), dist2);
        alertPool();
      }
    } else {
      detectCount2 = 0;
      triggered2   = false;
    }
  }

  // ─── โซน 3: เซนเซอร์เสริม ────────────
  float dist3 = readDistanceCM(TRIG3_PIN, ECHO3_PIN, PULSIN3_TIMEOUT);
  if (dist3 < 0) {
    detectCount3 = 0;
  } else {
    if (dist3 < DETECT3_DIST_CM) {
      detectCount3++;
      if (detectCount3 >= DETECT_CONFIRM
          && !triggered3
          && millis() - lastAlert3 >= COOLDOWN_MS) {
        logEvent(String(zone3Name), dist3);
        alertSensor3(dist3);
        lastAlert3   = millis();
        triggered3   = true;
        detectCount3 = 0;
      }
    } else {
      detectCount3 = 0;
      triggered3   = false;
    }
  }

  // ─── WiFi reconnect + auto flush queue ──
  if (millis() - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] STA หลุด → reconnect...");
      WiFi.reconnect();
    } else if (queueCount > 0) {
      int sent = flushQueue();
      if (sent > 0) {
        lastSyncOK  = true;
        lastSyncMsg = "Auto-sync " + String(sent) + " รายการ " + getTimeOnly();
        Serial.println("[QUEUE] Auto-flush sent=" + String(sent));
      }
    }
  }

  delay(50);
}
