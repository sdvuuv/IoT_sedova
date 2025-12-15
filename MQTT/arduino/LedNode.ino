// КОД ДЛЯ НАНО ПОРТ USB0
#define LED_PIN 13
#define BAUD_RATE 9600

const char CMD_ON = 'u';
const char CMD_OFF = 'd';

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void setLedState(bool state) {
  if (state) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("ACK:LED_ON");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("ACK:LED_OFF");
  }
}

void loop() {
  if (Serial.available() > 0) {
    char receivedChar = Serial.read();
    
    if (receivedChar == CMD_ON) {
      setLedState(true);
    } 
    else if (receivedChar == CMD_OFF) {
      setLedState(false);
    }
  }
}
