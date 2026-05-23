#ifndef MQTT_H
#define MQTT_H

#include <PubSubClient.h>

PubSubClient mqtt_cli(wifiClient);

volatile bool lamp_on = false;

String mqtt_command_topic() {
  return mqtt_topic_base + "/command";
}
String mqtt_state_topic() {
  return mqtt_topic_base + "/state";
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT message ["); Serial.print(topic); Serial.print("]: ");
  Serial.println(msg);

  msg.trim();
  msg.toUpperCase();
  if (msg == "ON" || msg == "1") {
    lamp_on = true;
  } else if (msg == "OFF" || msg == "0") {
    lamp_on = false;
  }
}

void MQTT_init() {
  mqtt_cli.setServer(mqtt_broker, mqtt_port);
  mqtt_cli.setCallback(mqtt_callback);
}

bool MQTT_ensure_connected() {
  if (mqtt_cli.connected()) return true;

  String client_id = "esp8266-" + id() + "-" + String(random(0xffff), HEX);
  Serial.print("MQTT connect as "); Serial.println(client_id);

  if (mqtt_cli.connect(client_id.c_str())) {
    Serial.println("MQTT connected");
    String cmd = mqtt_command_topic();
    String st  = mqtt_state_topic();
    mqtt_cli.subscribe(cmd.c_str());
    mqtt_cli.publish(st.c_str(), "online");
    Serial.println("Subscribed to: " + cmd);
    Serial.println("Publishing state to: " + st);
    return true;
  } else {
    Serial.print("MQTT connect failed, state=");
    Serial.println(mqtt_cli.state());
    return false;
  }
}

#endif