import paho.mqtt.client as mqtt
import time
import threading

BROKER = "broker.emqx.io"
PORT = 1883
TOPIC_SUB = "esp8266/sensor"
TOPIC_PUB = "esp8266/command"

def on_connect(client, userdata, flags, rc):
    print(f"Подключено к брокеру (код: {rc})")
    client.subscribe(TOPIC_SUB)
    print(f"Подписка на топик: {TOPIC_SUB}")

def on_message(client, userdata, msg):
    print(f"📥 Получено от ESP [Топик {msg.topic}]: {msg.payload.decode('utf-8')}")

# Настройка клиента
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

print("Подключение к брокеру...")
client.connect(BROKER, PORT, 60)
client.loop_start()

try:
    print("Введите 'ON' чтобы включить LED, 'OFF' чтобы выключить.")
    
    while True:
        cmd = input("Команда на ESP: ").strip().upper()
        if cmd in ["ON", "OFF"]:
            client.publish(TOPIC_PUB, cmd)
            print(f"Отправлено: {cmd}")
        else:
            print("Неизвестная команда")
        time.sleep(1)

except KeyboardInterrupt:
    print("Выход...")
    client.loop_stop()
    client.disconnect()