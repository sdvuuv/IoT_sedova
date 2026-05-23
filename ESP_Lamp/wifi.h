#ifndef WIFI_H
#define WIFI_H

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <EEPROM.h>

WiFiClient wifiClient;
String ip = "(IP unset)";

void EEPROM_setup() {
  EEPROM.begin(EEPROM_SIZE);
}

// Сохранить SSID и пароль в EEPROM
void EEPROM_saveCreds(const String& ssid, const String& pass) {
  EEPROM.write(EEPROM_FLAG_ADDR, EEPROM_VALID_FLAG);

  // SSID
  for (int i = 0; i < 32; i++) {
    if (i < (int)ssid.length()) EEPROM.write(EEPROM_SSID_ADDR + i, ssid[i]);
    else                        EEPROM.write(EEPROM_SSID_ADDR + i, 0);
  }
  EEPROM.write(EEPROM_SSID_ADDR + 32, 0);

  // Password
  for (int i = 0; i < 64; i++) {
    if (i < (int)pass.length()) EEPROM.write(EEPROM_PASS_ADDR + i, pass[i]);
    else                        EEPROM.write(EEPROM_PASS_ADDR + i, 0);
  }
  EEPROM.write(EEPROM_PASS_ADDR + 64, 0);

  EEPROM.commit();
  Serial.println("Credentials saved to EEPROM");
}

bool EEPROM_loadCreds(char* ssidOut, size_t ssidLen, char* passOut, size_t passLen) {
  if (EEPROM.read(EEPROM_FLAG_ADDR) != EEPROM_VALID_FLAG) {
    return false;
  }
  for (size_t i = 0; i < ssidLen - 1; i++) {
    ssidOut[i] = EEPROM.read(EEPROM_SSID_ADDR + i);
    if (ssidOut[i] == 0) break;
  }
  ssidOut[ssidLen - 1] = 0;

  for (size_t i = 0; i < passLen - 1; i++) {
    passOut[i] = EEPROM.read(EEPROM_PASS_ADDR + i);
    if (passOut[i] == 0) break;
  }
  passOut[passLen - 1] = 0;
  return true;
}

void EEPROM_clearCreds() {
  EEPROM.write(EEPROM_FLAG_ADDR, 0xFF);
  EEPROM.commit();
  Serial.println("EEPROM credentials cleared");
}

bool ap_mode = true;

String id() {
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  return macID;
}

bool StartAPMode() {
  IPAddress apIP(192, 168, 4, 1);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  String apName = ssidAP + "_" + id();
  WiFi.softAP(apName.c_str(), passwordAP.c_str());
  ip = WiFi.softAPIP().toString();
  ap_mode = true;
  Serial.println("");
  Serial.println("WiFi up in AP mode with name: " + apName);
  Serial.println("AP IP: " + ip);
  return true;
}

bool TryStartCLIMode(const char* ssid, const char* pass, uint32_t timeout_ms = 15000) {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) {
      Serial.println("\nWiFi connection timeout");
      return false;
    }
    delay(250);
    Serial.print(".");
  }
  ip = WiFi.localIP().toString();
  ap_mode = false;
  Serial.println("");
  Serial.println("Connected. Local IP: " + ip);
  return true;
}

bool hasInternet() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  client.setTimeout(2000);
  bool ok = client.connect("1.1.1.1", 80);
  client.stop();
  return ok;
}

#endif