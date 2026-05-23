#ifndef CONFIG_H
#define CONFIG_H

String ssidAP     = "ESP_LAMP";       // имя точки доступа ESP
String passwordAP = "ESP8266123";     // пароль точки доступа ESP
// http://192.168.4.1/

char ssidCLI[33]     = "";
char passwordCLI[65] = "";

// ===== MQTT =====
const char* mqtt_broker = "broker.emqx.io";
const int   mqtt_port   = 1883;

// Базовый топик для управления лампочкой.
// Топик команд:   <mqtt_topic_base>/command
// Топик состояния: <mqtt_topic_base>/state
String mqtt_topic_base = "esp_lamp";

// ===== Пины =====
const int led        = 2;   
const int lamp_led   = 2;  
                            // (когда устройство полностью подключено — он управляется по MQTT)

// ===== EEPROM раскладка =====
// [0]        — флаг валидности (0xA5 = данные записаны)
// [1..33]    — SSID (32 символа + \0)
// [34..98]   — пароль (64 символа + \0)
#define EEPROM_SIZE      128
#define EEPROM_FLAG_ADDR 0
#define EEPROM_SSID_ADDR 1
#define EEPROM_PASS_ADDR 34
#define EEPROM_VALID_FLAG 0xA5

#endif