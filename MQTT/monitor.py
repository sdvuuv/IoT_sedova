import time
import random
from paho.mqtt.client import Client, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion

BROKER_HOST = 'broker.emqx.io'
TOPIC_LUM = 'matveymftopic/luminosity'
TOPIC_STATE = 'matveymftopic/led_state'

THRESHOLD = 40

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Подключено.")
        client.subscribe([(TOPIC_LUM, 1), (TOPIC_STATE, 1)])
    else:
        print(f"Ошибка подключения: {reason_code}")

def on_message(client, userdata, msg: MQTTMessage):
    try:
        topic = msg.topic
        payload = msg.payload.decode('utf-8')
        timestamp = time.strftime("%H:%M:%S")

        if topic == TOPIC_LUM:
            val = int(payload)
            print(f"\n[{timestamp}] СЕНСОР: {val}")
            if val < THRESHOLD:
                print(f"Темно ({val} < {THRESHOLD}). Ожидаю включение светодиода...")
            else:
                print(f"Светло ({val} >= {THRESHOLD}). Ожидаю выключение светодиода...")

        elif topic == TOPIC_STATE:
            if "LED_ON" in payload:
                print("Светодиод ВКЛЮЧЕН")
            elif "LED_OFF" in payload:
                print("Светодиод ВЫКЛЮЧЕН")

    except Exception as e:
        print(f"Ошибка: {e}")

if __name__ == '__main__':
    client_id = f'MONITOR_ADMIN_{random.randint(1000, 9999)}'
    client = Client(callback_api_version=CallbackAPIVersion.VERSION2, client_id=client_id)
    
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER_HOST)
        client.loop_forever()
    except KeyboardInterrupt:
        print("Завершение работы монитора.")
        client.disconnect()