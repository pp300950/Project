from pydantic import BaseModel
from datetime import datetime
from typing import Optional, List


# ---------- Auth ----------
class LoginRequest(BaseModel):
    username: str
    password: str

class Token(BaseModel):
    access_token: str
    token_type: str = "bearer"


# ---------- Patient ----------
class PatientCreate(BaseModel):
    patient_code: str
    full_name: str
    age: Optional[int] = None
    gender: Optional[str] = None
    weight_kg: Optional[float] = None
    height_cm: Optional[float] = None
    condition: Optional[str] = None
    doctor_name: Optional[str] = None
    step_goal: int = 7500
    notes: Optional[str] = None

class PatientUpdate(BaseModel):
    full_name: Optional[str] = None
    age: Optional[int] = None
    gender: Optional[str] = None
    weight_kg: Optional[float] = None
    height_cm: Optional[float] = None
    condition: Optional[str] = None
    doctor_name: Optional[str] = None
    step_goal: Optional[int] = None
    notes: Optional[str] = None

class PatientOut(BaseModel):
    id: int
    patient_code: str
    full_name: str
    age: Optional[int]
    gender: Optional[str]
    weight_kg: Optional[float]
    height_cm: Optional[float]
    condition: Optional[str]
    doctor_name: Optional[str]
    step_goal: int
    notes: Optional[str]
    updated_at: datetime

    class Config:
        from_attributes = True


# ---------- Health Record (จาก ESP8266) ----------
class HealthDataIn(BaseModel):
    patient_code: str           # ESP รู้ patient_code ไว้ล่วงหน้า
    steps: int = 0
    cadence: Optional[float] = None
    distance_m: Optional[float] = None
    heart_rate: Optional[int] = None
    activity_duration_sec: Optional[int] = None
    device_id: Optional[str] = None

class HealthRecordOut(BaseModel):
    id: int
    recorded_at: datetime
    steps: int
    cadence: Optional[float]
    distance_m: Optional[float]
    heart_rate: Optional[int]
    activity_duration_sec: Optional[int]

    class Config:
        from_attributes = True


# ---------- Dashboard ----------
class VitalSummary(BaseModel):
    steps_today: int
    steps_avg_7d: float
    cadence_avg_7d: Optional[float]
    heart_rate_avg_7d: Optional[float]
    distance_avg_7d_m: Optional[float]
    last_recorded_at: Optional[datetime]

class ChartPoint(BaseModel):
    date: str
    steps: int
    cadence: Optional[float]
    heart_rate: Optional[float]
    distance_m: Optional[float]

class ChartData(BaseModel):
    range: str
    data: List[ChartPoint]
