#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <SoftwareSerial.h>

const char* AP_SSID = "ABC";
const char* AP_PASS = "12345678";

const byte    DNS_PORT = 53;
DNSServer     dnsServer;
ESP8266WebServer server(80);

SoftwareSerial arSerial(-1, D4);
String lastStatus = "รอคำสั่ง...";

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
    .header h1 {
      font-size: 22px;
      font-weight: 700;
      letter-spacing: 2px;
      color: #58a6ff;
      text-transform: uppercase;
    }
    .header p { font-size: 13px; color: #8b949e; margin-top: 6px; }
    .zone-grid {
      display: flex;
      flex-direction: column;
      gap: 14px;
      width: 100%;
      max-width: 360px;
    }
    .zone-btn {
      display: flex;
      align-items: center;
      gap: 16px;
      padding: 20px 22px;
      border: none;
      border-radius: 14px;
      cursor: pointer;
      font-size: 17px;
      font-weight: 600;
      color: white;
      width: 100%;
      text-align: left;
      transition: transform 0.1s, opacity 0.1s;
      -webkit-tap-highlight-color: transparent;
    }
    .zone-btn:active { transform: scale(0.96); opacity: 0.85; }
    .zone-btn .icon { font-size: 28px; flex-shrink: 0; }
    .zone-btn .label { display: flex; flex-direction: column; }
    .zone-btn .label span {
      font-size: 12px;
      font-weight: 400;
      opacity: 0.75;
      margin-top: 2px;
    }
    .btn-1 { background: linear-gradient(135deg, #e85d04, #f48c06); }
    .btn-2 { background: linear-gradient(135deg, #023e8a, #0077b6); }
    .btn-3 { background: linear-gradient(135deg, #4a0e8f, #7b2d8b); }
    .status-box {
      margin-top: 28px;
      width: 100%;
      max-width: 360px;
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 12px;
      padding: 16px 18px;
    }
    .status-label {
      font-size: 11px;
      color: #8b949e;
      letter-spacing: 1px;
      text-transform: uppercase;
      margin-bottom: 8px;
    }
    .status-text { font-size: 15px; color: #58a6ff; font-weight: 500; min-height: 22px; }
    .dot-flash::after {
      content: '';
      animation: dots 1.2s infinite;
    }
    @keyframes dots {
      0%   { content: ''; }
      33%  { content: '.'; }
      66%  { content: '..'; }
      100% { content: '...'; }
    }
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
      <div class="label">
        รั้วบ้าน
        <span>001.mp3</span>
      </div>
    </button>

    <button class="zone-btn btn-2" onclick="send(3)">
      <span class="icon">🏊</span>
      <div class="label">
        สระว่ายน้ำ
        <span>002.mp3</span>
      </div>
    </button>

    <button class="zone-btn btn-3" onclick="send(1)">
      <span class="icon">👟</span>
      <div class="label">
        สวมรองเท้า
        <span>003.mp3</span>
      </div>
    </button>

  </div>

  <div class="status-box">
    <div class="status-label">สถานะ</div>
    <div class="status-text" id="status">รอคำสั่ง...</div>
  </div>

  <div class="footer">ESP8266 · ABC Network · 192.168.4.1</div>

  <script>
    const names = { 2: 'รั้วบ้าน', 3: 'สระว่ายน้ำ', 1: 'สวมรองเท้า' };
    const statusEl = document.getElementById('status');

    function send(n) {
      statusEl.className = 'status-text dot-flash';
      statusEl.textContent = 'กำลังส่งคำสั่ง';

      fetch('/play?track=' + n)
        .then(r => r.text())
        .then(t => {
          statusEl.className = 'status-text';
          statusEl.textContent = '▶ กำลังเล่น: ' + names[n];
          setTimeout(() => {
            statusEl.textContent = 'รอคำสั่ง...';
          }, 4000);
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

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", String(FPSTR(HTML_PAGE)));
}

void handlePlay() {
  if (!server.hasArg("track")) {
    server.send(400, "text/plain", "missing track");
    return;
  }
  String t = server.arg("track");
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

void setup() {
  Serial.begin(115200);
  arSerial.begin(9600);
  delay(500);

  Serial.println(F("\n=============================="));
  Serial.println(F(" ESP8266 Web Controller"));
  Serial.println(F("=============================="));

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  IPAddress apIP = WiFi.softAPIP();
  Serial.print(F("[WiFi] AP: "));   Serial.println(AP_SSID);
  Serial.print(F("[WiFi] IP: "));   Serial.println(apIP);

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
  Serial.println(F("[WEB] Server เริ่มแล้ว"));
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}


