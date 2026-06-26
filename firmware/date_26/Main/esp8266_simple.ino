#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <SoftwareSerial.h>
#include <time.h>

// ─── WiFi AP (Web Controller) ────────────
const char* AP_SSID = "ABC";
const char* AP_PASS = "12345678";

// ─── WiFi Station (LINE) ─────────────────
const char* STA_SSID = "PP";
const char* STA_PASS = "12345678";

// ─── LINE ────────────────────────────────
#define LINE_TOKEN "+WCQuf3X+jrVPNhe61leiOB9TSgimmcEtn2YeK2q5DUV8amp4mCVY8kNzNIfw5fQVvc5dwR4JwzhcogU1WDo6SresUC5D4IZYqWpGtODwWbUnwjPSaWj3k0fUoU/Bwus6FiyxKYib5zfAIWHu5jVygdB04t89/1O/w1cDnyilFU="
#define USER_ID    "Uf72325ea6cc9aee84558f42e01a07e33"

// ─── Server / DNS ─────────────────────────
const byte DNS_PORT = 53;
DNSServer dnsServer;
ESP8266WebServer server(80);

// ─── Serial to Arduino ────────────────────
SoftwareSerial arSerial(-1, D4);  // TX=D4

// ─── Sensor 1: รั้วบ้าน ───────────────────
#define TRIG1_PIN          D1
#define ECHO1_PIN          D2
#define DETECT1_DIST_CM    16
#define PULSIN1_TIMEOUT    4000

// ─── Sensor 2: สระว่ายน้ำ ─────────────────
#define TRIG2_PIN          D6
#define ECHO2_PIN          D7
#define DETECT2_DIST_CM    20
#define PULSIN2_TIMEOUT    4500

// ─── Buzzer ───────────────────────────────
#define BUZZER_PIN         D5
#define BUZZER_DURATION_MS 2000

// ─── Cooldown / Confirm ───────────────────
#define COOLDOWN_MS        30000
#define DETECT_CONFIRM     3

unsigned long lastAlert1  = 0;
unsigned long lastAlert2  = 0;
int detectCount1 = 0;
int detectCount2 = 0;
bool triggered1  = false;
bool triggered2  = false;

// ─── Buzzer non-blocking ──────────────────
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;

// ─────────────────────────────────────────
// HTML หน้าเว็บ (เหมือนเดิมทุกอย่าง)
// ─────────────────────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
  <title>ระบบแจ้งเตือนเสียง</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: #0d1117;
      color: #e6edf3;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 30px 20px;
    }
    .header { text-align: center; margin-bottom: 30px; }
    .header h1 { font-size: 22px; font-weight: 700; letter-spacing: 2px; color: #58a6ff; text-transform: uppercase; }
    .header p  { font-size: 13px; color: #8b949e; margin-top: 6px; }
    .zone-grid { display: flex; flex-direction: column; gap: 14px; width: 100%; max-width: 360px; }
    .zone-btn {
      display: flex; align-items: center; gap: 16px;
      padding: 20px 22px; border: none; border-radius: 14px;
      cursor: pointer; font-size: 17px; font-weight: 600; color: white;
      width: 100%; text-align: left;
      transition: transform 0.1s, opacity 0.1s;
      -webkit-tap-highlight-color: transparent;
    }
    .zone-btn:active { transform: scale(0.96); opacity: 0.85; }
    .zone-btn .icon  { font-size: 28px; flex-shrink: 0; }
    .zone-btn .label { display: flex; flex-direction: column; }
    .zone-btn .label span { font-size: 12px; font-weight: 400; opacity: 0.75; margin-top: 2px; }
    .btn-1 { background: linear-gradient(135deg, #e85d04, #f48c06); }
    .btn-2 { background: linear-gradient(135deg, #023e8a, #0077b6); }
    .btn-3 { background: linear-gradient(135deg, #4a0e8f, #7b2d8b); }
    .status-box {
      margin-top: 28px; width: 100%; max-width: 360px;
      background: #161b22; border: 1px solid #30363d;
      border-radius: 12px; padding: 16px 18px;
    }
    .status-label { font-size: 11px; color: #8b949e; letter-spacing: 1px; text-transform: uppercase; margin-bottom: 8px; }
    .status-text  { font-size: 15px; color: #58a6ff; font-weight: 500; min-height: 22px; }
    .dot-flash::after { content: ''; animation: dots 1.2s infinite; }
    @keyframes dots { 0% { content:''; } 33% { content:'.'; } 66% { content:'..'; } 100% { content:'...'; } }
    .footer { margin-top: 30px; font-size: 11px; color: #484f58; }
  </style>
</head>
<body>
  <div class="header">
    <h1>🔊 ระบบแจ้งเตือนเสียง</h1>
    <p>กดปุ่มเพื่อเล่นเสียงแจ้งเตือน</p>
  </div>
  <div class="zone-grid">
    <button class="zone-btn btn-1" onclick="send(2)">
      <span class="icon">🏠</span>
      <div class="label">รั้วบ้าน<span>001.mp3</span></div>
    </button>
    <button class="zone-btn btn-2" onclick="send(3)">
      <span class="icon">🏊</span>
      <div class="label">สระว่ายน้ำ<span>002.mp3</span></div>
    </button>
    <button class="zone-btn btn-3" onclick="send(1)">
      <span class="icon">👟</span>
      <div class="label">สวมรองเท้า<span>003.mp3</span></div>
    </button>
  </div>
  <div class="status-box">
    <div class="status-label">สถานะ</div>
    <div class="status-text" id="status">รอคำสั่ง...</div>
  </div>
  <div class="footer">ESP8266 · ABC Network · 192.168.4.1</div>
  <script>
    const names = { 2:'รั้วบ้าน', 3:'สระว่ายน้ำ', 1:'สวมรองเท้า' };
    const statusEl = document.getElementById('status');
    function send(n) {
      statusEl.className = 'status-text dot-flash';
      statusEl.textContent = 'กำลังส่งคำสั่ง';
      fetch('/play?track=' + n)
        .then(r => r.text())
        .then(() => {
          statusEl.className = 'status-text';
          statusEl.textContent = '▶ กำลังเล่น: ' + names[n];
          setTimeout(() => { statusEl.textContent = 'รอคำสั่ง...'; }, 4000);
        })
        .catch(() => {
          statusEl.className = 'status-text';
          statusEl.textContent = '❌ ส่งไม่สำเร็จ ลองใหม่';
        });
    }
  </script>
</body>
</html>
)rawliteral";

// ─────────────────────────────────────────
// ฟังก์ชันช่วย
// ─────────────────────────────────────────
String getTimestamp() {
  time_t now = time(nullptr);
  if (now < 86400) return "ไม่สามารถดึงเวลาได้";
  struct tm* t = localtime(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", t);
  return String(buf);
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

void sendLine(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LINE] WiFi ไม่ได้เชื่อมต่อ — ข้ามการส่ง LINE");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.line.me/v2/bot/message/push");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(LINE_TOKEN));
  String payload =
    "{\"to\":\"" + String(USER_ID) + "\","
    "\"messages\":[{\"type\":\"text\",\"text\":\""
    + jsonEscape(message) + "\"}]}";
  int code = http.POST(payload);
  Serial.println("[LINE] HTTP " + String(code));
  http.end();
}

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

// ─────────────────────────────────────────
// Alert functions — ส่งเสียง + LINE
// ─────────────────────────────────────────
void alertFence() {
  lastAlert1   = millis();
  triggered1   = true;
  detectCount1 = 0;

  arSerial.write('2');  // รั้วบ้าน = track 2
  Serial.println("[ALERT→UNO] ส่ง '2' รั้วบ้าน");

  startBuzzer();

  String msg = "❗❗ แจ้งเตือน ❗❗\n";
  msg += "🏚 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ รั้วบ้าน🏚\n";
  msg += "🕐 เวลา: " + getTimestamp();
  sendLine(msg);
}

void alertPool() {
  lastAlert2   = millis();
  triggered2   = true;
  detectCount2 = 0;

  arSerial.write('3');  // สระว่ายน้ำ = track 3
  Serial.println("[ALERT→UNO] ส่ง '3' สระว่ายน้ำ");

  startBuzzer();

  String msg = "⚡⚡ แจ้งเตือน ⚡⚡\n";
  msg += "💦 ตรวจพบผู้ป่วย ณ\n";
  msg += "บริเวณ สระว่ายน้ำ💦\n";
  msg += "🕐 เวลา: " + getTimestamp();
  sendLine(msg);
}

// ─────────────────────────────────────────
// Web handlers
// ─────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", String(FPSTR(HTML_PAGE)));
}

void handlePlay() {
  if (!server.hasArg("track")) {
    server.send(400, "text/plain", "missing track");
    return;
  }
  String t  = server.arg("track");
  char   cmd = t.charAt(0);
  if (cmd == '1' || cmd == '2' || cmd == '3') {
    arSerial.write(cmd);
    Serial.print(F("[WEB→UNO] ส่ง '"));
    Serial.print(cmd);
    Serial.println(F("'"));
    server.send(200, "text/plain", "OK:" + t);
  } else {
    server.send(400, "text/plain", "invalid track");
  }
}

void handleCaptive() {
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

// ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  arSerial.begin(9600);

  pinMode(TRIG1_PIN,  OUTPUT);
  pinMode(ECHO1_PIN,  INPUT);
  pinMode(TRIG2_PIN,  OUTPUT);
  pinMode(ECHO2_PIN,  INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(TRIG1_PIN,  LOW);
  digitalWrite(TRIG2_PIN,  LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println(F("\n=============================="));
  Serial.println(F(" ESP8266 — Sensor + Web + LINE"));
  Serial.println(F("=============================="));

  // ─── WiFi AP_STA: AP สำหรับ Web + STA สำหรับ LINE ───
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("[WiFi] AP 'ABC' เริ่มแล้ว");

  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("[WiFi] กำลังเชื่อม 'PP'");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] PP เชื่อมแล้ว IP: " + WiFi.localIP().toString());
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    int t = 0;
    while (now < 86400 && t < 20) { delay(500); now = time(nullptr); t++; }
    Serial.println("[NTP] เวลา: " + getTimestamp());
    sendLine("🟢 ระบบพร้อมทำงาน\n"
             "📌 รั้วบ้าน: " + String(DETECT1_DIST_CM) + " cm\n"
             "📌 สระว่ายน้ำ: " + String(DETECT2_DIST_CM) + " cm\n"
             "⏰ " + getTimestamp());
  } else {
    Serial.println("\n[WiFi] PP เชื่อมไม่ได้ — LINE จะไม่ทำงาน");
  }

  // ─── DNS + Web Server ────────────────────
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", handleRoot);
  server.on("/play", handlePlay);
  server.on("/generate_204",              handleCaptive);
  server.on("/fwlink",                    handleCaptive);
  server.on("/hotspot-detect.html",       handleCaptive);
  server.on("/library/test/success.html", handleCaptive);
  server.on("/success.txt",               handleCaptive);
  server.onNotFound(handleRoot);
  server.begin();

  Serial.println(F("[WEB] Server เริ่มแล้ว — 192.168.4.1"));
  Serial.println(F("==============================\n"));
}

// ─────────────────────────────────────────
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateBuzzer();

  // ─── ตรวจจับ: รั้วบ้าน ───────────────────
  float dist1 = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
  if (dist1 < 0) {
    detectCount1 = 0;
  } else {
    if (dist1 < DETECT1_DIST_CM) {
      detectCount1++;
      if (detectCount1 >= DETECT_CONFIRM
          && !triggered1
          && millis() - lastAlert1 >= COOLDOWN_MS) {
        alertFence();
      }
    } else {
      detectCount1 = 0;
      triggered1   = false;
    }
  }

  // ─── ตรวจจับ: สระว่ายน้ำ ─────────────────
  float dist2 = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
  if (dist2 < 0) {
    detectCount2 = 0;
  } else {
    if (dist2 < DETECT2_DIST_CM) {
      detectCount2++;
      if (detectCount2 >= DETECT_CONFIRM
          && !triggered2
          && millis() - lastAlert2 >= COOLDOWN_MS) {
        alertPool();
      }
    } else {
      detectCount2 = 0;
      triggered2   = false;
    }
  }

  delay(50);
}

