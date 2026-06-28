/*
 * ========================================
 * Arduino Uno — ฝั่งรับคำสั่ง + DFPlayer
 * ========================================
 *
 * การต่อวงจรทั้งหมด:
 * ────────────────────────────────────────
 *
 *  [ESP8266] ──Level Shifter──► Arduino Uno ──► DFPlayer Mini ──► ลำโพง
 *
 *  Level Shifter → Arduino:
 *  ─────────────────────────
 *  B1 (HV ฝั่ง) ─────────── Pin 10 (RX รับจาก ESP)
 *  5V ────────────────────── HV ของ Level Shifter
 *  GND ───────────────────── GND (ร่วมกับ ESP8266 ด้วย!)
 *
 *  Arduino → DFPlayer Mini:
 *  ─────────────────────────
 *  Pin 2 (RX) ◄──────────── DFPlayer TX
 *  Pin 3 (TX) ──────[1kΩ]── DFPlayer RX
 *  5V ────────────────────── DFPlayer VCC
 *  GND ───────────────────── DFPlayer GND
 *                            DFPlayer SPK_1 ── ลำโพง +
 *                            DFPlayer SPK_2 ── ลำโพง -
 *
 * ⚠️  GND ต้องร่วมกัน: ESP8266 + Level Shifter + Arduino + DFPlayer
 * ⚠️  ใส่ resistor 1kΩ ระหว่าง Pin3 กับ DFPlayer RX
 * ========================================
 */

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ─── SoftwareSerial ──────────────────────
SoftwareSerial espSerial(10, 11);  // Pin10=RX (รับจาก ESP), Pin11=TX (ไม่ใช้)
SoftwareSerial dfSerial(2, 3);     // Pin2=RX, Pin3=TX (ผ่าน 1kΩ)

DFRobotDFPlayerMini myDFPlayer;

// ─────────────────────────────────────────
void setup() {
  Serial.begin(9600);     // Serial Monitor
  espSerial.begin(9600);  // รับจาก ESP8266 (ต้องตรงกับ ESP)
  dfSerial.begin(9600);   // คุยกับ DFPlayer
  delay(2000);

  Serial.println(F("=============================="));
  Serial.println(F(" Arduino Uno — รับคำสั่ง + DFPlayer"));
  Serial.println(F("=============================="));

  // ─── เริ่ม DFPlayer ─────────────────
  Serial.print(F("[INIT] DFPlayer... "));
  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println(F("FAILED!"));
    Serial.println(F("  → เช็คสาย Pin2/Pin3 กับ DFPlayer"));
    Serial.println(F("  → เช็ค SD Card และชื่อไฟล์ 0001.mp3"));
    Serial.println(F("  → เช็ค GND ร่วมกัน"));
    while (true) delay(500);
  }
  Serial.println(F("OK!"));

  myDFPlayer.volume(28);  // ระดับเสียง 0-30
  myDFPlayer.setTimeOut(500);

  Serial.println(F("\n[READY] รอคำสั่ง..."));
  Serial.println(F("  แหล่งคำสั่ง 1: ESP8266 (Pin 10)"));
  Serial.println(F("  แหล่งคำสั่ง 2: Serial Monitor (พิมพ์เอง)"));
  Serial.println(F("  1 = รั้วบ้าน (001.mp3)"));
  Serial.println(F("  2 = สระว่ายน้ำ (002.mp3)"));
  Serial.println(F("  3 = ประตู/รองเท้า (003.mp3)"));
  Serial.println(F("==============================\n"));
}

// ─────────────────────────────────────────
void loop() {

  // ─── รับจาก ESP8266 ─────────────────
  if (espSerial.available()) {
    char cmd = espSerial.read();
    if (cmd != '\n' && cmd != '\r') {
      Serial.print(F("[ESP8266] รับคำสั่ง: "));
      Serial.println(cmd);
      playTrack(cmd);
    }
  }

  // ─── รับจาก Serial Monitor ──────────
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd != '\n' && cmd != '\r') {
      Serial.print(F("[PC] รับคำสั่ง: "));
      Serial.println(cmd);
      playTrack(cmd);
    }
  }

  // ─── รับ Event จาก DFPlayer ─────────
  if (myDFPlayer.available()) {
    uint8_t type  = myDFPlayer.readType();
    int     value = myDFPlayer.read();
    if (type == DFPlayerPlayFinished) {
      Serial.print(F("[INFO] เล่นจบ Track: "));
      Serial.println(value);
    }
    if (type == DFPlayerError) {
      Serial.print(F("[ERR] DFPlayer Error: "));
      Serial.println(value);
    }
  }
}

// ─────────────────────────────────────────
void playTrack(char cmd) {
  switch (cmd) {
    case '1':
      myDFPlayer.play(1);
      Serial.println(F("[PLAY] ▶ 001.mp3 — รั้วบ้าน"));
      break;
    case '2':
      myDFPlayer.play(2);
      Serial.println(F("[PLAY] ▶ 002.mp3 — สระว่ายน้ำ"));
      break;
    case '3':
      myDFPlayer.play(3);
      Serial.println(F("[PLAY] ▶ 003.mp3 — ประตู/รองเท้า"));
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
