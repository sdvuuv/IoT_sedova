### Задача:
Используя данные с ультразвукового сенсора. Разработать метод и код для калибровки инфракрасного дальномера. Т.е. код который позволит получить данные что бы воспроизвести данные с графика ниже.
### Рассмотрим код и логику его работы.
Ссылка на видео [Появится когда я куплю новый дальномер потому что выданный я сожгла]

##### 1. Прошивка сенсорного узла (3.ino)

Микроконтроллер работает как ведомое устройство, выполняя калибровку по команде с ПК.

```cpp
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
    int usDistance = sonar.ping_cm();
    int irAnalogValue = analogRead(IR_PIN);

    if (usDistance >= minRange && usDistance <= maxRange) {
      if (!collectedDistances[usDistance]) {
        collectedDistances[usDistance] = true;
        pointsCollected++;

        Serial.print("DATA,");
        Serial.print(usDistance);
        Serial.print(",");
        Serial.println(irAnalogValue);

        checkCalibrationCompleteness();
      }
    }
  }
}
```

- принимает команду `CALIBRATE <min> <max>` по UART и запускает процесс калибровки
- для каждого уникального расстояния фиксирует одну пару значений: `usDistance` - эталон от УЗ-датчика, `irAnalogValue` - сырой АЦП с ИК-датчика
- предотвращает дублирование точек через булев массив `collectedDistances[101]`
- подаёт световую сигнализацию (мигание LED) при каждом успешно зафиксированном измерении
- отправляет данные в формате `DATA,<расстояние>,<аналог>` для простого парсинга на стороне ПК

```cpp
void checkCalibrationCompleteness() {
  float coverage = (float)pointsCollected / totalRequiredPoints;
  if (coverage < 0.9) return;

  int lastCollected = -1;
  for (int i = minRange; i <= maxRange; i++) {
    if (collectedDistances[i]) {
      if (lastCollected != -1 && (i - lastCollected) > 2) {
        return;
      }
      lastCollected = i;
    }
  }

  isCalibrating = false;
  digitalWrite(LED_PIN, HIGH);
  Serial.println("STATUS,DONE");
}
```

- завершение калибровки определяется двумя критериями: покрытие диапазона не менее 90% и отсутствие пропусков более 2 см подряд
- при выполнении обоих условий отправляет `STATUS,DONE` и переводит LED в постоянное свечение как визуальное подтверждение завершения

##### 2. Скрипт сбора и анализа данных (3.py)

Скрипт выполняет роль управляющего узла: инициирует калибровку, накапливает данные по Serial и строит регрессионные модели.

```python
def collect_data():
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)

    ser.write(f"CALIBRATE {MIN_DIST} {MAX_DIST}\n".encode())

    while True:
        line = ser.readline().decode('utf-8').strip()
        if not line:
            continue

        if line.startswith("DATA"):
            _, dist, analog = line.split(",")
            distances.append(int(dist))
            analog_vals.append(int(analog))

        elif line == "STATUS,DONE":
            break

    ser.close()
    return np.array(analog_vals), np.array(distances)
```

- после установки соединения делает паузу 2 секунды для инициализации Arduino
- отправляет команду `CALIBRATE` с заданными границами диапазона
- в цикле читает строки из Serial, парсит только строки с маркером `DATA`, игнорируя прочие служебные сообщения
- завершает сбор по сигналу `STATUS,DONE` и возвращает два массива NumPy для дальнейшего анализа

```python
def plot_models(x, y):
    coeffs_1 = np.polyfit(x, y, 1)
    poly_1 = np.poly1d(coeffs_1)

    coeffs_3 = np.polyfit(x, y, 3)
    poly_3 = np.poly1d(coeffs_3)

    print("Функция перевода (примерно): Distance = {}*V^3 + {}*V^2 + {}*V + {}".format(
        round(coeffs_3[0], 6), round(coeffs_3[1], 6),
        round(coeffs_3[2], 6), round(coeffs_3[3], 6)
    ))
```

- строит две модели: линейную (степень 1) и кубическую (степень 3) с помощью `np.polyfit`
- отображает обе модели на одном холсте в виде двух subplot для визуального сравнения качества аппроксимации
- выводит коэффициенты кубического полинома в виде формулы перевода АЦП => расстояние - результат, пригодный для использования в прошивке