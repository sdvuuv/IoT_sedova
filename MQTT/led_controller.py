import time
import serial
import random
from paho.mqtt.client import Client, MQTTMessage
from paho.mqtt.enums import CallbackAPIVersion

# Настройки
BROKER_HOST = 'broker.emqx.io'
TOPIC_LUM = 'matveymftopic/luminosity'
TOPIC_STATE = 'matveymftopic/led_state'
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 9600

THRESHOLD = 40 

ser_connection = None
current_led_status = 'UNKNOWN'

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("Успешное подключение к MQTT")
        client.subscribe(TOPIC_LUM, qos=1)
    else:
        print(f"Ошибка подключения: код {reason_code}")

def on_message(client, userdata, msg: MQTTMessage):
    global current_led_status
    try:
        payload = msg.payload.decode('utf-8')
        sensor_val = int(payload)
        print(f"Получено значение света: {sensor_val}")
        
        command = None
        
        if sensor_val < THRESHOLD and current_led_status != 'ON':
            print("Свет слишком слабый -> Включаем светодиод")
            command = b'u'
            current_led_status = 'ON'
            
        elif sensor_val >= THRESHOLD and current_led_status != 'OFF':
            print("Свет достаточный -> Выключаем светодиод")
            command = b'd'
            current_led_status = 'OFF'
            
        if command and ser_connection and ser_connection.is_open:
            ser_connection.write(command)
            time.sleep(0.1)
            if ser_connection.in_waiting:
                response = ser_connection.readline().decode().strip()
                client.publish(TOPIC_STATE, response)

    except ValueError:
        print("Ошибка преобразования данных")

if __name__ == '__main__':
    try:
        print(f"Открытие порта {SERIAL_PORT}...")
        ser_connection = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        
        ser_connection.write(b'd')
        current_led_status = 'OFF'

        client_id = f'ACTUATOR_NODE_{random.randint(1000, 9999)}'
        client = Client(callback_api_version=CallbackAPIVersion.VERSION2, client_id=client_id)
        client.on_connect = on_connect
        client.on_message = on_message
        
        client.connect(BROKER_HOST)
        print("Ожидание сообщений...")
        client.loop_forever()

    except serial.SerialException as e:
        print(f"Не удалось открыть порт {SERIAL_PORT}. Проверьте подключение.")
    except KeyboardInterrupt:
        print("\nВыход...")
    finally:
        if ser_connection and ser_connection.is_open:
            ser_connection.close()