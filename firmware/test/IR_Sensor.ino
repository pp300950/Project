#define SENSOR_PIN D6

bool lastState = HIGH;
bool currentState;

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  Serial.println("=== IR Sensor Ready ===");
}

void loop() {
  currentState = digitalRead(SENSOR_PIN);

  if (currentState != lastState) {
    delay(50); // debounce

    currentState = digitalRead(SENSOR_PIN);
    if (currentState != lastState) {

      if (currentState == LOW) {
        Serial.println("[1] ตรวจพบสิ่งกีดขวาง!");
      } else {
        Serial.println("[0] ไม่มีสิ่งกีดขวาง");
      }

      lastState = currentState;
    }
  }
}