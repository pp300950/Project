"""
test_sensor.py — จำลองการส่งข้อมูลจากอุปกรณ์ฮาร์ดแวร์ขึ้น server

วิธีใช้:
  1. แก้ BASE_URL ให้ตรงกับ server ที่รัน (local หรือ Render)
  2. รัน:  python test_sensor.py
  3. เลือก mode ที่ต้องการทดสอบ

ต้องการ:  pip install requests
"""

import requests
import random
import time
import argparse
from datetime import datetime

# ─────────────────────────────────────────────
# ตั้งค่าเป้าหมาย
# ─────────────────────────────────────────────

BASE_URL   = "http://localhost:8000"   # ← เปลี่ยนเป็น https://your-app.onrender.com ตอน deploy
DEVICE_ID  = "esp8266_test_01"         # ← device_id ที่จะส่งข้อมูล

# ─────────────────────────────────────────────
# Helper
# ─────────────────────────────────────────────

def ts():
    return datetime.now().strftime("%H:%M:%S")

def post_event(payload: dict, label: str = ""):
    url = f"{BASE_URL}/api/event"
    try:
        r = requests.post(url, json=payload, timeout=10)
        print(f"  [{ts()}] {label}")
        print(f"           → {r.status_code} | {r.json()}")
    except Exception as e:
        print(f"  [{ts()}] ERROR: {e}")

def sep(title: str):
    print(f"\n{'─'*50}")
    print(f"  {title}")
    print(f"{'─'*50}")

# ─────────────────────────────────────────────
# Mode 1: ยิงเหตุการณ์ครั้งเดียวแบบครบทุก type
# ─────────────────────────────────────────────

def test_one_of_each():
    sep("TEST: ยิงครั้งเดียวครบทุก event_type")

    # zone_breach — รั้วบ้าน
    post_event({
        "device_id":   DEVICE_ID,
        "event_type":  "zone_breach",
        "zone":        "fence",
        "distance_cm": 11.4,
    }, "zone_breach → fence")
    time.sleep(1)

    # zone_breach — สระว่ายน้ำ
    post_event({
        "device_id":   DEVICE_ID,
        "event_type":  "zone_breach",
        "zone":        "pool",
        "distance_cm": 8.7,
    }, "zone_breach → pool")
    time.sleep(1)

    # shoe_removed — ถอดข้างซ้าย
    post_event({
        "device_id":  DEVICE_ID,
        "event_type": "shoe_removed",
        "shoe_left":  False,
        "shoe_right": True,
    }, "shoe_removed → ข้างซ้าย")
    time.sleep(1)

    # shoe_removed — ถอดทั้งคู่
    post_event({
        "device_id":  DEVICE_ID,
        "event_type": "shoe_removed",
        "shoe_left":  False,
        "shoe_right": False,
    }, "shoe_removed → ทั้งสองข้าง")
    time.sleep(1)

    # step_data — จำลองรอบการเดิน (ฟีเจอร์อนาคต)
    post_event({
        "device_id":  DEVICE_ID,
        "event_type": "step_data",
        "step_count": 42,
        "shoe_left":  True,
        "shoe_right": True,
        "raw_json":   {"raw_left": 1, "raw_right": 1, "interval_ms": 980},
    }, "step_data → 42 steps")
    time.sleep(1)

    # heartbeat — แค่ ping บอกว่าอุปกรณ์ยังออนไลน์
    post_event({
        "device_id":  DEVICE_ID,
        "event_type": "heartbeat",
    }, "heartbeat")

# ─────────────────────────────────────────────
# Mode 2: ยิงต่อเนื่อง (simulate real sensor loop)
# ─────────────────────────────────────────────

def test_continuous(count: int = 20, interval: float = 2.0):
    sep(f"TEST: ยิงต่อเนื่อง {count} ครั้ง (ทุก {interval}s)")

    ZONES       = ["fence", "pool"]
    EVENT_WEIGHTS = [
        ("zone_breach",  40),  # 40% โอกาส
        ("heartbeat",    35),  # 35%
        ("shoe_removed", 15),  # 15%
        ("step_data",    10),  # 10%
    ]
    events, weights = zip(*EVENT_WEIGHTS)

    for i in range(1, count + 1):
        print(f"\n  round {i}/{count}")
        etype = random.choices(events, weights=weights)[0]

        payload: dict = {"device_id": DEVICE_ID, "event_type": etype}

        if etype == "zone_breach":
            payload["zone"]        = random.choice(ZONES)
            payload["distance_cm"] = round(random.uniform(4.0, 19.0), 1)

        elif etype == "shoe_removed":
            # สุ่ม: ถอดข้างเดียวหรือทั้งคู่
            payload["shoe_left"]  = random.choice([True, False])
            payload["shoe_right"] = random.choice([True, False])
            # ถ้าสุ่มได้ทั้ง True จะไม่ถอดเลย — ให้ใช้ as-is

        elif etype == "step_data":
            payload["step_count"]  = random.randint(5, 120)
            payload["shoe_left"]   = True
            payload["shoe_right"]  = True
            payload["raw_json"]    = {"interval_ms": random.randint(700, 1300)}

        post_event(payload, etype)
        time.sleep(interval)

# ─────────────────────────────────────────────
# Mode 3: ยิงเหตุการณ์วิกฤต (ทดสอบ LINE alert)
# ─────────────────────────────────────────────

def test_critical():
    sep("TEST: ยิงเหตุการณ์วิกฤต (ทดสอบ LINE + buzzer)")

    print("  ⚠  zone_breach สระว่ายน้ำ ระยะใกล้มาก")
    post_event({
        "device_id":   DEVICE_ID,
        "event_type":  "zone_breach",
        "zone":        "pool",
        "distance_cm": 5.2,
    }, "CRITICAL: pool breach ระยะ 5.2cm")
    time.sleep(2)

    print("  ⚠  ถอดรองเท้าทั้งสองข้าง ก่อนเข้าใกล้สระ")
    post_event({
        "device_id":  DEVICE_ID,
        "event_type": "shoe_removed",
        "shoe_left":  False,
        "shoe_right": False,
    }, "CRITICAL: ถอดรองเท้าทั้งคู่")

# ─────────────────────────────────────────────
# Mode 4: stress test — ยิงเร็ว ๆ หลาย request
# ─────────────────────────────────────────────

def test_stress(count: int = 50):
    sep(f"TEST: stress {count} requests ติดกัน (ไม่หยุดพัก)")
    for i in range(1, count + 1):
        post_event({
            "device_id":   DEVICE_ID,
            "event_type":  "zone_breach",
            "zone":        random.choice(["fence", "pool"]),
            "distance_cm": round(random.uniform(5, 18), 1),
        }, f"stress #{i}")

# ─────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────

MODES = {
    "1": ("ยิงครั้งเดียวครบทุก type",        test_one_of_each),
    "2": ("ยิงต่อเนื่อง 20 ครั้ง (2s/ครั้ง)", lambda: test_continuous(20, 2.0)),
    "3": ("ยิงเหตุการณ์วิกฤต (test LINE)",    test_critical),
    "4": ("Stress test 50 requests",           lambda: test_stress(50)),
}

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Smart Shoe Sensor Simulator")
    parser.add_argument("--url",    default=BASE_URL,  help="Base URL ของ server")
    parser.add_argument("--device", default=DEVICE_ID, help="device_id ที่ใช้ทดสอบ")
    parser.add_argument("--mode",   default=None,      help="1/2/3/4 (ถ้าไม่ระบุจะถามแบบ interactive)")
    args = parser.parse_args()

    BASE_URL  = args.url
    DEVICE_ID = args.device

    print(f"\n Smart Shoe Sensor Simulator")
    print(f"  Server  : {BASE_URL}")
    print(f"  Device  : {DEVICE_ID}")

    if args.mode and args.mode in MODES:
        chosen = args.mode
    else:
        print("\n  เลือก mode:")
        for k, (label, _) in MODES.items():
            print(f"    {k}. {label}")
        print("    0. ออก")
        chosen = input("\n  > ").strip()

    if chosen == "0":
        print("  ออก")
    elif chosen in MODES:
        MODES[chosen][1]()
        print(f"\n  เสร็จสิ้น ✓")
    else:
        print("  ไม่รู้จัก mode นี้")
