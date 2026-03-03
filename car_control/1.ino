#define SPEED_LEFT 5
#define SPEED_RIGHT 6

#define DIR_LEFT 4
#define DIR_RIGHT 7

#define FOWARD_LEFT LOW
#define BACKWARD_LEFT HIGH

#define FOWARD_RIGHT LOW 
#define BACKWARD_RIGHT HIGH

void move(bool left_dir, int left_speed, bool right_dir, int right_speed) {
  digitalWrite(DIR_LEFT, left_dir);
  digitalWrite(DIR_RIGHT, right_dir);
  analogWrite(SPEED_LEFT, left_speed);
  analogWrite(SPEED_RIGHT, right_speed);
}

void foward(int speed) {
  move(FOWARD_LEFT, speed, FOWARD_RIGHT, speed);
}

void backward(int speed) {
  move(BACKWARD_LEFT, speed, BACKWARD_RIGHT, speed);
}
void turn_left(int speed, int steepness) {
  int inner_speed = speed - steepness; 
  if (inner_speed < 0) inner_speed = 0;
  move(FOWARD_LEFT, inner_speed, FOWARD_RIGHT, speed);
}

void turn_right(int speed, int steepness) {
  int inner_speed = speed - steepness; 
  if (inner_speed < 0) inner_speed = 0;
  move(FOWARD_LEFT, speed, FOWARD_RIGHT, inner_speed);
}



void rotate_left(int speed) {
  move(BACKWARD_LEFT, speed, FOWARD_RIGHT, speed);
}

void rotate_right(int speed) {
  move(FOWARD_LEFT, speed, BACKWARD_RIGHT, speed);
}

void stop_motors() {
  move(FOWARD_LEFT, 0, FOWARD_RIGHT, 0);
}

void setup() {
  pinMode(SPEED_LEFT, OUTPUT);
  pinMode(SPEED_RIGHT, OUTPUT);
  pinMode(DIR_LEFT, OUTPUT);
  pinMode(DIR_RIGHT, OUTPUT);

 
  foward(150);
  delay(2000);
  backward(150);
  delay(2000);
  turn_left(255, 255); 
  delay(2000);
  turn_right(255, 255);
  delay(2000);
  rotate_left(150);
  delay(1000);
  rotate_right(150);
  delay(1000);
  stop_motors();
}

void loop() {
}