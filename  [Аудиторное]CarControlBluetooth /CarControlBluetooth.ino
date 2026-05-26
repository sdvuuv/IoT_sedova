#include <SoftwareSerial.h>

SoftwareSerial bt(2, 3);

const uint8_t LEFT_EN  = 6;
const uint8_t LEFT_IN1 = 8;
const uint8_t LEFT_IN2 = 9;

const uint8_t RIGHT_EN  = 5;
const uint8_t RIGHT_IN1 = 11;
const uint8_t RIGHT_IN2 = 10;

const bool LEFT_INVERTED  = false;
const bool RIGHT_INVERTED = true;

const uint8_t SPEED_FORWARD_LEFT   = 190;
const uint8_t SPEED_FORWARD_RIGHT  = 180;
const uint8_t SPEED_BACKWARD_LEFT  = 190;
const uint8_t SPEED_BACKWARD_RIGHT = 180;
const uint8_t SPEED_TURN           = 170;

const unsigned long COMMAND_TIMEOUT = 700;
unsigned long lastCommandTime = 0;
char currentCommand = 'S';

void setMotor(uint8_t en, uint8_t in1, uint8_t in2, int speed, bool inverted) {
  if (inverted) {
    speed = -speed;
  }
  speed = constrain(speed, -255, 255);   
  if (speed > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(en, speed);
  } else if (speed < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(en, -speed);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(en, 0);
  }
}

void setLeftMotor(int speed) {
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, speed, LEFT_INVERTED);
}

void setRightMotor(int speed) {
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, speed, RIGHT_INVERTED);
}

// leftSpeed/rightSpeed: -255..255
void driveRaw(int leftSpeed, int rightSpeed) {
  setLeftMotor(leftSpeed);
  setRightMotor(rightSpeed);
}

void stopMotors() {
  driveRaw(0, 0);
}

void moveForward() {
  driveRaw(SPEED_FORWARD_LEFT, SPEED_FORWARD_RIGHT);
}

void moveBackward() {
  driveRaw(-SPEED_BACKWARD_LEFT, -SPEED_BACKWARD_RIGHT);
}

void turnLeft() {
  driveRaw(-SPEED_TURN, SPEED_TURN);
}

void turnRight() {
  driveRaw(SPEED_TURN, -SPEED_TURN);
}

void handleCommand(char cmd) {
  currentCommand = cmd;
  lastCommandTime = millis();

  switch (cmd) {
    case 'F':
      moveForward();
      Serial.println("FORWARD");
      break;

    case 'B':
      moveBackward();
      Serial.println("BACKWARD");
      break;

    case 'L':
      turnLeft();
      Serial.println("LEFT");
      break;

    case 'R':
      turnRight();
      Serial.println("RIGHT");
      break;

    case 'S':
      stopMotors();
      Serial.println("STOP");
      break;

    default:
      break;
  }
}

void setup() {
  pinMode(LEFT_EN, OUTPUT);
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);

  pinMode(RIGHT_EN, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  stopMotors();

  Serial.begin(9600);
  bt.begin(9600);

  Serial.println("CarControlBluetooth started");
  lastCommandTime = millis();
}

void loop() {
  while (bt.available()) {
    char cmd = bt.read();
    handleCommand(cmd);   

  if (millis() - lastCommandTime > COMMAND_TIMEOUT) {
    if (currentCommand != 'S') {
      currentCommand = 'S';
      stopMotors();
    }
  }
} 