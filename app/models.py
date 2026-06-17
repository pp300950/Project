from sqlalchemy import create_engine, Column, Integer, Float, String, DateTime, ForeignKey, Text
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, relationship
from datetime import datetime, timezone
from app.config import settings

engine = create_engine(settings.DATABASE_URL)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()


class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, index=True)
    username = Column(String, unique=True, index=True, nullable=False)
    hashed_password = Column(String, nullable=False)
    full_name = Column(String)
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))


class Patient(Base):
    __tablename__ = "patients"

    id = Column(Integer, primary_key=True, index=True)
    patient_code = Column(String, unique=True, index=True)  # เช่น PT-2024-001
    full_name = Column(String, nullable=False)
    age = Column(Integer)
    gender = Column(String)
    weight_kg = Column(Float)
    height_cm = Column(Float)
    condition = Column(String)          # โรคประจำตัว
    doctor_name = Column(String)
    step_goal = Column(Integer, default=7500)
    notes = Column(Text)
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    updated_at = Column(DateTime, default=lambda: datetime.now(timezone.utc), onupdate=lambda: datetime.now(timezone.utc))

    health_records = relationship("HealthRecord", back_populates="patient")


class HealthRecord(Base):
    __tablename__ = "health_records"

    id = Column(Integer, primary_key=True, index=True)
    patient_id = Column(Integer, ForeignKey("patients.id"), nullable=False)
    recorded_at = Column(DateTime, default=lambda: datetime.now(timezone.utc), index=True)

    # ข้อมูลจาก ESP8266
    steps = Column(Integer, default=0)              # จำนวนก้าว (สะสม)
    cadence = Column(Float)                         # ก้าว/นาที
    distance_m = Column(Float)                      # ระยะทาง เมตร
    heart_rate = Column(Integer)                    # bpm
    activity_duration_sec = Column(Integer)         # วินาทีที่เคลื่อนไหว
    device_id = Column(String)                      # MAC หรือ ID ของ ESP8266

    patient = relationship("Patient", back_populates="health_records")


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


def create_tables():
    Base.metadata.create_all(bind=engine)
