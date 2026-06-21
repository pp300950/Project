/*
  config.h — ค่าตั้งต้นของ ESP-01 (ฝั่งรองเท้า)

  ค่าพวกนี้คือ "ค่า default" ที่ใช้ตอนยังไม่เคยตั้งค่าผ่าน WiFi Config Portal
  เมื่อตั้งค่าผ่านพอร์ทัลแล้ว ค่าจริงจะถูกเก็บลง EEPROM แทนค่าพวกนี้
  (ดูรายละเอียด flow ใน README.md หัวข้อ "WiFi Config Portal")

  ไฟล์นี้ใช้ไฟล์เดียวกันกับรองเท้าซ้ายและขวา
  สิ่งที่ต่างกันระหว่าง 2 บอร์ดคือค่า DEVICE_ID เท่านั้น (ตั้งผ่านพอร์ทัล ไม่ต้องแก้โค้ด)
*/

#ifndef CONFIG_H
#define CONFIG_H

// ===== ค่า default ของ AP ฝั่ง ESP8266 มาสเตอร์ (ใช้ตอนยังไม่ตั้งค่าผ่านพอร์ทัล) =====
// ต้องตรงกับ AP_SSID / AP_PASSWORD ที่ตั้งไว้ในไฟล์ config.h ของ esp8266_master
#define DEFAULT_MASTER_AP_SSID     "ShoeMonitor-Master"
#define DEFAULT_MASTER_AP_PASSWORD "shoemonitor2024"

// ===== ค่า default device ID — ต้องตั้งให้ไม่ซ้ำกันระหว่างรองเท้าซ้าย/ขวา =====
// ถ้ายังไม่เคยตั้งค่าผ่านพอร์ทัล จะ fallback มาใช้ค่านี้
#define DEFAULT_DEVICE_ID "shoe01_left"   // อีกบอร์ดให้ตั้งพอร์ทัลเป็น "shoe01_right"

// ===== ขา GPIO ของ ESP-01 (มีให้ใช้จริงแค่ 2 ขานี้) =====
#define BUTTON_PIN 2   // GPIO2 — ปุ่มกดใต้รองเท้า (active-low, ใช้ INPUT_PULLUP)
// GPIO0 เว้นไว้ไม่ใช้ เพราะเป็นขาที่กำหนดโหมดบูต (ต้องลอยตัว/HIGH ตอนบูตปกติ)

// ===== Endpoint ปลายทางที่ ESP-01 ส่งสถานะไปหา (คือ ESP8266 มาสเตอร์ ไม่ใช่ FastAPI) =====
// IP นี้คือ IP เริ่มต้นของ ESP8266 ตอนปล่อย AP (ปกติ ESP8266 SoftAP จะได้ 192.168.4.1 เสมอ)
#define MASTER_STATUS_PATH   "/api/shoe-status"
#define MASTER_HOST_IP       "192.168.4.1"
#define MASTER_PORT          80

// ===== จังหวะเวลา =====
#define DEBOUNCE_MS          50      // หน่วงกันสัญญาณกระเพื่อมจากปุ่มกด
#define HEARTBEAT_INTERVAL_MS 15000  // ส่ง heartbeat ทุก 15 วิ แม้สถานะไม่เปลี่ยน
#define WIFI_RETRY_DELAY_MS  500
#define CONFIG_PORTAL_TIMEOUT_MS 300000  // 5 นาที — ตามที่ตกลงในเอกสารสถาปัตยกรรม

#endif
