# Smart Shoe Monitor — คู่มือตั้งค่าฐานข้อมูลและ Deploy

---

## 1. ตั้งค่า Supabase (ฐานข้อมูล)

### 1.1 สร้างโปรเจกต์

1. ไปที่ https://supabase.com → **New project**
2. ตั้งชื่อ เช่น `smart-shoe-monitor`
3. ตั้ง **Database Password** (จด password ไว้ — จะใช้ใน connection string)
4. เลือก Region: **Southeast Asia (Singapore)**
5. รอประมาณ 1–2 นาทีจนพร้อม

---

### 1.2 ดึง Connection String

1. ใน Supabase Dashboard → **Settings** → **Database**
2. เลื่อนลงหาหัวข้อ **Connection string** → เลือก tab **URI**
3. Copy string ที่ได้ รูปแบบ:

```
postgresql://postgres:[YOUR-PASSWORD]@db.xxxxxxxxxxxx.supabase.co:5432/postgres
```

4. แทน `[YOUR-PASSWORD]` ด้วย password ที่ตั้งไว้ใน 1.1

> ⚠️ ถ้า password มีอักขระพิเศษ เช่น `@` `#` `%` ให้ URL-encode ก่อน
> เช่น `@` → `%40`

---

### 1.3 ตารางฐานข้อมูล

**ไม่ต้องสร้างตารางด้วยมือ** — `main.py` จะรัน `init_db()` ตอน startup แล้วสร้างให้อัตโนมัติ

ถ้าอยากดูหรือสร้างเองด้วย SQL ก็ได้ (ไปที่ **SQL Editor** ใน Supabase):

```sql
-- ตาราง devices
CREATE TABLE IF NOT EXISTS devices (
    device_id   TEXT PRIMARY KEY,
    label       TEXT,
    location    TEXT,
    registered  TIMESTAMPTZ DEFAULT now()
);

-- ตาราง events
CREATE TABLE IF NOT EXISTS events (
    id          BIGSERIAL PRIMARY KEY,
    device_id   TEXT REFERENCES devices(device_id),
    event_type  TEXT NOT NULL,
    zone        TEXT,
    distance_cm REAL,
    shoe_left   BOOLEAN,
    shoe_right  BOOLEAN,
    step_count  INT,
    raw_json    JSONB,
    ts          TIMESTAMPTZ DEFAULT now()
);
```

---

## 2. Deploy บน Render

### 2.1 เตรียม Repository

โครงสร้างไฟล์ที่ต้องมีใน repo:

```
smart-shoe-monitor/
├── main.py
├── requirements.txt
└── (frontend/ ทีหลัง)
```

Push ขึ้น GitHub (public หรือ private ก็ได้)

---

### 2.2 สร้าง Web Service บน Render

1. ไปที่ https://render.com → **New** → **Web Service**
2. เชื่อม GitHub repo ที่เตรียมไว้
3. ตั้งค่า:

| ฟิลด์ | ค่า |
|---|---|
| **Name** | `smart-shoe-monitor` |
| **Runtime** | `Python 3` |
| **Build Command** | `pip install -r requirements.txt` |
| **Start Command** | `uvicorn main:app --host 0.0.0.0 --port $PORT` |
| **Instance Type** | Free (หรือ Starter $7/เดือน ถ้าต้องการไม่ sleep) |

---

### 2.3 ตั้ง Environment Variables

ใน Render → **Environment** → **Add Environment Variable**

| Key | Value | หมายเหตุ |
|---|---|---|
| `SUPABASE_DB_URL` | `postgresql://postgres:...` | Connection string จากข้อ 1.2 |
| `SECRET_KEY` | `abc123xyz...` | สุ่ม string ยาว ๆ ใช้สำหรับ JWT |
| `ADMIN_USERNAME` | `admin` | ชื่อผู้ใช้ login หน้าเว็บ |
| `ADMIN_PASSWORD` | `your_password` | รหัสผ่าน (ตั้งให้ยากหน่อย) |

> 💡 สร้าง SECRET_KEY แบบสุ่มได้ด้วย:
> ```bash
> python -c "import secrets; print(secrets.token_hex(32))"
> ```

---

### 2.4 Deploy

1. กด **Create Web Service**
2. Render จะ build และ deploy อัตโนมัติ
3. รอประมาณ 2–3 นาที
4. เข้า URL ที่ Render ให้มา เช่น `https://smart-shoe-monitor.onrender.com`
5. ลอง GET `/health` → ควรได้ `{"status": "ok"}`
6. เข้า `/docs` → เห็น Swagger UI ของ API ทั้งหมด

> ⚠️ Free tier บน Render จะ **sleep หลังไม่มีคนใช้ 15 นาที**
> และใช้เวลา ~30 วินาทีตอน wake up ครั้งแรก

---

## 3. ทดสอบด้วย test_sensor.py

### ติดตั้ง dependency

```bash
pip install requests
```

### รันแบบ interactive

```bash
python test_sensor.py
```

จะถามว่าจะทดสอบ mode ไหน:
- **1** — ยิงครบทุก event type ครั้งละ 1 ครั้ง
- **2** — ยิงต่อเนื่อง 20 ครั้ง (ทุก 2 วินาที) จำลอง sensor loop
- **3** — ยิงเหตุการณ์วิกฤต เช็คว่าระบบบันทึกได้ถูกต้อง
- **4** — Stress test 50 requests ติดกัน

### รันแบบระบุ argument โดยตรง

```bash
# ทดสอบกับ server จริงบน Render
python test_sensor.py --url https://smart-shoe-monitor.onrender.com --mode 1

# ระบุ device_id เอง
python test_sensor.py --url http://localhost:8000 --device esp8266_ward_b --mode 2
```

---

## 4. สรุปโครงสร้างข้อมูลที่ส่งจาก ESP8266

### Payload ปัจจุบัน (ระบบทดสอบ)

```json
POST /api/event
{
  "device_id":   "esp8266_ward_a",
  "event_type":  "zone_breach",
  "zone":        "fence",
  "distance_cm": 11.4
}
```

### Payload อนาคต (หลังติดปุ่มใต้ลองเท้า)

```json
POST /api/event
{
  "device_id":   "esp8266_ward_a",
  "event_type":  "step_data",
  "shoe_left":   true,
  "shoe_right":  true,
  "step_count":  42,
  "raw_json": {
    "raw_left":    1,
    "raw_right":   1,
    "interval_ms": 980
  }
}
```

> ไม่ต้องแก้ schema DB เพิ่ม — field เหล่านี้ถูก reserve ไว้ใน `events` table แล้ว

---

## 5. Local Development (ทดสอบบนเครื่องตัวเอง)

```bash
# สร้าง .env
SUPABASE_DB_URL=postgresql://postgres:password@db.xxx.supabase.co:5432/postgres
SECRET_KEY=dev_secret_key_12345
ADMIN_USERNAME=admin
ADMIN_PASSWORD=admin1234

# รัน server
pip install -r requirements.txt
uvicorn main:app --reload --port 8000

# ทดสอบ
python test_sensor.py --url http://localhost:8000 --mode 1
```
