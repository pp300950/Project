/*
  storage.h — จัดการอ่าน/เขียนค่าตั้งค่าถาวรของ ESP8266 มาสเตอร์ ลง EEPROM

  เก็บมากกว่า ESP-01 เพราะมาสเตอร์มีหน้าที่มากกว่า:
  - WiFi ของ AP (ที่ปล่อยให้ ESP-01 ต่อ) + WiFi STA (บ้าน/มือถือ)
  - URL ของ server ปลายทาง + cloud_mode on/off
  - LINE token/user id
*/

#ifndef STORAGE_H
#define STORAGE_H

#include <EEPROM.h>
#include "config.h"

#define EEPROM_SIZE         512
#define ADDR_MAGIC          0
#define ADDR_AP_SSID        1     // 32 ไบต์
#define ADDR_AP_PASSWORD    33    // 32 ไบต์
#define ADDR_STA_SSID       65    // 32 ไบต์
#define ADDR_STA_PASSWORD   97    // 32 ไบต์
#define ADDR_SERVER_URL     129   // 96 ไบต์ (URL อาจยาว)
#define ADDR_CLOUD_MODE     225   // 1 ไบต์ (0/1)
#define ADDR_LINE_TOKEN     226   // 200 ไบต์ (LINE token ค่อนข้างยาว)
#define ADDR_LINE_USER_ID   426   // 50 ไบต์

#define MAGIC_BYTE 0xB6

struct MasterConfig {
  String apSsid;
  String apPassword;
  String staSsid;
  String staPassword;
  String serverUrl;
  bool   cloudMode;
  String lineToken;
  String lineUserId;
};

String mEepromReadString(int startAddr, int maxLen) {
  char buf[201]; // รองรับสูงสุด ADDR_LINE_TOKEN (200 ไบต์) + 1
  int i = 0;
  for (; i < maxLen - 1; i++) {
    char c = EEPROM.read(startAddr + i);
    if (c == '\0' || c == 0xFF) break;
    buf[i] = c;
  }
  buf[i] = '\0';
  return String(buf);
}

void mEepromWriteString(int startAddr, int maxLen, const String& value) {
  int len = value.length();
  for (int i = 0; i < maxLen; i++) {
    if (i < len && i < maxLen - 1) {
      EEPROM.write(startAddr + i, value[i]);
    } else {
      EEPROM.write(startAddr + i, 0);
    }
  }
}

bool loadMasterConfig(MasterConfig &cfg) {
  EEPROM.begin(EEPROM_SIZE);
  byte magic = EEPROM.read(ADDR_MAGIC);

  if (magic != MAGIC_BYTE) {
    cfg.apSsid       = DEFAULT_AP_SSID;
    cfg.apPassword   = DEFAULT_AP_PASSWORD;
    cfg.staSsid      = DEFAULT_STA_SSID;
    cfg.staPassword  = DEFAULT_STA_PASSWORD;
    cfg.serverUrl    = DEFAULT_SERVER_URL;
    cfg.cloudMode    = DEFAULT_CLOUD_MODE;
    cfg.lineToken    = DEFAULT_LINE_TOKEN;
    cfg.lineUserId   = DEFAULT_LINE_USER_ID;
    EEPROM.end();
    return false;
  }

  cfg.apSsid      = mEepromReadString(ADDR_AP_SSID, 32);
  cfg.apPassword  = mEepromReadString(ADDR_AP_PASSWORD, 32);
  cfg.staSsid     = mEepromReadString(ADDR_STA_SSID, 32);
  cfg.staPassword = mEepromReadString(ADDR_STA_PASSWORD, 32);
  cfg.serverUrl   = mEepromReadString(ADDR_SERVER_URL, 96);
  cfg.cloudMode   = EEPROM.read(ADDR_CLOUD_MODE) == 1;
  cfg.lineToken   = mEepromReadString(ADDR_LINE_TOKEN, 200);
  cfg.lineUserId  = mEepromReadString(ADDR_LINE_USER_ID, 50);
  EEPROM.end();
  return true;
}

void saveMasterConfig(const MasterConfig &cfg) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(ADDR_MAGIC, MAGIC_BYTE);
  mEepromWriteString(ADDR_AP_SSID, 32, cfg.apSsid);
  mEepromWriteString(ADDR_AP_PASSWORD, 32, cfg.apPassword);
  mEepromWriteString(ADDR_STA_SSID, 32, cfg.staSsid);
  mEepromWriteString(ADDR_STA_PASSWORD, 32, cfg.staPassword);
  mEepromWriteString(ADDR_SERVER_URL, 96, cfg.serverUrl);
  EEPROM.write(ADDR_CLOUD_MODE, cfg.cloudMode ? 1 : 0);
  mEepromWriteString(ADDR_LINE_TOKEN, 200, cfg.lineToken);
  mEepromWriteString(ADDR_LINE_USER_ID, 50, cfg.lineUserId);
  EEPROM.commit();
  EEPROM.end();
}

#endif
