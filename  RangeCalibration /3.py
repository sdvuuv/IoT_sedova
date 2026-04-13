import serial
import time
import numpy as np
import matplotlib.pyplot as plt

SERIAL_PORT = 'dev/ttyUSB0'
BAUD_RATE = 115200
MIN_DIST = 10
MAX_DIST = 30

def collect_data():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2) 
    except Exception as e:
        print(f"Ошибка подключения: {e}")
        return None, None

    ser.write(f"CALIBRATE {MIN_DIST} {MAX_DIST}\n".encode())

    distances = []
    analog_vals = []

    
    while True:
        line = ser.readline().decode('utf-8').strip()
        if not line:
            continue
            
        if line.startswith("DATA"):
            _, dist, analog = line.split(",")
            distances.append(int(dist))
            analog_vals.append(int(analog))
            print(f"Расстояние={dist} см, АЦП={analog}")
            
        elif line == "STATUS,DONE":
            print("Сбор данных завершен")
            break

    ser.close()
    return np.array(analog_vals), np.array(distances)

def plot_models(x, y):
    sort_idx = np.argsort(x)
    x_sorted = x[sort_idx]
    y_sorted = y[sort_idx]

    plt.figure(figsize=(14, 6))

    plt.subplot(1, 2, 1)
    coeffs_1 = np.polyfit(x, y, 1)
    poly_1 = np.poly1d(coeffs_1)
    plt.scatter(x, y, color='none', edgecolor='red', label='Сырые данные')
    plt.plot(x_sorted, poly_1(x_sorted), color='magenta', label=f'Степень 1')
    plt.title("Линейная модель (Степень 1)")
    plt.xlabel("Analog value (Predictor)")
    plt.ylabel("Distance in cm (Response)")
    plt.legend()

    plt.subplot(1, 2, 2)
    coeffs_3 = np.polyfit(x, y, 3) 
    poly_3 = np.poly1d(coeffs_3)
    plt.scatter(x, y, color='none', edgecolor='red', label='Сырые данные')
    
    x_smooth = np.linspace(x.min(), x.max(), 100)
    plt.plot(x_smooth, poly_3(x_smooth), color='magenta', label=f'Степень 3')
    plt.show()


    print("Функция перевода (примерно): Distance = {}*V^3 + {}*V^2 + {}*V + {}".format(
        round(coeffs_3[0], 6), round(coeffs_3[1], 6), round(coeffs_3[2], 6), round(coeffs_3[3], 6)
    ))

if __name__ == "__main__":
    X_analog, Y_distance = collect_data()
    if X_analog is not None and len(X_analog) > 0:
        plot_models(X_analog, Y_distance)
    else:
        print("Данные не собраны.")