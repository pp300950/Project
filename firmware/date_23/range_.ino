/*
 * ระบบตรวจจับ 2 จุด (HC-SR04 x2)
 *
 * การต่อสายไฟ:
 *   === ตัวที่ 1: รั้วบ้าน ===
 *   HC-SR04 #1 TRIG → D1
 *   HC-SR04 #1 ECHO → D2
 *
 *   === ตัวที่ 2: สระว่ายน้ำ ===
 *   HC-SR04 #2 TRIG → D6
 *   HC-SR04 #2 ECHO → D7
 *
 *   Buzzer +  → D5
 *   Buzzer -  → GND
 *   VCC ทั้งคู่ → 3.3V (หรือ 5V ถ้าจับได้ไม่ดี)
 *   GND ทั้งคู่ → GND
 */

#include <ESP8266WiFi.h>
#include <time.h>

const char* ssid     = "PP";
const char* password = "12345678";

// === เซ็นเซอร์ตัวที่ 1: รั้วบ้าน ===
#define TRIG1_PIN          D1
#define ECHO1_PIN          D2
#define DETECT1_DIST_CM    16
#define PULSIN1_TIMEOUT    4000

// === เซ็นเซอร์ตัวที่ 2: สระว่ายน้ำ ===
#define TRIG2_PIN          D6
#define ECHO2_PIN          D7
#define DETECT2_DIST_CM    20
#define PULSIN2_TIMEOUT    4500

// === Buzzer ===
#define BUZZER_PIN         D5

// === Cooldown แยกกันแต่ละจุด ===
#define COOLDOWN_MS        5000

// === Confirm ติดต่อกันกี่ครั้งถึงทริก ===
#define DETECT_CONFIRM     3

unsigned long lastAlert1 = 0;
unsigned long lastAlert2 = 0;
int detectCount1 = 0;
int detectCount2 = 0;

int timezone = 7 * 3600;
int dst = 0;

// === Buzzer non-blocking ===
unsigned long buzzerStartTime = 0;
int buzzerStep = 0;
bool buzzerActive = false;

// ================================================

String getTimestamp() {
  time_t now = time(nullptr);
  if (now < 86400) return "ไม่สามารถดึงเวลาได้";
  struct tm* t = localtime(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", t);
  return String(buf);
}

float readDistanceCM(int trigPin, int echoPin, unsigned long timeout_us) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, timeout_us);
  if (duration == 0) return -1.0;
  return duration * 0.034 / 2.0;
}

void startBuzzer() {
  buzzerStep = 0;
  buzzerActive = true;
  buzzerStartTime = millis();
  digitalWrite(BUZZER_PIN, HIGH);
}

void updateBuzzer() {
  if (!buzzerActive) return;

  unsigned long elapsed = millis() - buzzerStartTime;
  unsigned long times[] = {300, 500, 800, 1000, 1300, 1500};

  if (buzzerStep < 6 && elapsed >= times[buzzerStep]) {
    buzzerStep++;
    if (buzzerStep % 2 == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
    if (buzzerStep >= 6) {
      buzzerActive = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

void alertFence() {
  lastAlert1 = millis();
  detectCount1 = 0;
  Serial.println("[ALERT] รั้วบ้าน — " + getTimestamp());
  startBuzzer();
}

void alertPool() {
  lastAlert2 = millis();
  detectCount2 = 0;
  Serial.println("[ALERT] สระว่ายน้ำ — " + getTimestamp());
  startBuzzer();
}

// ================================================

void setup() {
  Serial.begin(115200);

  pinMode(TRIG1_PIN,  OUTPUT);
  pinMode(ECHO1_PIN,  INPUT);
  pinMode(TRIG2_PIN,  OUTPUT);
  pinMode(ECHO2_PIN,  INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRIG1_PIN,  LOW);
  digitalWrite(TRIG2_PIN,  LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.print("กำลังเชื่อม WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString());

  configTime(timezone, dst, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 86400) { delay(500); now = time(nullptr); }
  Serial.println("เวลา: " + getTimestamp());

  Serial.println("=== Ready ===");
  Serial.println("รั้วบ้าน   : " + String(DETECT1_DIST_CM) + " cm");
  Serial.println("สระว่ายน้ำ: " + String(DETECT2_DIST_CM) + " cm");
}

void loop() {
  updateBuzzer();

  // --- ตรวจจับ: รั้วบ้าน ---
  float dist1 = readDistanceCM(TRIG1_PIN, ECHO1_PIN, PULSIN1_TIMEOUT);
  if (dist1 < 0) {
    detectCount1 = 0;
    Serial.println("[รั้วบ้าน] ไม่มีสัญญาณ (timeout)");
  } else {
    Serial.println("[รั้วบ้าน] ระยะ: " + String(dist1, 1) + " cm");
    if (dist1 < DETECT1_DIST_CM) {
      detectCount1++;
      Serial.println("[รั้วบ้าน] confirm: " + String(detectCount1) + "/" + String(DETECT_CONFIRM));
      if (detectCount1 >= DETECT_CONFIRM && millis() - lastAlert1 >= COOLDOWN_MS) {
        alertFence();
      }
    } else {
      detectCount1 = 0;
    }
  }

  // --- ตรวจจับ: สระว่ายน้ำ ---
  float dist2 = readDistanceCM(TRIG2_PIN, ECHO2_PIN, PULSIN2_TIMEOUT);
  if (dist2 < 0) {
    detectCount2 = 0;
    Serial.println("[สระว่ายน้ำ] ไม่มีสัญญาณ (timeout)");
  } else {
    Serial.println("[สระว่ายน้ำ] ระยะ: " + String(dist2, 1) + " cm");
    if (dist2 < DETECT2_DIST_CM) {
      detectCount2++;
      Serial.println("[สระว่ายน้ำ] confirm: " + String(detectCount2) + "/" + String(DETECT_CONFIRM));
      if (detectCount2 >= DETECT_CONFIRM && millis() - lastAlert2 >= COOLDOWN_MS) {
        alertPool();
      }
    } else {
      detectCount2 = 0;
    }
  }

  delay(50);
}