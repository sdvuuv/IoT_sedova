#include <NewPing.h>

#define TRIGGER_PIN  9
#define ECHO_PIN     8
#define MAX_DISTANCE 200 // Максимальное расстояние для УЗ датчика
#define IR_PIN       A0  // Пин ИК-датчика
#define LED_PIN      13  // Пин индикации

NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

bool isCalibrating = false;
int minRange = 0;
int maxRange = 0;

bool collectedDistances[101]; 
int pointsCollected = 0;
int totalRequiredPoints = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  resetCalibration();
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    if (command.startsWith("CALIBRATE")) {
      sscanf(command.c_str(), "CALIBRATE %d %d", &minRange, &maxRange);
      if (minRange > 0 && maxRange <= 100 && minRange < maxRange) {
        startCalibration();
      }
    }
  }

  if (isCalibrating) {
    delay(50); // Небольшая задержка между измерениями
    int usDistance = sonar.ping_cm(); // Измеряем эталонное расстояние
    int irAnalogValue = analogRead(IR_PIN); // Читаем ИК-датчик

    // Если объект в заданном диапазоне
    if (usDistance >= minRange && usDistance <= maxRange) {
      
      // Если для этого расстояния еще нет данных
      if (!collectedDistances[usDistance]) {
        collectedDistances[usDistance] = true;
        pointsCollected++;
        
        // Сигнализируем пользователю миганием
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);

        // Отправляем сырые данные на ПК
        Serial.print("DATA,");
        Serial.print(usDistance);
        Serial.print(",");
        Serial.println(irAnalogValue);

        checkCalibrationCompleteness();
      }
    }
  }
}

void startCalibration() {
  resetCalibration();
  totalRequiredPoints = maxRange - minRange + 1;
  isCalibrating = true;
  Serial.println("STATUS,STARTED");
}

void resetCalibration() {
  for (int i = 0; i <= 100; i++) collectedDistances[i] = false;
  pointsCollected = 0;
  isCalibrating = false;
  digitalWrite(LED_PIN, LOW);
}

void checkCalibrationCompleteness() {
  //  Покрытие 90%
  float coverage = (float)pointsCollected / totalRequiredPoints;
  if (coverage < 0.9) return;

  // Нет пропусков более 2 см 
  int lastCollected = -1;
  for (int i = minRange; i <= maxRange; i++) {
    if (collectedDistances[i]) {
      if (lastCollected != -1 && (i - lastCollected) > 2) {
        return; // Нашли недопустимый пропуск, ждем дальше
      }
      lastCollected = i;
    }
  }

  isCalibrating = false;
  digitalWrite(LED_PIN, HIGH); 
  Serial.println("STATUS,DONE");
}