#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// กำหนด pin สำหรับ SoftwareSerial
SoftwareSerial mySoftwareSerial(D5, D6); // RX=D5, TX=D6
DFRobotDFPlayerMini myDFPlayer;

void setup() {
  Serial.begin(115200);
  mySoftwareSerial.begin(9600);

  delay(1000); // รอให้ DFPlayer พร้อม

  if (!myDFPlayer.begin(mySoftwareSerial)) {
    Serial.println("DFPlayer ไม่ตอบสนอง!");
    Serial.println("ตรวจสอบสาย / SD Card");
    while (true); // หยุดถ้าหา DFPlayer ไม่เจอ
  }

  Serial.println("DFPlayer พร้อมแล้ว");

  myDFPlayer.volume(20);  // ระดับเสียง 0-30
  myDFPlayer.play(1);     // เล่นไฟล์ที่ 1 = 0001.mp3
}

void loop() {
  // ไม่ต้องทำอะไร เล่นครั้งเดียวตอน setup
}