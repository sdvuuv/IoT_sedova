#define SPEED_LEFT 5
#define SPEED_RIGHT 6

#define DIR_LEFT 4
#define DIR_RIGHT 7

#define FOWARD_LEFT LOW
#define BACKWARD_LEFT HIGH

#define FOWARD_RIGHT HIGH
#define BACKWARD_RIGHT LOW
void move(bool left_dir, int left_speed, bool right_dir, int right_speed){
  analogWrite(DIR_LEFT, left_dir);
  analogWrite(DIR_RIGHT, right_dir);
  analogWrite(SPEED_LEFT, left_speed);
  analogWrite(SPEED_RIGHT, right_speed);
  
  
}
void setup() {
  pinMode(SPEED_LEFT, OUTPUT);
  pinMode(SPEED_RIGHT, OUTPUT);
  pinMode(DIR_LEFT, OUTPUT);
  pinMode(DIR_RIGHT, OUTPUT);
  move(FOWARD_LEFT, 255, FOWARD_RIGHT, 0);
  delay(2000);
  move(FOWARD_LEFT, 0, FOWARD_RIGHT, 255);
  
  delay(2000);
  move(FOWARD_LEFT, 0, FOWARD_RIGHT, 0);

  

  

}

void loop() {
  

}