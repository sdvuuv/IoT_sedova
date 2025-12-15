import time
import serial
import random
from paho.mqtt.client import Client
from paho.mqtt.enums import CallbackAPIVersion

BROKER_HOST = 'broker.emqx.io'
TOPIC_LUM = 'matveymftopic/luminosity'
SERIAL_PORT = '/dev/ttyUSB1' 
BAUD_RATE = 9600

def run_gateway():
    client_id = f'SENSOR_NODE_{random.randint(1000, 9999)}'
    mqtt_client = Client(callback_api_version=CallbackAPIVersion.VERSION2, client_id=client_id)
    
    try:
        print(f"Подключение к Arduino на {SERIAL_PORT}...")
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        mqtt_client.connect(BROKER_HOST)
        mqtt_client.loop_start()
        
        print("Чтение данных...")
        
        while True:
            ser.write(b'p')
            time.sleep(0.1)
            
            if ser.in_waiting > 0:
                raw_line = ser.readline().decode('utf-8', errors='ignore').strip()
                
                if "DATA:" in raw_line:
                    try:
                        value = raw_line.split("DATA:")[1]
                        print(f" -> Публикация в MQTT: {value}")
                        mqtt_client.publish(TOPIC_LUM, value, qos=1)
                    except IndexError:
                        pass
                        
            time.sleep(2)

    except serial.SerialException as e:
        print(f"ОШИБКА Serial порта: {e}")
    except KeyboardInterrupt:
        print("\nОстановка...")
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == '__main__':
    run_gateway()