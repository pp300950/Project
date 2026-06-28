#define BUTTON_PIN D5  // เปลี่ยนได้ตามสะดวก

bool lastState = HIGH;
bool currentState;

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("=== ทดสอบปุ่มกด ===");
}

void loop() {
  currentState = digitalRead(BUTTON_PIN);

  if (currentState != lastState) {
    delay(50); // debounce

    currentState = digitalRead(BUTTON_PIN);
    if (currentState != lastState) {

      if (currentState == LOW) {
        Serial.println("ON — กดแล้ว");
      } else {
        Serial.println("OFF — ปล่อยแล้ว");
      }

      lastState = currentState;
    }
  }
}