from fastapi import APIRouter, Depends, HTTPException, status, Header
from sqlalchemy.orm import Session
from sqlalchemy import func, cast, Date
from datetime import datetime, timezone, timedelta
from typing import List, Optional

from app.models import get_db, User, Patient, HealthRecord
from app.schemas import (
    LoginRequest, Token,
    PatientCreate, PatientUpdate, PatientOut,
    HealthDataIn, HealthRecordOut,
    VitalSummary, ChartData, ChartPoint,
)
from app.auth import (
    verify_password, create_access_token,
    hash_password, get_current_user,
)
from app.config import settings

router = APIRouter()


# ───────────────────────────────────────────
# AUTH
# ───────────────────────────────────────────

@router.post("/auth/login", response_model=Token, tags=["auth"])
def login(body: LoginRequest, db: Session = Depends(get_db)):
    user = db.query(User).filter(User.username == body.username).first()
    if not user or not verify_password(body.password, user.hashed_password):
        raise HTTPException(status_code=401, detail="ชื่อผู้ใช้หรือรหัสผ่านไม่ถูกต้อง")
    token = create_access_token({"sub": user.username})
    return {"access_token": token}


# ───────────────────────────────────────────
# PATIENT
# ───────────────────────────────────────────

@router.get("/patients", response_model=List[PatientOut], tags=["patient"])
def list_patients(db: Session = Depends(get_db), _: User = Depends(get_current_user)):
    return db.query(Patient).all()


@router.post("/patients", response_model=PatientOut, status_code=201, tags=["patient"])
def create_patient(body: PatientCreate, db: Session = Depends(get_db), _: User = Depends(get_current_user)):
    if db.query(Patient).filter(Patient.patient_code == body.patient_code).first():
        raise HTTPException(400, detail=f"patient_code '{body.patient_code}' มีอยู่แล้ว")
    patient = Patient(**body.model_dump())
    db.add(patient)
    db.commit()
    db.refresh(patient)
    return patient


@router.get("/patients/{patient_code}", response_model=PatientOut, tags=["patient"])
def get_patient(patient_code: str, db: Session = Depends(get_db), _: User = Depends(get_current_user)):
    patient = db.query(Patient).filter(Patient.patient_code == patient_code).first()
    if not patient:
        raise HTTPException(404, detail="ไม่พบผู้ป่วย")
    return patient


@router.patch("/patients/{patient_code}", response_model=PatientOut, tags=["patient"])
def update_patient(patient_code: str, body: PatientUpdate, db: Session = Depends(get_db), _: User = Depends(get_current_user)):
    patient = db.query(Patient).filter(Patient.patient_code == patient_code).first()
    if not patient:
        raise HTTPException(404, detail="ไม่พบผู้ป่วย")
    for field, value in body.model_dump(exclude_unset=True).items():
        setattr(patient, field, value)
    patient.updated_at = datetime.now(timezone.utc)
    db.commit()
    db.refresh(patient)
    return patient


# ───────────────────────────────────────────
# ESP8266 → รับข้อมูลสุขภาพ
# (ใช้ API Key แทน JWT เพราะ ESP ทำ OAuth ลำบาก)
# ───────────────────────────────────────────

def verify_esp_key(x_api_key: str = Header(...)):
    if x_api_key != settings.ESP_API_KEY:
        raise HTTPException(status_code=403, detail="API Key ไม่ถูกต้อง")


@router.post("/device/data", status_code=201, tags=["device"])
def receive_device_data(
    body: HealthDataIn,
    db: Session = Depends(get_db),
    _: None = Depends(verify_esp_key),
):
    patient = db.query(Patient).filter(Patient.patient_code == body.patient_code).first()
    if not patient:
        raise HTTPException(404, detail=f"ไม่พบ patient_code '{body.patient_code}'")

    record = HealthRecord(
        patient_id=patient.id,
        steps=body.steps,
        cadence=body.cadence,
        distance_m=body.distance_m,
        heart_rate=body.heart_rate,
        activity_duration_sec=body.activity_duration_sec,
        device_id=body.device_id,
    )
    db.add(record)
    db.commit()
    return {"status": "ok", "recorded_at": record.recorded_at.isoformat()}


# ───────────────────────────────────────────
# DASHBOARD — vitals summary
# ───────────────────────────────────────────

@router.get("/dashboard/{patient_code}/vitals", response_model=VitalSummary, tags=["dashboard"])
def get_vitals(patient_code: str, db: Session = Depends(get_db), _: User = Depends(get_current_user)):
    patient = db.query(Patient).filter(Patient.patient_code == patient_code).first()
    if not patient:
        raise HTTPException(404, detail="ไม่พบผู้ป่วย")

    now = datetime.now(timezone.utc)
    today_start = now.replace(hour=0, minute=0, second=0, microsecond=0)
    week_start = now - timedelta(days=7)

    # ก้าววันนี้ — เอา record ล่าสุดของวันนี้
    today_rec = (
        db.query(HealthRecord)
        .filter(HealthRecord.patient_id == patient.id, HealthRecord.recorded_at >= today_start)
        .order_by(HealthRecord.recorded_at.desc())
        .first()
    )

    # ค่าเฉลี่ย 7 วัน — aggregate รายวัน แล้วเฉลี่ย
    week_records = (
        db.query(HealthRecord)
        .filter(HealthRecord.patient_id == patient.id, HealthRecord.recorded_at >= week_start)
        .all()
    )

    def safe_avg(values):
        vals = [v for v in values if v is not None]
        return round(sum(vals) / len(vals), 1) if vals else None

    return VitalSummary(
        steps_today=today_rec.steps if today_rec else 0,
        steps_avg_7d=safe_avg([r.steps for r in week_records]) or 0,
        cadence_avg_7d=safe_avg([r.cadence for r in week_records]),
        heart_rate_avg_7d=safe_avg([r.heart_rate for r in week_records]),
        distance_avg_7d_m=safe_avg([r.distance_m for r in week_records]),
        last_recorded_at=today_rec.recorded_at if today_rec else None,
    )


# ───────────────────────────────────────────
# DASHBOARD — chart data
# ───────────────────────────────────────────

@router.get("/dashboard/{patient_code}/chart", response_model=ChartData, tags=["dashboard"])
def get_chart_data(
    patient_code: str,
    range: str = "7d",
    db: Session = Depends(get_db),
    _: User = Depends(get_current_user),
):
    patient = db.query(Patient).filter(Patient.patient_code == patient_code).first()
    if not patient:
        raise HTTPException(404, detail="ไม่พบผู้ป่วย")

    days_map = {"7d": 7, "30d": 30, "90d": 90}
    days = days_map.get(range, 7)
    since = datetime.now(timezone.utc) - timedelta(days=days)

    records = (
        db.query(HealthRecord)
        .filter(HealthRecord.patient_id == patient.id, HealthRecord.recorded_at >= since)
        .order_by(HealthRecord.recorded_at.asc())
        .all()
    )

    # group by date — เอาค่าสูงสุดของก้าวในแต่ละวัน (เพราะ ESP ส่งแบบสะสม)
    by_date: dict = {}
    for r in records:
        key = r.recorded_at.strftime("%Y-%m-%d")
        if key not in by_date:
            by_date[key] = {"steps": [], "cadence": [], "hr": [], "dist": []}
        by_date[key]["steps"].append(r.steps)
        if r.cadence:    by_date[key]["cadence"].append(r.cadence)
        if r.heart_rate: by_date[key]["hr"].append(r.heart_rate)
        if r.distance_m: by_date[key]["dist"].append(r.distance_m)

    def avg(lst): return round(sum(lst) / len(lst), 1) if lst else None

    points = [
        ChartPoint(
            date=date,
            steps=max(v["steps"]),           # สะสม → เอาสูงสุดของวัน
            cadence=avg(v["cadence"]),
            heart_rate=avg(v["hr"]),
            distance_m=avg(v["dist"]),
        )
        for date, v in sorted(by_date.items())
    ]

    return ChartData(range=range, data=points)
