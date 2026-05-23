#include "Config.h"
#include "WIFI.h"
#include "Server.h"
#include "MQTT.h"

// ===== Состояния устройства =====
enum DeviceState {
  ST_AP_NO_INTERNET,   
  ST_INTERNET_NO_MQTT,
  ST_FULLY_CONNECTED 
};

DeviceState state = ST_AP_NO_INTERNET;

inline void ledOn()  { digitalWrite(led, LOW); }
inline void ledOff() { digitalWrite(led, HIGH); }

uint32_t ledTimer = 0;
int ledPhase = 0; 

void updateLED() {
  uint32_t now = millis();

  switch (state) {
    case ST_AP_NO_INTERNET: {
      // равномерное медленное мигание: 500 мс вкл / 500 мс выкл
      if (now - ledTimer >= 500) {
        ledTimer = now;
        ledPhase = !ledPhase;
        if (ledPhase) ledOn(); else ledOff();
      }
      break;
    }

    case ST_INTERNET_NO_MQTT: {
      // равномерное быстрое мигание: 100 мс / 100 мс
      if (now - ledTimer >= 100) {
        ledTimer = now;
        ledPhase = !ledPhase;
        if (ledPhase) ledOn(); else ledOff();
      }
      break;
    }

    case ST_FULLY_CONNECTED: {
      // Если лампочка включена по MQTT — светодиод просто горит постоянно.
      if (lamp_on) {
        ledOn();
        ledTimer = now;
        ledPhase = 0;
        break;
      }
      static const uint32_t durations[4] = {100, 150, 100, 900};
      static const bool     levels[4]    = {true, false, true, false};
      if (now - ledTimer >= durations[ledPhase]) {
        ledTimer = now;
        ledPhase = (ledPhase + 1) % 4;
        if (levels[ledPhase]) ledOn(); else ledOff();
      }
      break;
    }
  }
}

// ===== Логика переходов между состояниями =====

uint32_t lastInternetCheck = 0;
const uint32_t INTERNET_CHECK_INTERVAL = 10000; // каждые 10 сек

uint32_t lastMqttRetry = 0;
const uint32_t MQTT_RETRY_INTERVAL = 5000;      // ретраить mqtt раз в 5 сек

void goToAPMode() {
  Serial.println(">> Switching to AP mode");
  StartAPMode();
  server_init();
  state = ST_AP_NO_INTERNET;
  ledTimer = 0;
  ledPhase = 0;
}

void goToConnectedMode() {
  Serial.println(">> Connected to WiFi, switching to STA mode");
  server.stop();              // выключаем web-сервер настройки
  state = ST_INTERNET_NO_MQTT;
  ledTimer = 0;
  ledPhase = 0;
  // MQTT попробуем подключить в loop()
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== ESP WiFi Lamp boot ===");

  pinMode(led, OUTPUT);
  ledOff();

  EEPROM_setup();

  bool haveCreds = EEPROM_loadCreds(ssidCLI, sizeof(ssidCLI),
                                    passwordCLI, sizeof(passwordCLI));

  bool connected = false;
  if (haveCreds && strlen(ssidCLI) > 0) {
    Serial.println("Found credentials in EEPROM:");
    Serial.print("  SSID: "); Serial.println(ssidCLI);
    connected = TryStartCLIMode(ssidCLI, passwordCLI, 15000);
  } else {
    Serial.println("No credentials in EEPROM — going to AP mode");
  }

  if (connected) {
    if (hasInternet()) {
      goToConnectedMode();
      MQTT_init();
    } else {
      Serial.println("Connected to WiFi but no internet — going AP");
      goToAPMode();
    }
  } else {
    goToAPMode();
  }
}

void loop() {
  updateLED();

  switch (state) {

    case ST_AP_NO_INTERNET: {
      server.handleClient();

      if (credsSubmitted) {
        credsSubmitted = false;
        Serial.println("Trying user-provided credentials...");

        // Копируем в глобальные переменные
        pendingSSID.toCharArray(ssidCLI, sizeof(ssidCLI));
        pendingPASS.toCharArray(passwordCLI, sizeof(passwordCLI));

        // Даём web-серверу время отдать страницу-ответ
        for (int i = 0; i < 20; i++) { server.handleClient(); delay(50); }

        if (TryStartCLIMode(ssidCLI, passwordCLI, 15000)) {
          if (hasInternet()) {
            EEPROM_saveCreds(pendingSSID, pendingPASS);
            goToConnectedMode();
            MQTT_init();
          } else {
            Serial.println("WiFi OK but no internet — back to AP");
            goToAPMode();
          }
        } else {
          Serial.println("Failed to connect — staying in AP mode");
          goToAPMode();
        }
      }
      break;
    }

    case ST_INTERNET_NO_MQTT: {
      uint32_t now = millis();
      if (now - lastInternetCheck > INTERNET_CHECK_INTERVAL) {
        lastInternetCheck = now;
        if (!hasInternet()) {
          Serial.println("Lost internet — back to AP");
          goToAPMode();
          break;
        }
      }
      // Пробуем подключиться к MQTT
      if (now - lastMqttRetry > MQTT_RETRY_INTERVAL) {
        lastMqttRetry = now;
        if (MQTT_ensure_connected()) {
          state = ST_FULLY_CONNECTED;
          ledTimer = 0;
          ledPhase = 0;
        }
      }
      break;
    }

    case ST_FULLY_CONNECTED: {
      // MQTT loop
      if (!mqtt_cli.connected()) {
        Serial.println("MQTT lost — back to ST_INTERNET_NO_MQTT");
        state = ST_INTERNET_NO_MQTT;
        ledTimer = 0;
        ledPhase = 0;
        break;
      }
      mqtt_cli.loop();

      uint32_t now = millis();
      if (now - lastInternetCheck > INTERNET_CHECK_INTERVAL) {
        lastInternetCheck = now;
        if (WiFi.status() != WL_CONNECTED || !hasInternet()) {
          Serial.println("Lost internet — back to AP");
          mqtt_cli.disconnect();
          goToAPMode();
        }
      }
      break;
    }
  }
}