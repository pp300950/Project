#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

/*
 * การต่อสาย Arduino:
 *
 *   === รับจาก ESP8266 ===
 *   Arduino Pin10 (RX) ← ESP D4 (TX1) ผ่าน voltage divider 1kΩ+2kΩ
 *   GND ร่วมกับ ESP
 *
 *   === DFPlayer Mini ===
 *   Arduino Pin2 (RX) ← DFPlayer TX
 *   Arduino Pin3 (TX) → DFPlayer RX
 *   DFPlayer VCC → 5V
 *   DFPlayer GND → GND
 *   Speaker → SPK_1, SPK_2
 *
 *   === ทดสอบผ่าน Serial Monitor ===
 *   พิมพ์ 1 → เล่น 001.mp3 (รั้วบ้าน)
 *   พิมพ์ 2 → เล่น 002.mp3 (สระว่ายน้ำ)
 *   พิมพ์ 3 → เล่น 003.mp3 (ประตู/รองเท้า)
 */

SoftwareSerial espSerial(10, 11);  // RX=10, TX=11
SoftwareSerial dfSerial(2, 3);     // RX=2, TX=3
DFRobotDFPlayerMini myDFPlayer;

// ================================================

void playTrack(char cmd) {
  if (cmd == '1') {
    myDFPlayer.play(1);
    Serial.println("[PLAY] 001.mp3 (รั้วบ้าน)");
  }
  else if (cmd == '2') {
    myDFPlayer.play(2);
    Serial.println("[PLAY] 002.mp3 (สระว่ายน้ำ)");
  }
  else if (cmd == '3') {
    myDFPlayer.play(3);
    Serial.println("[PLAY] 003.mp3 (ประตู/รองเท้า)");
  }
  else {
    Serial.println("[WARN] คำสั่งไม่รู้จัก: " + String(cmd));
  }
}

// ================================================

void setup() {
  Serial.begin(115200);
  espSerial.begin(9600);
  dfSerial.begin(9600);

  delay(2000);

  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer ไม่ตอบสนอง!");
    while (true) { delay(100); }
  }

  myDFPlayer.volume(28);
  Serial.println("=== Arduino พร้อมรับคำสั่ง ===");
  Serial.println("พิมพ์ 1=รั้วบ้าน  2=สระว่ายน้ำ  3=ประตู");
}

void loop() {
  // รับจาก ESP8266
  if (espSerial.available()) {
    char cmd = espSerial.read();
    Serial.println("[ESP] รับคำสั่ง: " + String(cmd));
    playTrack(cmd);
  }

  // รับจาก Serial Monitor (ทดสอบ)
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '\n' || cmd == '\r') return;  // ข้าม newline
    Serial.println("[PC] รับคำสั่ง: " + String(cmd));
    playTrack(cmd);
  }
}