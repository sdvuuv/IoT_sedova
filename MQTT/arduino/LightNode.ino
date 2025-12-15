// КОД ДЛЯ УНО ПОРТ USB1

#define SENSOR_PIN A0
#define BAUD_RATE 9600

const char CMD_GET_DATA = 'p';
const char CMD_STREAM_START = 's';

bool isStreaming = false;

void setup() {
  Serial.begin(BAUD_RATE);
  pinMode(SENSOR_PIN, INPUT);
}

void sendSensorData() {
  int rawValue = analogRead(SENSOR_PIN);
  Serial.print("DATA:");
  Serial.println(rawValue);
}

void processIncomingCommand() {
  if (Serial.available() > 0) {
    char incomingByte = Serial.read();
    
    switch (incomingByte) {
      case CMD_GET_DATA:
        isStreaming = false;
        sendSensorData();
        break;
        
      case CMD_STREAM_START:
        isStreaming = true;
        Serial.println("MSG:Stream_Mode_Enabled");
        break;
    }
  }
}

void loop() {
  processIncomingCommand();
  
  if (isStreaming) {
    sendSensorData();
    delay(100);
  }
}
