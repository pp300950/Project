/*
การต่อสายไฟ

D5  → ปุ่มกด → GND
D6  → D0 เซนเซอร์รั้ว
D7  → D0 เซนเซอร์สระ
D2  → ขา + บัซเซอร์ → GND
3.3V → VCC เซนเซอร์ทั้งสอง
GND → GND เซนเซอร์ทั้งสอง
*/

#define SENSOR_FENCE D6   // IR เซนเซอร์ตัวที่ 1 — รั้ว
#define SENSOR_POOL  D7   // IR เซนเซอร์ตัวที่ 2 — สระว่ายน้ำ
#define BUTTON_PIN   D5   // ปุ่มกด — สถานะระบบ (กด=ปิด, ปล่อย=เปิด)
#define BUZZER_PIN   D2   // บัซเซอร์

bool lastFence = HIGH;
bool lastPool  = HIGH;
bool curFence, curPool;
bool buttonState;

void triggerBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_FENCE, INPUT);
  pinMode(SENSOR_POOL,  INPUT);
  pinMode(BUTTON_PIN,   INPUT_PULLUP);
  pinMode(BUZZER_PIN,   OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("=== Security System Ready ===");
}

void loop() {
  buttonState = digitalRead(BUTTON_PIN);
  curFence    = digitalRead(SENSOR_FENCE);
  curPool     = digitalRead(SENSOR_POOL);

  // แสดงสถานะระบบ
  bool systemArmed = (buttonState == HIGH); // ปล่อยปุ่ม = ระบบเปิด

  // --- เซนเซอร์รั้ว ---
  if (curFence != lastFence) {
    delay(50);
    curFence = digitalRead(SENSOR_FENCE);
    if (curFence != lastFence) {
      if (curFence == LOW) {
        Serial.println("[รั้ว] ⚠ มีคนเดินผ่านรั้ว!");
        if (systemArmed) {
          Serial.println("       🔔 แจ้งเตือน! ระบบทำงาน");
          triggerBuzzer();
        } else {
          Serial.println("       🔕 ระบบปิดอยู่");
        }
      } else {
        Serial.println("[รั้ว] ✅ ไม่มีคนเดินผ่านรั้ว");
      }
      lastFence = curFence;
    }
  }

  // --- เซนเซอร์สระว่ายน้ำ ---
  if (curPool != lastPool) {
    delay(50);
    curPool = digitalRead(SENSOR_POOL);
    if (curPool != lastPool) {
      if (curPool == LOW) {
        Serial.println("[สระ]  ⚠ มีคนเดินผ่านสระว่ายน้ำ!");
        if (systemArmed) {
          Serial.println("       🔔 แจ้งเตือน! ระบบทำงาน");
          triggerBuzzer();
        } else {
          Serial.println("       🔕 ระบบปิดอยู่");
        }
      } else {
        Serial.println("[สระ]  ✅ ไม่มีคนเดินผ่านสระว่ายน้ำ");
      }
      lastPool = curPool;
    }
  }
}