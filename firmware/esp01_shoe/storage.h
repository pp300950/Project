/*
  storage.h — จัดการอ่าน/เขียนค่าตั้งค่าถาวรของ ESP-01 ลง EEPROM

  เหตุผลที่ต้องมีไฟล์นี้:
  ESP-01 ไม่มีปุ่ม reset ที่กดง่าย และไม่มีจอ ดังนั้นค่าที่ตั้งผ่าน
  Config Portal (SSID/Password ของมาสเตอร์, Device ID, Token แจ้งเตือน)
  ต้องเก็บไว้ใน EEPROM (flash จำลอง) เพื่อให้รอดพ้นการรีสตาร์ท/ไฟดับ
*/

#ifndef STORAGE_H
#define STORAGE_H

#include <EEPROM.h>
#include "config.h"

// ===== โครงสร้างหน่วยความจำ EEPROM =====
// ขนาดรวมต้องไม่เกิน 512 ไบต์ (จำกัดของ EEPROM.begin บน ESP8266/ESP-01)
#define EEPROM_SIZE       512
#define ADDR_MAGIC        0      // 1 ไบต์ — เลขเวทมนตร์เช็คว่าเคยเขียนค่าหรือยัง
#define ADDR_DEVICE_ID    1      // 32 ไบต์ — เช่น "shoe01_left"
#define ADDR_AP_SSID      33     // 32 ไบต์ — SSID ของ ESP8266 มาสเตอร์
#define ADDR_AP_PASSWORD  65     // 32 ไบต์ — Password ของ ESP8266 มาสเตอร์
#define ADDR_NOTIFY_TOKEN 97     // 64 ไบต์ — เผื่ออนาคตสลับช่องแจ้งเตือน (ปัจจุบันยังไม่ใช้ฝั่ง ESP-01)

#define MAGIC_BYTE 0xA5   // ค่าเฉพาะที่ใช้เช็คว่า EEPROM นี้เคยถูกตั้งค่าจากพอร์ทัลแล้ว

struct DeviceConfig {
  String deviceId;
  String apSsid;
  String apPassword;
  String notifyToken;
};

// อ่าน string ความยาวคงที่จาก EEPROM (คั่นด้วย null terminator)
String eepromReadString(int startAddr, int maxLen) {
  char buf[65]; // รองรับสูงสุด ADDR_NOTIFY_TOKEN (64 ไบต์) + 1
  int i = 0;
  for (; i < maxLen - 1; i++) {
    char c = EEPROM.read(startAddr + i);
    if (c == '\0' || c == 0xFF) break;
    buf[i] = c;
  }
  buf[i] = '\0';
  return String(buf);
}

void eepromWriteString(int startAddr, int maxLen, const String& value) {
  int len = value.length();
  for (int i = 0; i < maxLen; i++) {
    if (i < len && i < maxLen - 1) {
      EEPROM.write(startAddr + i, value[i]);
    } else {
      EEPROM.write(startAddr + i, 0);
    }
  }
}

bool loadConfig(DeviceConfig &cfg) {
  EEPROM.begin(EEPROM_SIZE);
  byte magic = EEPROM.read(ADDR_MAGIC);

  if (magic != MAGIC_BYTE) {
    // ยังไม่เคยตั้งค่าผ่านพอร์ทัล -> ใช้ค่า default จาก config.h
    cfg.deviceId    = DEFAULT_DEVICE_ID;
    cfg.apSsid      = DEFAULT_MASTER_AP_SSID;
    cfg.apPassword  = DEFAULT_MASTER_AP_PASSWORD;
    cfg.notifyToken = "";
    EEPROM.end();
    return false; // false = ใช้ค่า default อยู่ ยังไม่เคยตั้งค่าเอง
  }

  cfg.deviceId    = eepromReadString(ADDR_DEVICE_ID, 32);
  cfg.apSsid      = eepromReadString(ADDR_AP_SSID, 32);
  cfg.apPassword  = eepromReadString(ADDR_AP_PASSWORD, 32);
  cfg.notifyToken = eepromReadString(ADDR_NOTIFY_TOKEN, 64);
  EEPROM.end();
  return true;
}

void saveConfig(const DeviceConfig &cfg) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(ADDR_MAGIC, MAGIC_BYTE);
  eepromWriteString(ADDR_DEVICE_ID, 32, cfg.deviceId);
  eepromWriteString(ADDR_AP_SSID, 32, cfg.apSsid);
  eepromWriteString(ADDR_AP_PASSWORD, 32, cfg.apPassword);
  eepromWriteString(ADDR_NOTIFY_TOKEN, 64, cfg.notifyToken);
  EEPROM.commit();
  EEPROM.end();
}

#endif
