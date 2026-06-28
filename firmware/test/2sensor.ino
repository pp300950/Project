
#define SENSOR_FENCE D6   // เซนเซอร์ตัวที่ 1 — รั้ว
#define SENSOR_POOL  D7   // เซนเซอร์ตัวที่ 2 — สระว่ายน้ำ

bool lastFence = HIGH;
bool lastPool  = HIGH;
bool curFence, curPool;

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_FENCE, INPUT);
  pinMode(SENSOR_POOL,  INPUT);
  Serial.println("=== Security Sensor Ready ===");
  Serial.println("D6 = รั้ว | D5 = สระว่ายน้ำ");
  Serial.println("==============================");
}

void loop() {
  curFence = digitalRead(SENSOR_FENCE);
  curPool  = digitalRead(SENSOR_POOL);

  // - เซนเซอร์รั้ว -
  if (curFence != lastFence) {
    
    delay(50);
    curFence = digitalRead(SENSOR_FENCE);
    
    if (curFence != lastFence) {
      if (curFence == LOW) {
        Serial.println("[รั้ว]  ⚠ มีคนเดินผ่านรั้ว!");
        
      } else {
        Serial.println("[รั้ว]  ✅ ไม่มีสิ่งกีดขวาง");
        
      }
      
      lastFence = curFence;
      
    }
    
  }

  // - เซนเซอร์สระว่ายน้ำ -
  if (curPool != lastPool) {
    delay(50);
    curPool = digitalRead(SENSOR_POOL);
    if (curPool != lastPool) {
      if (curPool == LOW) {
        Serial.println("[สระ]   ⚠ มีคนเดินผ่านสระว่ายน้ำ!");
      } else {
        Serial.println("[สระ]   ✅ ไม่มีสิ่งกีดขวาง");
      }
      lastPool = curPool;
    }
  }
}



