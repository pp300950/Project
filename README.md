# Smart Shoe Monitor — โมเดลจิ๋วสำหรับนำเสนอ

ระบบตรวจสอบผู้ป่วยผ่านรองเท้าอัจฉริยะ (ใส่/ถอดรองเท้า) + เซนเซอร์ตรวจจับการเดินเข้าใกล้จุดอันตราย (สระว่ายน้ำ/ประตู/รั้วบ้าน) แจ้งเตือนผ่าน LINE + บัซเซอร์ทันที

โมเดลจิ๋วนี้จำลอง 1 คู่รองเท้า + มาสเตอร์ 1 ตัว + mock server เพื่อทดสอบ flow การส่งข้อมูลทั้งระบบ ก่อนต่อ Supabase จริง

> โครงสร้างสถาปัตยกรรมแบบละเอียดอยู่ใน `smart-shoe-monitor-architecture.md` (ไฟล์ที่ออกแบบไว้ก่อนหน้า) — README นี้คือคู่มือลงมือทำจริงต่อจากเอกสารนั้น

---

## ภาพรวมโครงสร้างไฟล์

```
smart-shoe-monitor/
├── firmware/
│   ├── esp01_shoe/              ← ใช้ flash ทั้งรองเท้าซ้ายและขวา (ไฟล์เดียวกัน)
│   │   ├── esp01_shoe.ino
│   │   ├── config.h
│   │   ├── storage.h
│   │   └── config_portal.h
│   └── esp8266_master/          ← มาสเตอร์ตัวเดียว ทำ 4 หน้าที่พร้อมกัน
│       ├── esp8266_master.ino
│       ├── config.h
│       ├── storage.h
│       ├── ultrasonic.h
│       ├── notify.h
│       ├── server_client.h
│       ├── config_portal.h
│       └── mini_dashboard.h
├── mock-server/                 ← FastAPI mock — ทดสอบ flow ก่อนต่อ Supabase จริง
│   ├── mock_server.py
│   └── requirements.txt
├── smart-shoe-dashboard.html    ← เว็บแดชบอร์ดหลัก (ของเดิม ยังไม่ได้แก้ในรอบนี้)
└── README.md                    ← ไฟล์นี้
```

---

## 1. ตารางต่อสายไฟ — ครบทุกขา

### 1.1 ESP-01 (รองเท้า — ต่อแบบเดียวกันทั้งซ้ายและขวา)

ESP-01 มีขา GPIO ใช้งานได้จริงแค่ 2 ขา (GPIO0, GPIO2) ขาอื่นถูกใช้งานภายในหมดแล้ว

| ขา ESP-01 | ต่อไปที่ | หมายเหตุ |
|---|---|---|
| **GPIO2** | ปุ่มกดใต้รองเท้า ขาที่ 1 | ตั้งเป็น `INPUT_PULLUP` ในโค้ดแล้ว ไม่ต้องมีตัวต้านทานนอก |
| ปุ่มกดใต้รองเท้า ขาที่ 2 | **GND** | ปุ่มเหยียบ = ครบวงจรลง GND = อ่านได้ `LOW` = ใส่รองเท้าอยู่ |
| **VCC** | 3.3V เท่านั้น | **ห้ามต่อ 5V เด็ดขาด ESP-01 พังทันที** |
| **GND** | GND ร่วมกับแหล่งจ่ายไฟ | |
| **CH_PD (EN)** | 3.3V | ต้องต่อ HIGH เสมอ ไม่งั้นบอร์ดจะไม่บูต |
| **RST** | ไม่ต่อ (ลอยตัว) หรือต่อปุ่มกดแยกถ้าต้องการรีเซ็ตมือ | ไม่จำเป็นสำหรับการทำงานปกติ |
| **GPIO0** | ไม่ต่อกับอะไรในวงจรใช้งานจริง | ใช้ต่อ GND ชั่วคราว **เฉพาะตอนอัปโหลดโค้ด** เท่านั้น (ดูหัวข้อ 3.1) |

**คำเตือนสำคัญ:** ESP-01 ไม่มีวงจรแปลงไฟ/USB ในตัว ต้องมีแหล่งจ่าย 3.3V ที่จ่ายกระแสได้นิ่งพอ (อย่างน้อย 250mA ตอนบูต/ส่ง WiFi) — ถ้าใช้ตัวแปลง 3.3V จากโมดูล AMS1117 จากไฟ 5V ทั่วไป มักไม่พอ แนะนำใช้ USB-to-TTL ที่มีขา 3.3V แยก หรือ regulator เฉพาะที่จ่ายกระแสได้สูง

### 1.2 ESP8266 มาสเตอร์ (ตัวกลาง — ใช้ NodeMCU/Wemos D1 mini ที่มีขา D0–D8)

| ขา ESP8266 (label บอร์ด) | ต่อไปที่ | หมายเหตุ |
|---|---|---|
| **D1** | HC-SR04 ตัวที่ 1 (สระว่ายน้ำ) — ขา **TRIG** | |
| **D2** | HC-SR04 ตัวที่ 1 (สระว่ายน้ำ) — ขา **ECHO** | ผ่าน voltage divider ก่อนเข้า D2 (ดูหมายเหตุด้านล่าง) |
| **D3** | HC-SR04 ตัวที่ 2 (ประตู/ทางออก) — ขา **TRIG** | |
| **D4** | HC-SR04 ตัวที่ 2 (ประตู/ทางออก) — ขา **ECHO** | ผ่าน voltage divider |
| **D5** | HC-SR04 ตัวที่ 3 (รั้วบ้าน) — ขา **TRIG** | |
| **D6** | HC-SR04 ตัวที่ 3 (รั้วบ้าน) — ขา **ECHO** | ผ่าน voltage divider |
| **D7** | ขา **+** ของบัซเซอร์ | |
| **GND** (ขาใดก็ได้) | ขา **−** ของบัซเซอร์ | |
| **5V / VIN** | ขา **VCC** ของ HC-SR04 ทั้ง 3 ตัว | HC-SR04 ต้องใช้ไฟ 5V ถึงจะวัดระยะได้แม่นยำ (ต่างจาก ESP-01 ที่ใช้ 3.3V) |
| **GND** | ขา **GND** ของ HC-SR04 ทั้ง 3 ตัว และบัซเซอร์ | รวม GND ทุกอุปกรณ์เข้าจุดเดียวกัน |

**คำเตือนสำคัญเรื่อง ECHO pin:** ขา ECHO ของ HC-SR04 ส่งสัญญาณออกมาที่ **5V** แต่ขา GPIO ของ ESP8266 รับได้สูงสุดแค่ **3.3V** — ถ้าต่อ ECHO ตรงเข้า GPIO โดยไม่มีตัวลดแรงดัน **เสี่ยงขา GPIO ไหม้** ต้องทำ voltage divider ก่อนทุกตัว:

```
HC-SR04 ECHO ──[ตัวต้านทาน 1kΩ]──┬── เข้าขา D2/D4/D6 ของ ESP8266
                                  │
                            [ตัวต้านทาน 2kΩ]
                                  │
                                 GND
```
(สัดส่วน 1kΩ:2kΩ จะลดแรงดันจาก 5V เหลือประมาณ 3.3V พอดี — ทำซ้ำแบบเดียวกันทั้ง 3 ตัว)

### 1.3 สรุปแหล่งจ่ายไฟทั้งระบบ

| อุปกรณ์ | แรงดันที่ต้องการ |
|---|---|
| ESP-01 x2 | 3.3V (แยกจาก HC-SR04) |
| ESP8266 มาสเตอร์ (NodeMCU) | 5V ผ่าน USB หรือขา VIN (มี regulator ในตัวแปลงเป็น 3.3V ให้ชิปอยู่แล้ว) |
| HC-SR04 x3 | 5V (ดึงจากขา 5V/VIN ของมาสเตอร์ได้เลยถ้าแหล่งจ่ายแรงพอ) |
| บัซเซอร์ | 3.3-5V (แบบ active buzzer ทั่วไปใช้ได้กับ 3.3V จาก D7 ของ ESP8266 ตรงๆ ไม่ต้องมี driver เพิ่ม) |

---

## 2. การติดตั้ง Arduino IDE และ Library

### 2.1 ติดตั้ง ESP8266 board package

1. เปิด Arduino IDE → **File → Preferences** → ช่อง "Additional Boards Manager URLs" ใส่:
   ```
   https://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
2. **Tools → Board → Boards Manager** → ค้นหา `esp8266` → กด Install (ใช้เวอร์ชัน 3.x)

### 2.2 Library ที่ต้องติดตั้ง (Sketch → Include Library → Manage Libraries)

| Library | เวอร์ชันที่แนะนำ | ใช้ในไฟล์ |
|---|---|---|
| **ArduinoJson** | **6.21.x** (สำคัญ: ห้ามใช้ v7 เพราะ API เปลี่ยน `createNestedArray`/`createNestedObject` จะ error) | `mini_dashboard.h`, `esp8266_master.ino` |
| ESP8266WiFi, ESP8266HTTPClient, ESP8266WebServer, WiFiClientSecure, EEPROM | มาพร้อม board package อยู่แล้ว ไม่ต้องติดตั้งเพิ่ม | ทุกไฟล์ |

### 2.3 ตั้งค่า Board ก่อนอัปโหลด

**สำหรับ ESP-01:**
- Tools → Board → `Generic ESP8266 Module`
- Flash Size: `1M (FS:none)` (ESP-01 รุ่นเก่ามี flash 512KB-1MB)
- Upload Speed: `115200` (ESP-01 ผ่าน USB-TTL บางตัวไม่รองรับความเร็วสูง)

**สำหรับ ESP8266 มาสเตอร์ (NodeMCU/Wemos D1):**
- Tools → Board → `NodeMCU 1.0 (ESP-12E Module)`
- Upload Speed: `115200` หรือ `921600` ก็ได้ตามความเสถียรของสาย USB

---

## 3. วิธีอัปโหลดโค้ด

### 3.1 ESP-01 (ขั้นตอนเฉพาะที่มักพลาด)

ESP-01 ไม่มี USB ในตัว ต้องใช้ USB-to-TTL adapter (เช่น CP2102, CH340) ต่อดังนี้:

| USB-TTL | ESP-01 |
|---|---|
| 3.3V | VCC + CH_PD |
| GND | GND |
| TX | RXD (ของ ESP-01) |
| RX | TXD (ของ ESP-01) |

**ก่อนกดอัปโหลด ต้องเข้าโหมด flash:**
1. ต่อ **GPIO0 ลง GND** ชั่วคราว (ใช้สายจัมเปอร์มือถือก็ได้)
2. กดปุ่ม reset ของบอร์ด (หรือถอด-เสียบไฟใหม่) ขณะที่ GPIO0 ยังต่อ GND อยู่
3. กดอัปโหลดในมือ Arduino IDE
4. อัปโหลดเสร็จแล้ว **ถอดสาย GPIO0-GND ออก** แล้วรีเซ็ตอีกครั้งเพื่อรันโหมดปกติ

> เปิด flash ครั้งแรกให้ตั้ง `DEFAULT_DEVICE_ID` ใน `config.h` เป็น `"shoe01_left"` ก่อน flash ตัวแรก แล้วแก้เป็น `"shoe01_right"` ก่อน flash ตัวที่สอง (หรือจะ flash ค่าเดียวกันแล้วไปตั้งค่าจริงผ่าน Config Portal ทีหลังก็ได้ — สะดวกกว่าถ้าจะ flash ทีละหลายตัว)

### 3.2 ESP8266 มาสเตอร์

เสียบ USB เข้าคอมได้เลย (มี USB ในตัว) เลือก Board ถูกแล้วกด Upload ตามปกติ

---

## 4. ลำดับการทดสอบระบบ (แนะนำให้ทำตามนี้)

1. **Flash มาสเตอร์ก่อน** เปิดเครื่อง → เปิด Serial Monitor (115200 baud) ดู log
   - มาสเตอร์จะปล่อย AP ชื่อ `ShoeMonitor-Master` ทันที (ค่า default)
   - ลองต่อ WiFi นี้จากมือถือ แล้วเข้า `http://192.168.4.1/` จะเห็น mini dashboard

2. **Flash ESP-01 ทั้งสองตัว** ตั้ง device_id ให้ต่างกัน (`shoe01_left` / `shoe01_right`)
   - เปิดเครื่อง ESP-01 จะพยายามต่อ AP ของมาสเตอร์อัตโนมัติ (ใช้ค่า default ที่ตรงกัน)
   - ถ้าเชื่อมต่อสำเร็จ จะเห็นสถานะอัปเดตใน mini dashboard ของมาสเตอร์

3. **ทดสอบกดปุ่มใต้รองเท้า** — ดู mini dashboard ว่าสถานะ "ใส่รองเท้าอยู่"/"ถอดรองเท้า" เปลี่ยนตามจริงไหม

4. **รัน mock server** บนคอม (ดูหัวข้อ 5) แล้วตั้งค่า `server_url` ในมาสเตอร์ให้ชี้มาที่ IP ของคอมเครื่องนั้น ผ่าน Config Portal (`http://<IP มาสเตอร์>/config`)

5. **ทดสอบ ultrasonic** — เอามือ/วัตถุเข้าใกล้ HC-SR04 ตัวใดตัวหนึ่งในระยะ < 80cm ดูว่า:
   - บัซเซอร์ดังทันที (ไม่รอ network)
   - มี log ปรากฏใน mock server console
   - ได้รับ LINE message (ถ้าตั้ง LINE token/user id ถูกต้อง)

---

## 5. การรัน Mock Server (FastAPI)

ใช้ทดสอบ flow การส่งข้อมูลจากบอร์ดให้ครบก่อนไปต่อ Supabase จริง — ข้อมูลเก็บใน memory เท่านั้น (รีสตาร์ทแล้วหาย)

```bash
cd mock-server
pip install -r requirements.txt
uvicorn mock_server:app --host 0.0.0.0 --port 8000 --reload
```

**สำคัญ:** คอมที่รัน mock server กับ ESP8266 มาสเตอร์ ต้องอยู่ใน **WiFi วงเดียวกัน** (วง STA/บ้าน ไม่ใช่วง AP ของมาสเตอร์) เพราะมาสเตอร์ส่งข้อมูลออกทาง STA

หาค่า IP ของคอม:
- Windows: `ipconfig` ดูที่ `IPv4 Address`
- Mac/Linux: `ifconfig` หรือ `ip addr` ดูที่ interface WiFi

แล้วเอา IP นั้นไปตั้งใน `server_url` ผ่าน `http://<IP มาสเตอร์>/config` เช่น `http://192.168.1.50:8000`

ดูข้อมูลที่ส่งเข้ามาทั้งหมดผ่านเบราว์เซอร์ได้ที่ `http://<IP คอม>:8000/api/v1/events` หรือเปิด `http://<IP คอม>:8000/docs` (FastAPI สร้าง Swagger UI ให้อัตโนมัติ ทดสอบยิง request ผ่านหน้าเว็บได้เลยไม่ต้องเขียน curl)

---

## 6. ตั้งค่า Supabase (สำหรับขั้นต่อไป — ยังไม่ได้ต่อจริงในโค้ดรอบนี้)

โค้ดรอบนี้เป็นแค่ mock server ที่เก็บข้อมูลใน memory เพื่อทดสอบ flow การส่งจากบอร์ด ขั้นต่อไปคือเปลี่ยน mock server ให้เขียนข้อมูลลง Supabase จริง ทำตามนี้:

### 6.1 สร้างโปรเจค Supabase

1. ไปที่ [supabase.com](https://supabase.com) → Sign up / Login
2. กด **New Project** → ตั้งชื่อโปรเจค → เลือก region ใกล้ที่สุด (เช่น Singapore สำหรับใช้งานในไทย) → ตั้ง Database Password (เก็บไว้ดีๆ ใช้ต่อ connection string)
3. รอสร้างโปรเจคเสร็จ (~2 นาที)

### 6.2 สร้างตารางตามที่ออกแบบไว้ในเอกสารสถาปัตยกรรม

ไปที่ **SQL Editor** ในเมนูซ้ายของ Supabase แล้วรัน SQL ประมาณนี้ (ปรับรายละเอียดตามต้องการ):

```sql
-- ตารางผู้ป่วย (field ที่ระบุตัวตนได้เก็บเป็น ciphertext ของ AES-256-GCM)
create table patients (
  id uuid primary key default gen_random_uuid(),
  bed_number text not null,
  encrypted_name bytea not null,      -- ciphertext จาก AES-256-GCM
  encryption_iv bytea not null,       -- IV/nonce เฉพาะของแถวนี้ (ต้องไม่ซ้ำกันทุกแถว)
  created_at timestamptz default now()
);

-- ตารางอุปกรณ์ (ESP-01 ซ้าย/ขวา + มาสเตอร์)
create table devices (
  device_id text primary key,
  patient_id uuid references patients(id),
  device_type text not null,          -- 'shoe_left' / 'shoe_right' / 'master'
  last_seen_at timestamptz
);

-- ตารางเหตุการณ์ (shoe_status, zone_intrusion, system_online, ฯลฯ)
create table events (
  id bigint generated always as identity primary key,
  event_type text not null,
  device_id text references devices(device_id),
  zone_id text,
  distance_cm numeric,
  foot_in_shoe boolean,
  occurred_at timestamptz not null,
  received_at timestamptz default now()
);

-- ตารางบัญชี caregiver/หมอ
create table users (
  id uuid primary key default gen_random_uuid(),
  username text unique not null,
  hashed_password text not null,
  role text not null,                 -- เช่น 'doctor', 'nurse', 'admin'
  created_at timestamptz default now()
);

-- ตาราง audit log (บันทึกทุกครั้งที่มีการเข้าถึงข้อมูลผู้ป่วย)
create table audit_log (
  id bigint generated always as identity primary key,
  user_id uuid references users(id),
  patient_id uuid references patients(id),
  action text not null,               -- เช่น 'view_patient_name', 'decrypt_record'
  occurred_at timestamptz default now()
);
```

### 6.3 หา Connection String

1. ไปที่ **Project Settings → Database**
2. คัดลอกค่าใน **Connection string** (เลือกแบบ "URI", โหมด "Transaction" สำหรับ serverless อย่าง Render)
3. รูปแบบจะเป็นประมาณ:
   ```
   postgresql://postgres.[project-ref]:[password]@aws-0-[region].pooler.supabase.com:6543/postgres
   ```

### 6.4 ตั้งค่าฝั่ง FastAPI (ตอนเปลี่ยนจาก mock เป็นของจริง)

```bash
pip install asyncpg sqlalchemy python-dotenv cryptography
```

เก็บ connection string และ encryption key ไว้ใน `.env` (ห้าม commit ขึ้น Git — เพิ่มไฟล์นี้ใน `.gitignore` ด้วย):

```env
DATABASE_URL=postgresql://postgres.xxxxx:yourpassword@aws-0-ap-southeast-1.pooler.supabase.com:6543/postgres
ENCRYPTION_KEY=<32-byte key สำหรับ AES-256 — generate ด้วย python -c "import os; print(os.urandom(32).hex())">
```

### 6.5 Deploy ขึ้น Render

1. Push โค้ด FastAPI (ของจริง ไม่ใช่ mock) ขึ้น GitHub repo
2. ไปที่ [render.com](https://render.com) → **New → Web Service** → เชื่อม GitHub repo
3. Build Command: `pip install -r requirements.txt`
4. Start Command: `uvicorn main:app --host 0.0.0.0 --port $PORT`
5. ไปที่ **Environment** → เพิ่ม `DATABASE_URL` และ `ENCRYPTION_KEY` เป็น environment variables (ค่าเดียวกับใน `.env` แต่ไม่ commit ไฟล์ขึ้น Git)

> รายละเอียดเรื่องการเข้ารหัส AES-256-GCM, RBAC, audit log flow ดูได้ในเอกสาร `smart-shoe-monitor-architecture.md` หัวข้อ "ชั้นที่ 4: ความปลอดภัยข้อมูลผู้ป่วย" — ส่วนนั้นออกแบบไว้ละเอียดแล้ว ขาดแค่ลงโค้ดจริงในรอบหน้า

---

## 7. ขอบเขตของโมเดลจิ๋วนี้ (รู้ไว้ก่อนนำเสนอ)

- **ตอนนี้ทำเสร็จ:** ESP-01 x2, ESP8266 มาสเตอร์ (AP+STA, ultrasonic x3, buzzer, LINE, mini dashboard), mock FastAPI server
- **ยังไม่ทำในรอบนี้:** การต่อ Supabase จริง, การเข้ารหัส AES-256-GCM, ระบบ login/RBAC, audit log, โหมด local-only ผ่าน Termux
- เซนเซอร์ที่ใช้จริงคือ **HC-SR04** (ไม่ใช่ digital 0/1 แบบโค้ดต้นแบบเดิม) ต้องมี voltage divider ที่ขา ECHO ทุกตัว (ดูหัวข้อ 1.2)
- LINE token/user id ที่ใส่มาในไฟล์ `config.h` เป็นค่าตัวอย่างจากโค้ดต้นแบบเดิม — ควรเปลี่ยนเป็นของจริงผ่าน Config Portal แทนการแก้ในไฟล์ตรงๆ เพื่อไม่ให้ค่าจริงหลุดไปกับซอร์สโค้ดที่อาจแชร์ต่อ
