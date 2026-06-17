# Health Monitor

ระบบติดตามสุขภาพผู้ป่วย — ESP8266 → FastAPI + PostgreSQL → Web Dashboard

## โครงสร้างโปรเจกต์

```
health-monitor/
├── main.py                  ← FastAPI entry point
├── requirements.txt
├── render.yaml              ← Deploy config สำหรับ Render.com
├── .env.example             ← copy เป็น .env แล้วแก้ค่า
├── app/
│   ├── config.py            ← Settings (อ่านจาก .env)
│   ├── models.py            ← SQLAlchemy models + DB session
│   ├── schemas.py           ← Pydantic schemas
│   ├── auth.py              ← JWT + password hashing
│   └── routes.py            ← API endpoints ทั้งหมด
├── frontend/
│   └── index.html           ← Single-page dashboard
└── esp8266/
    └── health_monitor.ino   ← Arduino sketch
```

## API Endpoints

| Method | Path | Auth | คำอธิบาย |
|--------|------|------|-----------|
| POST | `/api/auth/login` | — | รับ JWT token |
| GET | `/api/patients` | JWT | รายชื่อผู้ป่วย |
| POST | `/api/patients` | JWT | เพิ่มผู้ป่วย |
| GET | `/api/patients/{code}` | JWT | ข้อมูลผู้ป่วย |
| PATCH | `/api/patients/{code}` | JWT | แก้ไขข้อมูล |
| POST | `/api/device/data` | API Key | รับข้อมูลจาก ESP8266 |
| GET | `/api/dashboard/{code}/vitals` | JWT | สรุปค่าล่าสุด |
| GET | `/api/dashboard/{code}/chart?range=7d` | JWT | ข้อมูลกราฟ |

## รันในเครื่อง

```bash
# 1. ติดตั้ง dependencies
pip install -r requirements.txt

# 2. ตั้งค่า environment
cp .env.example .env
# แก้ DATABASE_URL, SECRET_KEY, ESP_API_KEY ใน .env

# 3. รัน server
uvicorn main:app --reload

# เปิด http://localhost:8000
# Swagger docs: http://localhost:8000/docs
# Login: admin / admin1234
```

## Deploy บน Render.com

1. Push โค้ดขึ้น GitHub
2. ไปที่ render.com → New → Blueprint
3. เลือก repo → Render จะอ่าน `render.yaml` อัตโนมัติ
4. ตั้งค่า `ESP_API_KEY` ใน Environment Variables
5. Deploy — ได้ URL เป็น `https://your-app.onrender.com`

## ตั้งค่า ESP8266

แก้ไขไฟล์ `esp8266/health_monitor.ino`:
```cpp
const char* WIFI_SSID    = "ชื่อ WiFi ของคุณ";
const char* WIFI_PASSWORD= "รหัส WiFi";
const char* SERVER_URL   = "https://your-app.onrender.com/api/device/data";
const char* API_KEY      = "ค่าเดียวกับ ESP_API_KEY ใน .env";
```

## Default credentials
- Web login: `admin` / `admin1234`  ← เปลี่ยนใน production!
- Patient code: `PT-2024-001`
