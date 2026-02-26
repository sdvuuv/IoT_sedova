#include <PubSubClient.h>

PubSubClient mqtt_cli(wifiClient);

void callback(char *topic, byte *payload, unsigned int length) {

    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);

    if (String(topic) == "esp8266/command") {
        if (message == "ON") {
            digitalWrite(led, LOW);  
            Serial.println("LED turned ON");
        } 
        else if (message == "OFF") {
            digitalWrite(led, HIGH);
            Serial.println("LED turned OFF");
        }
    }
}

void MQTT_reconnect() {
    while (!mqtt_cli.connected()) {
        String client_id = "esp8266-" + String(WiFi.macAddress());
        Serial.print("Attempting MQTT connection as " + client_id + "...");
        
        if (mqtt_cli.connect(client_id.c_str())) {
            Serial.println("Connected!");
            mqtt_cli.subscribe("esp8266/command");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_cli.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void MQTT_init(){
    mqtt_cli.setServer(mqtt_broker, mqtt_port);
    mqtt_cli.setCallback(callback);
    MQTT_reconnect(); 
}