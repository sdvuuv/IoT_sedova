### Задача:
- Дописать аудиторное 
### Требования:
- При первом включении ESP поднимает собственную точку доступа (режим AP) с известными SSID и паролем.
- Через web-интерфейс пользователь вводит данные своей домашней WiFi-сети.
- После успешного подключения к домашней сети ESP отключает свою точку доступа.
- Учётные данные домашней сети сохраняются в EEPROM — подключение не теряется после перезагрузки микроконтроллера.
- При потере подключения к интернету ESP снова поднимает свою точку доступа.
- Подключение к MQTT-брокеру, подписка на топик команд.
- Индикация состояния устройства встроенным светодиодом (три режима).
- Логика мигания светодиода неблокирующая — без delay() в основном цикле, чтобы web-сервер и MQTT продолжали работать.
- Python-клиент управляет лампочкой по сценарию: время свечения зависит от текущей минуты.

### Индикация состояний:
- Светодиод мигает равномерно медленно — поднята своя точка доступа, нет подключения к интернету.
- Светодиод мигает равномерно быстро — есть подключение к интернету, но нет подключения к брокеру.
- Светодиод мигает кратко два раза и пауза — есть подключение к интернету и к брокеру.

### Рассмотрим код и логику его работы.

##### Структура проекта
```
esp_wifi_lamp/
├── esp_wifi_lamp.ino     // конечный автомат состояний
├── Config.h              // настройки AP, MQTT, раскладка EEPROM
├── WIFI.h                // режимы AP / STA, проверка интернета, EEPROM
├── Server.h              // web-сервер с формой ввода SSID/пароля
├── MQTT.h                // подключение к брокеру, приём команд
└── lamp_client.py        // Python-клиент с расписанием
```

##### Глобальные константы и переменные (Config.h)
```c++
// Параметры собственной точки доступа («на коробке»)
String ssidAP     = "ESP_LAMP";
String passwordAP = "ESP8266123";

// Параметры домашней сети (заполняются пользователем через web)
char ssidCLI[33]     = "";
char passwordCLI[65] = "";

// MQTT
const char* mqtt_broker = "broker.emqx.io";
const int   mqtt_port   = 1883;
String mqtt_topic_base  = "esp_lamp";

// Пины
const int led = 2;   // встроенный светодиод GPIO2

// Раскладка EEPROM
#define EEPROM_SIZE       128
#define EEPROM_FLAG_ADDR  0
#define EEPROM_SSID_ADDR  1
#define EEPROM_PASS_ADDR  34
#define EEPROM_VALID_FLAG 0xA5
```
- ssidAP, passwordAP — данные точки доступа, которые «написаны на коробке»
- ssidCLI, passwordCLI — данные домашней сети, изначально пустые, заполняются из EEPROM или через форму
- mqtt_topic_base — базовый топик, команды приходят в `esp_lamp/command`, состояние публикуется в `esp_lamp/state`
- EEPROM_VALID_FLAG — байт-метка, по которой проверяется, что в памяти лежат валидные данные, а не мусор после прошивки

### Энергонезависимая память (WIFI.h)

##### 1. Сохранение учётных данных
```c++
void EEPROM_saveCreds(const String& ssid, const String& pass) {
  EEPROM.write(EEPROM_FLAG_ADDR, EEPROM_VALID_FLAG);
  for (int i = 0; i < 32; i++)
    EEPROM.write(EEPROM_SSID_ADDR + i, i < (int)ssid.length() ? ssid[i] : 0);
  for (int i = 0; i < 64; i++)
    EEPROM.write(EEPROM_PASS_ADDR + i, i < (int)pass.length() ? pass[i] : 0);
  EEPROM.commit();
}
```
- записывает SSID и пароль домашней сети в EEPROM
- первым байтом ставит флаг валидности, чтобы при следующем запуске понять, что данные есть
- commit() физически переносит данные в флеш-память

##### 2. Загрузка учётных данных
```c++
bool EEPROM_loadCreds(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen) {
  if (EEPROM.read(EEPROM_FLAG_ADDR) != EEPROM_VALID_FLAG) return false;
  // ... чтение SSID и пароля в переданные буферы
  return true;
}
```
- проверяет флаг валидности, и если он не выставлен — возвращает false (данных нет)
- если данные есть — читает их в переданные массивы
- вызывается в setup(), чтобы решить, подключаться сразу или поднимать точку доступа

### Работа с WiFi (WIFI.h)

##### 3. Режим точки доступа
```c++
bool StartAPMode() {
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP((ssidAP + "_" + id()).c_str(), passwordAP.c_str());
  ap_mode = true;
  return true;
}
```
- поднимает собственную точку доступа с фиксированным IP 192.168.4.1
- к имени сети добавляется id() — последние байты MAC-адреса, чтобы разные устройства не конфликтовали по имени

##### 4. Режим клиента
```c++
bool TryStartCLIMode(const char* ssid, const char* pass, uint32_t timeout_ms = 15000) {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) return false;
    delay(250);
  }
  ap_mode = false;
  return true;
}
```
- пытается подключиться к домашней сети с заданным таймаутом
- при успехе возвращает true, при превышении таймаута — false
- именно по результату этой функции принимается решение о переключении режима

##### 5. Проверка доступа в интернет
```c++
bool hasInternet() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  client.setTimeout(2000);
  bool ok = client.connect("1.1.1.1", 80);
  client.stop();
  return ok;
}
```
- проверяет именно доступ в интернет, а не просто подключение к роутеру
- делает TCP-коннект к публичному адресу 1.1.1.1
- нужно для случая, когда WiFi есть, но интернета нет (по требованию устройство должно вернуться в режим AP)

### Web-сервер (Server.h)

##### 6. Форма настройки
```c++
void handleRoot() {
  String html = FPSTR(HTML_FORM);
  html.replace("%DEVICE_ID%", id());
  server.send(200, "text/html; charset=utf-8", html);
}
```
- отдаёт HTML-страницу с формой для ввода SSID и пароля домашней сети
- страница доступна по адресу http://192.168.4.1/ когда устройство в режиме AP

##### 7. Приём введённых данных
```c++
void handleSetWifi() {
  pendingSSID = server.arg("ssid");
  pendingPASS = server.hasArg("pass") ? server.arg("pass") : "";
  credsSubmitted = true;
  server.send(200, "text/html; charset=utf-8", FPSTR(HTML_OK));
}
```
- обрабатывает POST-запрос с данными формы
- извлекает параметры ssid и pass из тела запроса
- выставляет флаг credsSubmitted, по которому основной цикл начнёт попытку подключения

### MQTT (MQTT.h)

##### 8. Приём команд
```c++
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim(); msg.toUpperCase();
  if (msg == "ON"  || msg == "1") lamp_on = true;
  else if (msg == "OFF" || msg == "0") lamp_on = false;
}
```
- вызывается при получении сообщения из топика команд
- принимает команды ON / OFF (или 1 / 0)
- меняет переменную lamp_on, по которой индикация показывает состояние лампочки

##### 9. Подключение к брокеру
```c++
bool MQTT_ensure_connected() {
  if (mqtt_cli.connected()) return true;
  String client_id = "esp8266-" + id() + "-" + String(random(0xffff), HEX);
  if (mqtt_cli.connect(client_id.c_str())) {
    mqtt_cli.subscribe(mqtt_command_topic().c_str());
    mqtt_cli.publish(mqtt_state_topic().c_str(), "online");
    return true;
  }
  return false;
}
```
- неблокирующая попытка подключиться к брокеру
- при успехе подписывается на топик команд и публикует состояние online
- если подключиться не удалось — возвращает false, попытка повторится позже

### Конечный автомат состояний (esp_wifi_lamp.ino)

Устройство в любой момент времени находится в одном из трёх состояний:
```c++
enum DeviceState {
  ST_AP_NO_INTERNET,   // поднята своя AP, ждём настройки
  ST_INTERNET_NO_MQTT, // подключены к WiFi, но к брокеру нет
  ST_FULLY_CONNECTED   // всё подключено
};
```
- каждому состоянию соответствует свой режим индикации светодиода
- переходы между состояниями зависят от наличия интернета и подключения к брокеру

##### Индикация светодиодом
```c++
void updateLED() {
  uint32_t now = millis();
  switch (state) {
    case ST_AP_NO_INTERNET:    // медленное мигание 500/500 мс
    case ST_INTERNET_NO_MQTT:  // быстрое мигание 100/100 мс
    case ST_FULLY_CONNECTED:   // два коротких мигания + пауза
  }
}
```
- логика мигания построена на сравнении millis() с меткой времени, без delay()
- это обязательно: пока светодиод «мигает», web-сервер и MQTT должны продолжать обрабатываться
- встроенный светодиод GPIO2 инвертирован — LOW зажигает, HIGH гасит
- в состоянии ST_FULLY_CONNECTED при команде ON светодиод горит постоянно (показывает, что лампочка включена)

### setup()
```c++
void setup() {
  Serial.begin(115200);
  pinMode(led, OUTPUT);
  EEPROM_setup();

  bool haveCreds = EEPROM_loadCreds(ssidCLI, sizeof(ssidCLI),
                                    passwordCLI, sizeof(passwordCLI));
  bool connected = false;
  if (haveCreds && strlen(ssidCLI) > 0)
    connected = TryStartCLIMode(ssidCLI, passwordCLI, 15000);

  if (connected && hasInternet()) {
    goToConnectedMode();
    MQTT_init();
  } else {
    goToAPMode();
  }
}
```
- читает сохранённые данные из EEPROM
- если данные есть — сразу пытается подключиться к домашней сети
- если подключение удалось и есть интернет — переходит в рабочий режим
- иначе поднимает свою точку доступа и ждёт настройки через web

### loop()
```c++
void loop() {
  updateLED();
  switch (state) {
    case ST_AP_NO_INTERNET:    // server.handleClient(), обработка credsSubmitted
    case ST_INTERNET_NO_MQTT:  // проверка интернета, попытки подключения к MQTT
    case ST_FULLY_CONNECTED:   // mqtt_cli.loop(), контроль интернета
  }
}
```
- на каждой итерации обновляет индикацию светодиода
- в режиме AP обрабатывает web-запросы; если пользователь отправил данные — пытается подключиться, при успехе сохраняет их в EEPROM
- в режиме STA проверяет наличие интернета и периодически пытается подключиться к брокеру
- при потере интернета в любом рабочем состоянии устройство возвращается в режим AP
- при потере подключения к брокеру возвращается из ST_FULLY_CONNECTED в ST_INTERNET_NO_MQTT

### Python-клиент (lamp_client.py)

Клиент управляет лампочкой через MQTT по сценарию, зависящему от времени.

##### Сценарий по времени
```python
def compute_window(minute_index):
    total_steps = (INITIAL_END - INITIAL_START) - MIN_WINDOW + 1
    phase = minute_index % total_steps
    shrink_end   = (phase + 1) // 2
    shrink_start = phase // 2
    start = INITIAL_START + shrink_start
    end   = INITIAL_END   - shrink_end
    return start, end
```
- лампочка горит с 20 по 40 секунду каждой минуты
- каждую следующую минуту время свечения уменьшается на 1 секунду
- минимальное время свечения 10 секунд (с 25 по 35 секунду)
- после минимума окно сбрасывается обратно на 20 секунд и сценарий повторяется

##### Отправка команд
```python
def run_scenario(client):
    while True:
        now = datetime.now()
        win_start, win_end = compute_window(minute_offset)
        desired = "ON" if win_start <= now.second < win_end else "OFF"
        if desired != last_sent:
            client.publish(COMMAND_TOPIC, desired)
        time.sleep(0.2)
```
- каждые 200 мс вычисляет, должна ли лампочка гореть в текущую секунду
- команда отправляется только при смене состояния, чтобы не засорять брокер дубликатами
- ESP, получив команду, зажигает или гасит светодиод