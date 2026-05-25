// ===================== ПИНЫ МОТОРОВ (MotorShield) =====================
#define SPEED_LEFT   5
#define SPEED_RIGHT  6
#define DIR_LEFT     4
#define DIR_RIGHT    7

#define FORWARD_LEFT   LOW
#define BACKWARD_LEFT  HIGH
#define FORWARD_RIGHT  LOW
#define BACKWARD_RIGHT HIGH

// ===================== ПИНЫ ДАТЧИКОВ (HC-SR04) =====================
// Передний датчик
#define TRIG_FRONT  8
#define ECHO_FRONT  9
// Левый датчик 
#define TRIG_LEFT   10
#define ECHO_LEFT   11

// ===================== ПАРАМЕТРЫ ЛАБИРИНТА =====================
const float CD = 15.0;   // целевое расстояние до боковой стены, см
const float FD = 300.0;  // порог "впереди/слева свободно", см
const int   BASE_SPEED   = 150;  // базовая скорость движения
const int   ROTATE_SPEED = 150;  // скорость разворота на месте

const unsigned long TURN_90_MS = 1250;
const unsigned long CD_MS      = 500;  

// ===================== PID для движения вдоль левой стены =====================
float Kp = 6.0;
float Ki = 0.0;
float Kd = 3.0;
float pid_integral = 0;
float pid_prev_err = 0;

// ===================== СОСТОЯНИЯ =====================
enum State { SEARCH_WALL, FORWARD, TURN_LEFT, ROTATE_RIGHT, STOP };
State state = SEARCH_WALL;

unsigned long state_timer = 0;   // момент входа в состояние с таймером

// ===================== УПРАВЛЕНИЕ МОТОРАМИ =====================
void move(bool left_dir, int left_speed, bool right_dir, int right_speed) {
  left_speed  = constrain(left_speed, 0, 255);
  right_speed = constrain(right_speed, 0, 255);
  digitalWrite(DIR_LEFT,  left_dir);
  digitalWrite(DIR_RIGHT, right_dir);
  analogWrite(SPEED_LEFT,  left_speed);
  analogWrite(SPEED_RIGHT, right_speed);
}

void forward(int speed) {
  move(FORWARD_LEFT, speed, FORWARD_RIGHT, speed);
}

void backward(int speed) {
  move(BACKWARD_LEFT, speed, BACKWARD_RIGHT, speed);
}

// корректирующее движение: correction > 0 -> уводит вправо (от левой стены)
void forward_corrected(int speed, int correction) {
  int left  = speed + correction;
  int right = speed - correction;
  move(FORWARD_LEFT, left, FORWARD_RIGHT, right);
}

void rotate_left(int speed) {
  move(BACKWARD_LEFT, speed, FORWARD_RIGHT, speed);
}

void rotate_right(int speed) {
  move(FORWARD_LEFT, speed, BACKWARD_RIGHT, speed);
}

void stop_motors() {
  move(FORWARD_LEFT, 0, FORWARD_RIGHT, 0);
}

// ===================== ЧТЕНИЕ ДАТЧИКОВ =====================
float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return FD; 
  return duration * 0.0343 / 2.0; // см
}

// ===================== PID-КОРРЕКЦИЯ =====================
int computePID(float ld) {
  float error = ld - CD;          // > 0 -> мы далеко от стены -> надо ближе (влево)
  pid_integral += error;
  pid_integral = constrain(pid_integral, -50, 50);
  float derivative = error - pid_prev_err;
  pid_prev_err = error;

  float output = Kp * error + Ki * pid_integral + Kd * derivative;
  int correction = (int)(-output);
  return constrain(correction, -BASE_SPEED, BASE_SPEED);
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(9600);

  pinMode(SPEED_LEFT, OUTPUT);
  pinMode(SPEED_RIGHT, OUTPUT);
  pinMode(DIR_LEFT, OUTPUT);
  pinMode(DIR_RIGHT, OUTPUT);

  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);

  stop_motors();
  delay(1000); // пауза перед стартом
}

void loop() {
  float fd = readDistance(TRIG_FRONT, ECHO_FRONT);
  float ld = readDistance(TRIG_LEFT,  ECHO_LEFT);

  Serial.print("STATE="); Serial.print(state);
  Serial.print(" fd=");   Serial.print(fd);
  Serial.print(" ld=");   Serial.println(ld);

  switch (state) {

    case SEARCH_WALL:
      // ищем левую стену: едем вперёд пока её нет
      forward(BASE_SPEED);
      if (ld <= FD) {
        state = FORWARD;
        pid_integral = 0;
        pid_prev_err = 0;
      }
      break;

    case FORWARD:
      if (ld > FD) {
        // слева открылся проход -> поворот налево
        state_timer = millis();
        state = TURN_LEFT;
      } else if (fd < CD) {
        // впереди стена, слева стена -> разворот вправо
        state_timer = millis();
        state = ROTATE_RIGHT;
      } else {
        // едем вперёд, держим дистанцию CD до левой стены
        int correction = computePID(ld);
        forward_corrected(BASE_SPEED, correction);
      }
      break;

    case TURN_LEFT:
      // сначала чуть проезжаем вперёд, затем крутим 90°
      if (millis() - state_timer < CD_MS) {
        forward(BASE_SPEED);
      } else if (millis() - state_timer < CD_MS + TURN_90_MS) {
        rotate_left(ROTATE_SPEED);
      } else {
        state = FORWARD;
        pid_integral = 0;
        pid_prev_err = 0;
      }
      break;

    case ROTATE_RIGHT:
      if (millis() - state_timer < TURN_90_MS) {
        rotate_right(ROTATE_SPEED);
      } else {
        state = FORWARD;
        pid_integral = 0;
        pid_prev_err = 0;
      }
      break;

    case STOP:
      stop_motors();
      break;
  }

  delay(30); 
}