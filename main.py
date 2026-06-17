from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
import os

from app.models import create_tables, SessionLocal, User, Patient
from app.auth import hash_password
from app.routes import router


@asynccontextmanager
async def lifespan(app: FastAPI):
    # สร้าง table และ seed ข้อมูลเริ่มต้น
    create_tables()
    seed_default_data()
    yield


def seed_default_data():
    db = SessionLocal()
    try:
        # สร้าง user เริ่มต้นถ้ายังไม่มี
        if not db.query(User).filter(User.username == "admin").first():
            db.add(User(
                username="admin",
                hashed_password=hash_password("admin1234"),
                full_name="ผู้ดูแลระบบ",
            ))
            db.commit()
            print("✓ Created default user: admin / admin1234")

        # สร้างผู้ป่วยตัวอย่างถ้ายังไม่มี
        if not db.query(Patient).filter(Patient.patient_code == "PT-2024-001").first():
            db.add(Patient(
                patient_code="PT-2024-001",
                full_name="สมชาย ธนากร",
                age=67,
                gender="ชาย",
                weight_kg=72.0,
                height_cm=168.0,
                condition="เบาหวาน",
                doctor_name="นพ.วิชาญ",
                step_goal=7500,
            ))
            db.commit()
            print("✓ Created default patient: PT-2024-001")
    finally:
        db.close()


app = FastAPI(
    title="Health Monitor API",
    description="API สำหรับรับข้อมูลสุขภาพจาก ESP8266 และแสดงผลบนเว็บ",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],   # ปรับเป็น domain จริงตอน production
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# API routes
app.include_router(router, prefix="/api")

# Static files (frontend)
static_dir = os.path.join(os.path.dirname(__file__), "frontend", "static")
if os.path.exists(static_dir):
    app.mount("/static", StaticFiles(directory=static_dir), name="static")

@app.get("/", include_in_schema=False)
def serve_frontend():
    index_path = os.path.join(os.path.dirname(__file__), "frontend", "index.html")
    return FileResponse(index_path)

@app.get("/health", tags=["system"])
def health_check():
    return {"status": "ok"}
