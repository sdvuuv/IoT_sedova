#include "Config.h"
#include "WIFI.h"
#include "Server.h"
#include "MQTT.h"

unsigned long lastMsgTime = 0;

void setup(void){
  Serial.begin(115200);
  
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH); 
  
  WIFI_init(true); 
  server_init();
  MQTT_init();
  
  mqtt_cli.publish("esp8266/state", "ESP8266 started and connected!");
}

void loop(void){
  server.handleClient();                   
  
  if (!mqtt_cli.connected()) {
    MQTT_reconnect();
  }
  mqtt_cli.loop();

  unsigned long now = millis();
  if (now - lastMsgTime > 5000) {
    lastMsgTime = now;
    
    int sensorValue = analogRead(A0);
    String payload = String(sensorValue);
    
    mqtt_cli.publish("esp8266/sensor", payload.c_str());
    Serial.println("Published to esp8266/sensor: " + payload);
  }
}