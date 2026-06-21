/*
  config.h — ค่าตั้งต้นของ ESP8266 มาสเตอร์

  บอร์ดนี้ทำงาน 4 อย่างพร้อมกัน (ตามเอกสารสถาปัตยกรรม ชั้นที่ 2):
  1. AP โหมด — ปล่อย WiFi ของตัวเอง ให้ ESP-01 ทั้งสองข้างต่อเข้ามา
  2. STA โหมด — ต่อ WiFi บ้าน/มือถือ เพื่อขึ้นเน็ตไปหา server
  3. อ่าน ultrasonic HC-SR04 x3 ตัว (สระ/ประตู/รั้ว)
  4. Mini dashboard ในตัว (เข้าผ่าน AP โดยตรง ไม่ผ่านเน็ต)

  ค่าพวกนี้คือค่า default ตอนยังไม่ตั้งค่าผ่าน Config Portal
  ตั้งค่าจริงจะถูกเก็บ EEPROM แทนเมื่อมีคนกรอกฟอร์มผ่านพอร์ทัลแล้ว
*/

#ifndef CONFIG_H
#define CONFIG_H

// ===== AP ที่มาสเตอร์ปล่อยให้ ESP-01 ต่อเข้ามา (วง local ไม่มีเน็ตจริง) =====
// ต้องตรงกับค่าที่ตั้งใน config.h ของ esp01_shoe
#define DEFAULT_AP_SSID      "ShoeMonitor-Master"
#define DEFAULT_AP_PASSWORD  "shoemonitor2024"

// ===== STA — WiFi บ้าน/มือถือ ที่มาสเตอร์ใช้ขึ้นเน็ตไปหา server =====
#define DEFAULT_STA_SSID      "PP"
#define DEFAULT_STA_PASSWORD  "12345678"

// ===== ปลายทาง FastAPI mock server (โหมด cloud_mode: on) =====
// เปลี่ยนเป็น URL จริงของ Render ตอน deploy จริง — ตอนนี้ชี้ไปที่ mock server สำหรับทดสอบ flow
#define DEFAULT_SERVER_URL    "http://192.168.1.100:8000"
#define SERVER_EVENT_PATH     "/api/v1/events"
#define SERVER_HEARTBEAT_PATH "/api/v1/heartbeat"

// ===== cloud_mode: true = ส่งขึ้น FastAPI server, false = local-only (ยังไม่ส่งไปไหน เก็บ log ไว้ดูใน mini dashboard) =====
#define DEFAULT_CLOUD_MODE    true

// ===== LINE Messaging API (ย้ายมาจากโค้ดต้นแบบ main2.ino) =====
#define DEFAULT_LINE_TOKEN "+WCQuf3X+jrVPNhe61leiOB9TSgimmcEtn2YeK2q5DUV8amp4mCVY8kNzNIfw5fQVvc5dwR4JwzhcogU1WDo6SresUC5D4IZYqWpGtODwWbUnwjPSaWj3k0fUoU/Bwus6FiyxKYib5zfAIWHu5jVygdB04t89/1O/w1cDnyilFU="
#define DEFAULT_LINE_USER_ID "Uf72325ea6cc9aee84558f42e01a07e33"

// ===== ขา GPIO ของ ESP8266 (มาสเตอร์มีขาเยอะกว่า ESP-01 ใช้ครบทุกฟีเจอร์ได้) =====
// HC-SR04 ตัวที่ 1 — จุดเฝ้าระวัง: สระว่ายน้ำ
#define TRIG_POOL   D1
#define ECHO_POOL   D2

// HC-SR04 ตัวที่ 2 — จุดเฝ้าระวัง: ประตู/ทางออก
#define TRIG_DOOR   D3
#define ECHO_DOOR   D4

// HC-SR04 ตัวที่ 3 — จุดเฝ้าระวัง: รั้วบ้าน
#define TRIG_FENCE  D5
#define ECHO_FENCE  D6

// บัซเซอร์ — ดังทันทีที่ตรวจพบคนเดินเข้าใกล้ ไม่ต้องรอ server ตอบ
#define BUZZER_PIN  D7

// ===== ระยะที่ถือว่า "มีคนเดินเข้าใกล้" (เซนติเมตร) =====
// ถ้าวัดได้น้อยกว่าค่านี้ = มีวัตถุ/คนอยู่ในระยะนี้ = trigger แจ้งเตือน
#define ALERT_DISTANCE_CM   80

// ===== จังหวะเวลา =====
#define ULTRASONIC_POLL_INTERVAL_MS  300     // อ่านเซนเซอร์ทุก 300ms
#define ALERT_COOLDOWN_MS            10000   // กันแจ้งเตือนถี่เกินไปจากจุดเดียวกัน (10 วิ)
#define BUZZER_DURATION_MS           1000
#define CONFIG_PORTAL_TIMEOUT_MS     300000  // 5 นาที
#define MASTER_HEARTBEAT_INTERVAL_MS 30000   // มาสเตอร์เองก็ส่ง heartbeat ขึ้น server ทุก 30 วิ

// ===== โซนเวลาประเทศไทย =====
#define TZ_OFFSET_SEC  (7 * 3600)
#define TZ_DST         0

#endif
