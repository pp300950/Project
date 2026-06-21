/*
  ultrasonic.h — จัดการอ่านค่า HC-SR04 ทั้ง 3 ตัว (สระ/ประตู/รั้ว)

  หลักการทำงานของ HC-SR04:
  1. ส่งพัลส์ HIGH 10 ไมโครวินาทีเข้าขา TRIG
  2. เซนเซอร์จะยิงคลื่นเสียงอัลตราโซนิกออกไป แล้วรอเสียงสะท้อนกลับ
  3. ขา ECHO จะเป็น HIGH ตลอดช่วงเวลาที่รอเสียงสะท้อน (ใช้ pulseIn อ่านความยาวพัลส์นี้)
  4. แปลงเวลาที่ได้เป็นระยะทาง: distance(cm) = duration(us) / 58

  ตรรกะแจ้งเตือน: ถ้าระยะที่วัดได้ < ALERT_DISTANCE_CM = มีคน/วัตถุเข้ามาในระยะนั้น
  ต่างจากโค้ดต้นแบบ (main2.ino) ที่ใช้เซนเซอร์ดิจิทัล 0/1 ตรงๆ — ของจริงนี้ต้องแปลงค่าระยะทางก่อน
*/

#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <Arduino.h>
#include "config.h"

enum ZoneId { ZONE_POOL = 0, ZONE_DOOR = 1, ZONE_FENCE = 2 };

struct UltrasonicZone {
  ZoneId id;
  uint8_t trigPin;
  uint8_t echoPin;
  const char* nameTh;     // ชื่อภาษาไทยสำหรับ log / แจ้งเตือน
  const char* nameSlug;   // ชื่อสำหรับส่งขึ้น API (ไม่มีภาษาไทย กัน encoding ปัญหา)
  bool wasTriggered;      // สถานะรอบก่อนหน้า กันแจ้งเตือนซ้ำตอนยังอยู่ในระยะ
  unsigned long lastAlertAt;
};

UltrasonicZone zones[3] = {
  { ZONE_POOL,  TRIG_POOL,  ECHO_POOL,  "สระว่ายน้ำ", "pool",  false, 0 },
  { ZONE_DOOR,  TRIG_DOOR,  ECHO_DOOR,  "ประตู/ทางออก", "door",  false, 0 },
  { ZONE_FENCE, TRIG_FENCE, ECHO_FENCE, "รั้วบ้าน",    "fence", false, 0 },
};

void setupUltrasonicPins() {
  for (int i = 0; i < 3; i++) {
    pinMode(zones[i].trigPin, OUTPUT);
    pinMode(zones[i].echoPin, INPUT);
    digitalWrite(zones[i].trigPin, LOW);
  }
}

// อ่านระยะทางจาก HC-SR04 หนึ่งตัว คืนค่าเป็น cm
// คืนค่า -1 ถ้าอ่านไม่ได้ (timeout — ไม่มีวัตถุในระยะตรวจจับ หรือสายหลุด)
long readDistanceCm(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // timeout 25000us ~ ระยะไกลสุดที่ HC-SR04 วัดได้จริง (~4m) กันบอร์ดค้างถ้าไม่มีเสียงสะท้อนกลับ
  long duration = pulseIn(echoPin, HIGH, 25000);

  if (duration == 0) return -1; // ไม่มีเสียงสะท้อนกลับมาในเวลาที่กำหนด

  return duration / 58; // สูตรแปลงเวลา -> เซนติเมตร
}

#endif
