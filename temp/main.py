"""
Smart Shoe Monitor — FastAPI Backend
Deploy: Render (Web Service)
Database: Supabase (PostgreSQL via psycopg2)

ENV ที่ต้องตั้งใน Render Dashboard:
  SUPABASE_DB_URL   = postgresql://postgres:[password]@db.xxxx.supabase.co:5432/postgres
  SECRET_KEY        = (random string สำหรับ JWT)
  ADMIN_USERNAME    = admin
  ADMIN_PASSWORD    = your_password

Start command ใน Render:
  uvicorn main:app --host 0.0.0.0 --port $PORT
"""

import os
import hmac
import hashlib
import json
from datetime import datetime, timedelta, timezone
from typing import Optional, List, Literal

import psycopg2
import psycopg2.extras
from fastapi import FastAPI, HTTPException, Depends, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
import jwt  # pip install PyJWT

# ──────────────────────────────────────────────
# Config
# ──────────────────────────────────────────────

SECRET_KEY     = os.getenv("SECRET_KEY", "changeme_in_production")
ALGORITHM      = "HS256"
TOKEN_EXPIRE_H = 12

ADMIN_USERNAME = os.getenv("ADMIN_USERNAME", "admin")
ADMIN_PASSWORD = os.getenv("ADMIN_PASSWORD", "admin1234")

DB_URL     = os.getenv("SUPABASE_DB_URL", "")        # postgresql://...

TZ_BKK = timezone(timedelta(hours=7))

# ──────────────────────────────────────────────
# FastAPI App
# ──────────────────────────────────────────────

app = FastAPI(
    title="Smart Shoe Monitor API",
    version="1.0.0",
    description="Backend สำหรับระบบเฝ้าระวังรองเท้าอัจฉริยะ",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],   # ปรับเป็น domain จริงตอน production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

security = HTTPBearer()

# ──────────────────────────────────────────────
# Database helpers
# ──────────────────────────────────────────────

def get_conn():
    """เปิด connection ใหม่ต่อ 1 request (Supabase ปิด idle ไว)"""
    if not DB_URL:
        raise HTTPException(503, "SUPABASE_DB_URL ยังไม่ได้ตั้งค่า")
    return psycopg2.connect(DB_URL, cursor_factory=psycopg2.extras.RealDictCursor)


def init_db():
    """สร้างตารางถ้ายังไม่มี — รันตอน startup"""
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            # ตาราง devices — 1 แถวต่อ 1 ESP8266 master
            cur.execute("""
                CREATE TABLE IF NOT EXISTS devices (
                    device_id   TEXT PRIMARY KEY,
                    label       TEXT,
                    location    TEXT,
                    registered  TIMESTAMPTZ DEFAULT now()
                );
            """)

            # ตาราง events — log ทุก event จากอุปกรณ์
            cur.execute("""
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
            """)

        conn.commit()
    finally:
        conn.close()


@app.on_event("startup")
def on_startup():
    try:
        init_db()
        print("[DB] Tables ready")
    except Exception as e:
        print(f"[DB] init_db failed: {e}")


# ──────────────────────────────────────────────
# Auth (JWT)
# ──────────────────────────────────────────────

def create_token(username: str) -> str:
    expire = datetime.now(TZ_BKK) + timedelta(hours=TOKEN_EXPIRE_H)
    return jwt.encode({"sub": username, "exp": expire}, SECRET_KEY, algorithm=ALGORITHM)


def verify_token(credentials: HTTPAuthorizationCredentials = Depends(security)) -> str:
    try:
        payload = jwt.decode(credentials.credentials, SECRET_KEY, algorithms=[ALGORITHM])
        return payload["sub"]
    except jwt.ExpiredSignatureError:
        raise HTTPException(401, "Token หมดอายุ")
    except jwt.PyJWTError:
        raise HTTPException(401, "Token ไม่ถูกต้อง")


# ──────────────────────────────────────────────
# Pydantic schemas
# ──────────────────────────────────────────────

class LoginRequest(BaseModel):
    username: str
    password: str


class DeviceRegister(BaseModel):
    device_id: str = Field(..., example="esp8266_ward_a")
    label: str     = Field(..., example="หอผู้ป่วย A - เตียง 3")
    location: str  = Field("", example="สระน้ำ")


class SensorEvent(BaseModel):
    """
    Payload ที่ ESP8266 ส่งมา (POST /api/event)
    ปัจจุบัน: ส่งเฉพาะ zone + distance
    อนาคต:    เพิ่ม shoe_left/right + step_count จาก ESP-01
    """
    device_id:   str
    event_type:  Literal["zone_breach", "shoe_removed", "step_data", "heartbeat"]
    zone:        Optional[str]   = None   # "fence" | "pool"
    distance_cm: Optional[float] = None
    shoe_left:   Optional[bool]  = None   # future
    shoe_right:  Optional[bool]  = None   # future
    step_count:  Optional[int]   = None   # future
    raw_json:    Optional[dict]  = None   # raw payload จาก firmware เผื่อ debug


class EventFilter(BaseModel):
    device_id:  Optional[str]  = None
    event_type: Optional[str]  = None
    zone:       Optional[str]  = None
    from_ts:    Optional[str]  = None   # ISO8601
    to_ts:      Optional[str]  = None



# ──────────────────────────────────────────────
# Routes — Auth
# ──────────────────────────────────────────────

@app.post("/api/login", tags=["Auth"])
def login(body: LoginRequest):
    """Login ด้วย username/password — ได้ JWT กลับมา"""
    if body.username != ADMIN_USERNAME or body.password != ADMIN_PASSWORD:
        raise HTTPException(401, "ชื่อผู้ใช้หรือรหัสผ่านไม่ถูกต้อง")
    token = create_token(body.username)
    return {"access_token": token, "token_type": "bearer", "expires_in_hours": TOKEN_EXPIRE_H}


@app.get("/api/me", tags=["Auth"])
def me(user: str = Depends(verify_token)):
    return {"username": user}


# ──────────────────────────────────────────────
# Routes — Devices
# ──────────────────────────────────────────────

@app.post("/api/devices", tags=["Devices"], status_code=201)
def register_device(body: DeviceRegister, user: str = Depends(verify_token)):
    """ลงทะเบียนอุปกรณ์ใหม่ (upsert)"""
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                INSERT INTO devices(device_id, label, location)
                VALUES(%s, %s, %s)
                ON CONFLICT(device_id) DO UPDATE
                  SET label=EXCLUDED.label, location=EXCLUDED.location
            """, (body.device_id, body.label, body.location))
        conn.commit()
    finally:
        conn.close()
    return {"device_id": body.device_id, "message": "registered"}


@app.get("/api/devices", tags=["Devices"])
def list_devices(user: str = Depends(verify_token)):
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            cur.execute("SELECT * FROM devices ORDER BY registered DESC")
            return cur.fetchall()
    finally:
        conn.close()


# ──────────────────────────────────────────────
# Routes — Sensor Events (ฝั่ง Hardware POST มา)
# ──────────────────────────────────────────────

@app.post("/api/event", tags=["Sensor"])
def receive_event(body: SensorEvent):
    """
    ESP8266 ส่ง event มาที่ endpoint นี้ (ไม่ต้อง auth เพื่อให้ firmware ง่าย)
    อนาคตสามารถเพิ่ม device_secret ใน header ได้
    """
    conn = get_conn()
    event_id = None
    try:
        with conn.cursor() as cur:
            # auto-register device ถ้ายังไม่มี
            cur.execute("""
                INSERT INTO devices(device_id, label, location)
                VALUES(%s, %s, '')
                ON CONFLICT(device_id) DO NOTHING
            """, (body.device_id, body.device_id))

            cur.execute("""
                INSERT INTO events
                  (device_id, event_type, zone, distance_cm,
                   shoe_left, shoe_right, step_count, raw_json)
                VALUES (%s,%s,%s,%s,%s,%s,%s,%s)
                RETURNING id, ts
            """, (
                body.device_id,
                body.event_type,
                body.zone,
                body.distance_cm,
                body.shoe_left,
                body.shoe_right,
                body.step_count,
                json.dumps(body.raw_json) if body.raw_json else None,
            ))
            row = cur.fetchone()
            event_id = row["id"]
            ts_str   = row["ts"].astimezone(TZ_BKK).strftime("%d/%m/%Y %H:%M:%S")

        conn.commit()
    finally:
        conn.close()

    return {"event_id": event_id, "ts": ts_str, "status": "recorded"}


# ──────────────────────────────────────────────
# Routes — Dashboard (หน้า Dashboard อ่าน)
# ──────────────────────────────────────────────

@app.get("/api/dashboard/summary", tags=["Dashboard"])
def dashboard_summary(user: str = Depends(verify_token)):
    """
    สรุปสถิติวันนี้สำหรับหน้า Dashboard
    - จำนวน zone_breach วันนี้
    - จำนวน shoe_removed วันนี้
    - breakdown ตาม zone
    - event ล่าสุด 20 รายการ
    """
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            today_start = datetime.now(TZ_BKK).replace(
                hour=0, minute=0, second=0, microsecond=0
            )

            # รวม zone_breach วันนี้
            cur.execute("""
                SELECT COUNT(*) AS cnt
                FROM events
                WHERE event_type='zone_breach'
                  AND ts >= %s
            """, (today_start,))
            breach_today = cur.fetchone()["cnt"]

            # breakdown ตาม zone วันนี้
            cur.execute("""
                SELECT zone, COUNT(*) AS cnt
                FROM events
                WHERE event_type='zone_breach'
                  AND ts >= %s
                GROUP BY zone
            """, (today_start,))
            breach_by_zone = {r["zone"]: r["cnt"] for r in cur.fetchall()}

            # shoe_removed วันนี้
            cur.execute("""
                SELECT COUNT(*) AS cnt
                FROM events
                WHERE event_type='shoe_removed'
                  AND ts >= %s
            """, (today_start,))
            shoe_removed_today = cur.fetchone()["cnt"]

            # event ล่าสุด 20 รายการ (ทุก type)
            cur.execute("""
                SELECT e.id, e.device_id, d.label, e.event_type,
                       e.zone, e.distance_cm,
                       e.shoe_left, e.shoe_right, e.step_count,
                       e.ts AT TIME ZONE 'Asia/Bangkok' AS ts_local
                FROM events e
                LEFT JOIN devices d ON d.device_id = e.device_id
                ORDER BY e.ts DESC
                LIMIT 20
            """)
            recent = cur.fetchall()

        return {
            "breach_today":     breach_today,
            "breach_by_zone":   breach_by_zone,
            "shoe_removed_today": shoe_removed_today,
            "recent_events":    recent,
        }
    finally:
        conn.close()


# ──────────────────────────────────────────────
# Routes — Log (หน้า Log อ่าน)
# ──────────────────────────────────────────────

@app.get("/api/logs", tags=["Log"])
def get_logs(
    device_id:  Optional[str]  = None,
    event_type: Optional[str]  = None,
    zone:       Optional[str]  = None,
    from_ts:    Optional[str]  = None,
    to_ts:      Optional[str]  = None,
    page:       int            = 1,
    per_page:   int            = 50,
    user: str = Depends(verify_token),
):
    """
    ดึง event logs พร้อม filter และ pagination
    จาก frontend ส่ง query params มา
    """
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            conditions = []
            params     = []

            if device_id:
                conditions.append("e.device_id = %s")
                params.append(device_id)
            if event_type:
                conditions.append("e.event_type = %s")
                params.append(event_type)
            if zone:
                conditions.append("e.zone = %s")
                params.append(zone)
            if from_ts:
                conditions.append("e.ts >= %s")
                params.append(from_ts)
            if to_ts:
                conditions.append("e.ts <= %s")
                params.append(to_ts)

            where = ("WHERE " + " AND ".join(conditions)) if conditions else ""

            # นับ total
            cur.execute(
                f"SELECT COUNT(*) AS cnt FROM events e {where}", params
            )
            total = cur.fetchone()["cnt"]

            # ดึงข้อมูล
            offset = (page - 1) * per_page
            cur.execute(f"""
                SELECT e.id,
                       e.device_id,
                       d.label AS device_label,
                       e.event_type,
                       e.zone,
                       e.distance_cm,
                       e.shoe_left,
                       e.shoe_right,
                       e.step_count,
                       e.ts AT TIME ZONE 'Asia/Bangkok' AS ts_local
                FROM events e
                LEFT JOIN devices d ON d.device_id = e.device_id
                {where}
                ORDER BY e.ts DESC
                LIMIT %s OFFSET %s
            """, params + [per_page, offset])
            rows = cur.fetchall()

        return {
            "total":    total,
            "page":     page,
            "per_page": per_page,
            "pages":    (total + per_page - 1) // per_page,
            "data":     rows,
        }
    finally:
        conn.close()


# ──────────────────────────────────────────────
# Routes — Report (หน้า Report กราฟ)
# ──────────────────────────────────────────────

@app.get("/api/report/zone-pie", tags=["Report"])
def report_zone_pie(
    days: int = 30,
    user: str = Depends(verify_token)
):
    """
    Pie Chart: สัดส่วนการละเมิดแต่ละจุดเสี่ยง
    ส่งกลับ: [{ zone, count }]
    """
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            since = datetime.now(TZ_BKK) - timedelta(days=days)
            cur.execute("""
                SELECT COALESCE(zone, 'unknown') AS zone,
                       COUNT(*) AS count
                FROM events
                WHERE event_type = 'zone_breach'
                  AND ts >= %s
                GROUP BY zone
                ORDER BY count DESC
            """, (since,))
            return cur.fetchall()
    finally:
        conn.close()


@app.get("/api/report/hourly-line", tags=["Report"])
def report_hourly_line(
    days: int = 7,
    user: str = Depends(verify_token)
):
    """
    Line Chart: แนวโน้มช่วงเวลาวิกฤต (zone_breach รายชั่วโมง)
    ส่งกลับ: [{ hour, count }]  hour = 0-23
    """
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            since = datetime.now(TZ_BKK) - timedelta(days=days)
            cur.execute("""
                SELECT EXTRACT(HOUR FROM ts AT TIME ZONE 'Asia/Bangkok')::INT AS hour,
                       COUNT(*) AS count
                FROM events
                WHERE event_type = 'zone_breach'
                  AND ts >= %s
                GROUP BY hour
                ORDER BY hour
            """, (since,))
            # เติม 0 สำหรับชั่วโมงที่ไม่มีข้อมูล
            raw  = {r["hour"]: r["count"] for r in cur.fetchall()}
            data = [{"hour": h, "count": raw.get(h, 0)} for h in range(24)]
            return data
    finally:
        conn.close()


@app.get("/api/report/daily-line", tags=["Report"])
def report_daily_line(
    days: int = 30,
    user: str = Depends(verify_token)
):
    """
    Line Chart: จำนวน event รายวัน (zone_breach + shoe_removed)
    ส่งกลับ: [{ date, zone_breach, shoe_removed }]
    """
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            since = datetime.now(TZ_BKK) - timedelta(days=days)
            cur.execute("""
                SELECT
                    (ts AT TIME ZONE 'Asia/Bangkok')::DATE AS date,
                    event_type,
                    COUNT(*) AS count
                FROM events
                WHERE event_type IN ('zone_breach','shoe_removed')
                  AND ts >= %s
                GROUP BY date, event_type
                ORDER BY date
            """, (since,))
            rows = cur.fetchall()

        # pivot
        pivot: dict = {}
        for r in rows:
            d = str(r["date"])
            if d not in pivot:
                pivot[d] = {"date": d, "zone_breach": 0, "shoe_removed": 0}
            pivot[d][r["event_type"]] = r["count"]

        return list(pivot.values())
    finally:
        conn.close()


@app.get("/api/report/step-stats", tags=["Report"])
def report_step_stats(
    device_id: Optional[str] = None,
    days: int = 7,
    user: str = Depends(verify_token)
):
    """
    สถิติการย่ำเท้า (รองรับล่วงหน้าสำหรับ ESP-01 shoe button)
    ส่งกลับ: { total_steps, avg_per_day, by_device }
    """
    conn = get_conn()
    try:
        with conn.cursor() as cur:
            since = datetime.now(TZ_BKK) - timedelta(days=days)
            params = [since]
            dev_filter = ""
            if device_id:
                dev_filter = "AND device_id = %s"
                params.append(device_id)

            cur.execute(f"""
                SELECT
                    device_id,
                    SUM(step_count) AS total_steps,
                    COUNT(*) AS records,
                    AVG(step_count) AS avg_steps_per_record
                FROM events
                WHERE event_type = 'step_data'
                  AND step_count IS NOT NULL
                  AND ts >= %s
                  {dev_filter}
                GROUP BY device_id
            """, params)
            by_device = cur.fetchall()

            total = sum(r["total_steps"] or 0 for r in by_device)
            return {
                "days":       days,
                "total_steps": total,
                "by_device":  by_device,
            }
    finally:
        conn.close()


# ──────────────────────────────────────────────
# Health check (Render ping)
# ──────────────────────────────────────────────

@app.get("/health", tags=["System"])
def health():
    return {"status": "ok", "ts": datetime.now(TZ_BKK).isoformat()}


@app.get("/", tags=["System"])
def root():
    return {
        "app": "Smart Shoe Monitor API",
        "version": "1.0.0",
        "docs": "/docs",
    }
