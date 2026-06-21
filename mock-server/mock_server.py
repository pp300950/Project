"""
mock_server.py — FastAPI mock server สำหรับทดสอบ flow การส่งข้อมูลจาก ESP8266 มาสเตอร์

จุดประสงค์: ให้ทดสอบได้ว่าบอร์ดส่งข้อมูลถูก format/endpoint ก่อนไปต่อ Supabase จริง
- เก็บข้อมูลไว้ใน memory เท่านั้น (list ธรรมดา) รีสตาร์ท server แล้วข้อมูลหายหมด
- ไม่มี authentication/encryption ใดๆ ในไฟล์นี้ — ของจริงต้องทำตามที่ระบุใน
  smart-shoe-monitor-architecture.md (AES-256-GCM, RBAC, audit log) ก่อนใช้งานจริง
- พิมพ์ทุก request ที่เข้ามาลง console ให้เห็นว่าบอร์ดส่งอะไรมาบ้าง real-time

วิธีรัน:
  pip install fastapi uvicorn
  uvicorn mock_server:app --host 0.0.0.0 --port 8000 --reload

แล้วตั้งค่า server_url ในมาสเตอร์ (ผ่าน /config) ให้เป็น http://<IP เครื่องที่รัน server นี้>:8000
*** เครื่องที่รัน mock server กับ ESP8266 ต้องอยู่ใน WiFi วงเดียวกัน (วง STA/บ้าน) ***
"""

from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from datetime import datetime
from typing import Optional
from pydantic import BaseModel

app = FastAPI(title="Smart Shoe Monitor — Mock Server")

# อนุญาตทุก origin เพื่อให้ทดสอบจาก dashboard HTML ได้ง่าย (ของจริงต้องจำกัด origin)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ===== เก็บข้อมูลใน memory เท่านั้น (mock — ของจริงคือ Supabase) =====
events_log: list[dict] = []
heartbeats_log: list[dict] = []


class ShoeEvent(BaseModel):
    event_type: str            # "shoe_status"
    device_id: str             # "shoe01_left" / "shoe01_right"
    foot_in_shoe: bool
    timestamp: str


class ZoneEvent(BaseModel):
    event_type: str            # "zone_intrusion"
    zone_id: str                # "pool" / "door" / "fence"
    distance_cm: float
    timestamp: str


class SystemEvent(BaseModel):
    event_type: str            # "system_online"
    message: str


class HeartbeatPayload(BaseModel):
    device_id: str
    uptime_ms: int
    free_heap: Optional[int] = None


@app.post("/api/v1/events")
async def receive_event(request: Request):
    """
    รับ event ทุกชนิดจากมาสเตอร์ — ใช้ Request ดิบแทน Pydantic model เดียว
    เพราะ payload มีหลายรูปแบบ (shoe_status / zone_intrusion / system_online)
    แยกตาม field "event_type" เอง แทนการสร้าง union model ที่ซับซ้อนเกินจำเป็นสำหรับ mock
    """
    body = await request.json()
    body["received_at"] = datetime.now().isoformat()
    events_log.append(body)

    event_type = body.get("event_type", "unknown")
    print(f"\n📥 [EVENT] {event_type}")
    print(f"   payload: {body}")

    # จำลองตรรกะ "เคสเสี่ยงสูง" แบบง่ายๆ — งานจริงต้องเทียบเวลาข้าม event
    # และดึงข้อมูลผู้ป่วยจาก DB (ของ mock นี้แค่ print เตือนให้เห็น flow)
    if event_type == "zone_intrusion":
        print(f"   ⚠️  มีคนเดินเข้าใกล้จุดเฝ้าระวัง: {body.get('zone_id')} "
              f"(ระยะ {body.get('distance_cm')} cm)")
    elif event_type == "shoe_status" and not body.get("foot_in_shoe", True):
        print(f"   👟 ถอดรองเท้า: {body.get('device_id')}")

    return {"status": "received", "event_type": event_type}


@app.post("/api/v1/heartbeat")
async def receive_heartbeat(payload: HeartbeatPayload):
    entry = payload.model_dump()
    entry["received_at"] = datetime.now().isoformat()
    heartbeats_log.append(entry)

    print(f"💓 [HEARTBEAT] {payload.device_id} — uptime {payload.uptime_ms}ms, "
          f"free_heap {payload.free_heap}")

    return {"status": "received"}


@app.get("/api/v1/events")
async def list_events(limit: int = 50):
    """ดูข้อมูล event ทั้งหมดที่เข้ามา (เรียงใหม่สุดก่อน) — ใช้ตรวจ debug ผ่านเบราว์เซอร์ได้เลย"""
    return {"count": len(events_log), "events": list(reversed(events_log))[:limit]}


@app.get("/api/v1/heartbeat")
async def list_heartbeats(limit: int = 50):
    return {"count": len(heartbeats_log), "heartbeats": list(reversed(heartbeats_log))[:limit]}


@app.get("/")
async def root():
    return {
        "service": "Smart Shoe Monitor Mock Server",
        "status": "running",
        "endpoints": [
            "POST /api/v1/events",
            "POST /api/v1/heartbeat",
            "GET  /api/v1/events",
            "GET  /api/v1/heartbeat",
        ],
        "note": "ข้อมูลทั้งหมดเก็บใน memory เท่านั้น รีสตาร์ท server แล้วข้อมูลหาย — ของจริงต่อ Supabase ดูใน README",
    }
