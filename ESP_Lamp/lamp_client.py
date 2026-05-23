import time
from datetime import datetime
 
import paho.mqtt.client as mqtt
 
 
BROKER = "broker.emqx.io"
PORT = 1883
 
COMMAND_TOPIC = "esp_lamp/command"
STATE_TOPIC   = "esp_lamp/state"
 
# Параметры сценария
INITIAL_START = 20
INITIAL_END   = 40   # exclusive
MIN_WINDOW    = 10   # минимальная ширина окна в секундах
 
 
def compute_window(minute_index: int) -> tuple[int, int]:
    initial_width = INITIAL_END - INITIAL_START          # 20
    total_steps   = initial_width - MIN_WINDOW + 1       # 11 (от 20 до 10 включительно)
    phase = minute_index % total_steps                   # 0..10
 
    shrink_end   = (phase + 1) // 2  # 0,1,1,2,2,3,3,4,4,5,5
    shrink_start = phase // 2        # 0,0,1,1,2,2,3,3,4,4,5
    start = INITIAL_START + shrink_start
    end   = INITIAL_END   - shrink_end
    return start, end
 
 
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Connected to {BROKER}:{PORT}")
        client.subscribe(STATE_TOPIC)
        print(f"[MQTT] Subscribed to state topic: {STATE_TOPIC}")
    else:
        print(f"[MQTT] Connect failed, rc={rc}")
 
 
def on_message(client, userdata, msg):
    payload = msg.payload.decode(errors="replace")
    print(f"[MQTT<-] {msg.topic}: {payload}")
 
 
def run_scenario(client: mqtt.Client):
    print(f"[scenario] Publishing commands to: {COMMAND_TOPIC}")
    print("[scenario] Schedule (one phase per minute, then repeat):")
    for i in range(11):
        s, e = compute_window(i)
        print(f"  phase {i:2d}: lamp ON from sec {s} to sec {e-1} "
              f"(width = {e-s}s)")
    print("[scenario] Running. Ctrl+C to stop.\n")
 
    last_sent = None
 
    # Минута старта — точка отсчёта фазы сценария
    start_minute = datetime.now().minute
 
    while True:
        now = datetime.now()
        sec = now.second
        minute_offset = (now.minute - start_minute) % 60
        win_start, win_end = compute_window(minute_offset)
        should_be_on = win_start <= sec < win_end
 
        desired = "ON" if should_be_on else "OFF"
        if desired != last_sent:
            client.publish(COMMAND_TOPIC, desired)
            print(f"[{now.strftime('%H:%M:%S')}] phase={minute_offset} "
                  f"window=[{win_start},{win_end}) -> {desired}")
            last_sent = desired
 
        time.sleep(0.2)
 
 
def main():
    print(f"[setup] Command topic: {COMMAND_TOPIC}")
    print(f"[setup] State topic:   {STATE_TOPIC}")
 
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
 
    client.connect(BROKER, PORT, keepalive=30)
    client.loop_start()
 
    try:
        run_scenario(client)
    except KeyboardInterrupt:
        print("\n[exit] Stopping...")
        client.publish(COMMAND_TOPIC, "OFF")
        time.sleep(0.3)
    finally:
        client.loop_stop()
        client.disconnect()
 
 
if __name__ == "__main__":
    main()
