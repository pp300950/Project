#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ─── เปลี่ยนจาก Pin 10 → A0 ───────────────
// A0 ใช้เป็น Digital ได้ ค่าคือ 14 บน Uno
SoftwareSerial espSerial(A0, A1);  // RX=A0, TX=A1 (ไม่ใช้)
SoftwareSerial dfSerial(2, 3);
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);
  dfSerial.begin(9600);
  delay(1000);
  Serial.println(F("=============================="));
  Serial.println(F(" ทดสอบรับสัญญาณจาก ESP8266"));
  Serial.println(F(" RX = Pin A0"));
  Serial.println(F("=============================="));
  Serial.println(F(" รอข้อมูล..."));

  dfSerial.listen();
  Serial.print(F("[INIT] DFPlayer... "));
  if (!myDFPlayer.begin(dfSerial, false)) {  // ← false = ไม่รอ ACK
    Serial.println(F("FAILED!"));
    while (true) delay(500);
  }
  Serial.println(F("OK!"));
  myDFPlayer.volume(28);

  espSerial.listen();
}

void loop() {
  // ─── รับจาก ESP8266 ─────────────────────
  if (espSerial.available()) {
    char c = espSerial.read();
    // แสดง ตัวอักษร
    Serial.print(F("[RX] ตัวอักษร : "));
    Serial.println(c);
    // แสดง ค่า ASCII (เลขฐาน 10)
    Serial.print(F("[RX] ASCII Dec : "));
    Serial.println((int)c);
    // แสดง ค่า HEX
    Serial.print(F("[RX] ASCII Hex : 0x"));
    Serial.println((int)c, HEX);
    Serial.println(F("------------------------------"));
    if (c != '\n' && c != '\r') {
      playTrack(c);
    }
  }

  // ─── รับจาก Serial Monitor ──────────────
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd != '\n' && cmd != '\r') {
      Serial.print(F("[PC] รับคำสั่ง: "));
      Serial.println(cmd);
      playTrack(cmd);
    }
  }
}

void playTrack(char cmd) {
  switch (cmd) {
    case '1':
      myDFPlayer.playMp3Folder(1);  // ← เปลี่ยนจาก play(1)
      Serial.println(F("[PLAY] ▶ 0001.mp3 — รั้วบ้าน"));
      break;
    case '2':
      myDFPlayer.playMp3Folder(2);  // ← เปลี่ยนจาก play(2)
      Serial.println(F("[PLAY] ▶ 0002.mp3 — สระว่ายน้ำ"));
      break;
    case '3':
      myDFPlayer.playMp3Folder(3);  // ← เปลี่ยนจาก play(3)
      Serial.println(F("[PLAY] ▶ 0003.mp3 — สวมรองเท้า"));
      break;
    case 'p':
      myDFPlayer.pause();
      Serial.println(F("[CMD] ⏸ หยุดชั่วคราว"));
      break;
    case 'r':
      myDFPlayer.start();
      Serial.println(F("[CMD] ▶ เล่นต่อ"));
      break;
    case '+':
      myDFPlayer.volumeUp();
      Serial.println(F("[CMD] 🔊 เพิ่มเสียง"));
      break;
    case '-':
      myDFPlayer.volumeDown();
      Serial.println(F("[CMD] 🔉 ลดเสียง"));
      break;
    default:
      Serial.print(F("[WARN] ไม่รู้จักคำสั่ง: "));
      Serial.println(cmd);
      break;
  }
}

