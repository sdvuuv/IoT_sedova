#include <EEPROM.h>

// ===================== ПИНЫ ДРАЙВЕРА L298N =====================
// Левый мотор
const uint8_t LEFT_EN  = 6;   // ШИМ-пин скорости (ENA)
const uint8_t LEFT_IN1 = 8;   // направление, вход 1
const uint8_t LEFT_IN2 = 9;   // направление, вход 2
// Правый мотор
const uint8_t RIGHT_EN  = 5;  // ШИМ-пин скорости (ENB)
const uint8_t RIGHT_IN1 = 11; // направление, вход 1
const uint8_t RIGHT_IN2 = 10; // направление, вход 2
// Пины 0/1 заняты аппаратным Serial -> к ним подключается Bluetooth-модуль

// ===================== КОМАНДЫ GAMEPAD =====================
// X B A Y  - кнопки;  < > ^ v - крестовина
#define CMD_X   'X'   // выбор стороны
#define CMD_B   'B'   // переключение направления вращения мотора
#define CMD_A   'A'   // зафиксировать текущую комбинацию как "вперёд"
#define CMD_Y   'Y'   // переключить комбинацию HIGH/LOW на следующую
#define CMD_LT  '<'   // уменьшить значение
#define CMD_RT  '>'   // увеличить значение
#define CMD_UP  '^'   // следующий редактируемый параметр
#define CMD_DN  'v'   // предыдущий редактируемый параметр

// дополнительные служебные команды (не из gamepad, для отладки/сброса)
#define CMD_TEST_FWD   'f'   // проверочный заезд: вперёд
#define CMD_TEST_BACK  'b'   // проверочный заезд: назад
#define CMD_TEST_L90   'l'   // проверочный поворот налево на текущий угол
#define CMD_TEST_R90   'r'   // проверочный поворот направо на текущий угол
#define CMD_STOP       'S'   // немедленный стоп
#define CMD_SAVE       's'   // записать калибровку в EEPROM
#define CMD_RESET      '0'   // сброс калибровки к значениям по умолчанию
#define CMD_DUMP       '?'   // вывести текущее состояние калибровки

// ===================== СТОРОНЫ =====================
enum Side { SIDE_LEFT = 0, SIDE_RIGHT = 1 };

// ===================== УГЛЫ ПОВОРОТА =====================
// время поворота калибруется отдельно для 90/180/270/360 градусов,
// потому что из-за инерции и проскальзывания 360 != 4 * 90
const int ANGLE_COUNT = 4;
const int ANGLE_VALUES[ANGLE_COUNT] = { 90, 180, 270, 360 };

// ===================== РЕДАКТИРУЕМЫЕ ПАРАМЕТРЫ =====================
// крестовиной < > меняем значение, ^ v выбираем какой параметр редактируем
enum Param {
  P_SPEED_LEFT = 0,    // базовая скорость левого мотора
  P_SPEED_RIGHT,       // базовая скорость правого мотора
  P_TURN_90,           // время поворота на 90 градусов, мс
  P_TURN_180,          // время поворота на 180 градусов, мс
  P_TURN_270,          // время поворота на 270 градусов, мс
  P_TURN_360,          // время поворота на 360 градусов, мс
  PARAM_COUNT
};

// ===================== СТРУКТУРА КАЛИБРОВКИ =====================
// то, что подбирает пользователь и что хранится в EEPROM
struct Calibration {
  uint16_t magic;          // метка валидности записи в EEPROM

  // признак инверсии каждого мотора: меняет местами IN1/IN2,
  // т.е. учитывает способ подключения проводов мотора
  bool     invertLeft;     // инвертирован ли левый мотор
  bool     invertRight;    // инвертирован ли правый мотор

  // балансировка скоростей (колёса крутятся немного по-разному)
  uint8_t  speedLeft;      // скорость левого мотора  0..255
  uint8_t  speedRight;     // скорость правого мотора 0..255

  // время поворота на месте для каждого из углов, мс
  uint16_t turnMs[ANGLE_COUNT];
};

const uint16_t CALIB_MAGIC = 0xCA17;   // "CAlibration v1"
const int      EEPROM_ADDR = 0;

Calibration calib;

// ===================== ТЕКУЩЕЕ СОСТОЯНИЕ КАЛИБРОВКИ =====================
Side  currentSide   = SIDE_LEFT;   // какая сторона сейчас настраивается (команда X)
uint8_t comboIndex  = 0;           // индекс перебираемой комбинации HIGH/LOW (команда Y)
Param currentParam  = P_SPEED_LEFT;// какой параметр редактируется крестовиной

// четыре комбинации HIGH/LOW для пары IN1/IN2 одного мотора.
// именно их перебирает кнопка Y, а кнопка A фиксирует выбранную как "вперёд"
struct Combo { uint8_t in1; uint8_t in2; };
const Combo COMBOS[4] = {
  { LOW,  LOW  },   // оба LOW  - мотор стоит (выбег)
  { HIGH, LOW  },   // вращение в одну сторону
  { LOW,  HIGH },   // вращение в другую сторону
  { HIGH, HIGH }    // оба HIGH - мотор стоит (торможение)
};

// ===================== ЗНАЧЕНИЯ ПО УМОЛЧАНИЮ =====================
void loadDefaults() {
  calib.magic       = CALIB_MAGIC;
  calib.invertLeft  = false;
  calib.invertRight = false;
  calib.speedLeft   = 150;
  calib.speedRight  = 150;
  calib.turnMs[0]   = 600;   // 90
  calib.turnMs[1]   = 1200;  // 180
  calib.turnMs[2]   = 1800;  // 270
  calib.turnMs[3]   = 2400;  // 360
}

// ===================== РАБОТА С EEPROM =====================
void saveCalibration() {
  EEPROM.put(EEPROM_ADDR, calib);
  Serial.println(F("SAVED to EEPROM"));
}

void loadCalibration() {
  Calibration tmp;
  EEPROM.get(EEPROM_ADDR, tmp);
  if (tmp.magic == CALIB_MAGIC) {
    calib = tmp;
    Serial.println(F("LOADED from EEPROM"));
  } else {
    loadDefaults();
    Serial.println(F("EEPROM empty -> defaults"));
  }
}

// ===================== НИЗКОУРОВНЕВОЕ УПРАВЛЕНИЕ МОТОРАМИ (L298N) =====================
// speed: -255..255. Знак задаёт направление, модуль - скорость через ШИМ.
// inverted переставляет IN1/IN2 местами - так учитывается способ подключения проводов.
void setMotor(uint8_t en, uint8_t in1, uint8_t in2, int speed, bool inverted) {
  if (inverted) speed = -speed;
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
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, speed, calib.invertLeft);
}

void setRightMotor(int speed) {
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, speed, calib.invertRight);
}

// одновременная установка обоих моторов, скорости -255..255
void driveRaw(int leftSpeed, int rightSpeed) {
  setLeftMotor(leftSpeed);
  setRightMotor(rightSpeed);
}

void stop_motors() {
  driveRaw(0, 0);
}

// "вперёд" с учётом инверсии и балансировки скоростей
void driveForward() {
  driveRaw(calib.speedLeft, calib.speedRight);
}

// "назад" - оба мотора с отрицательной скоростью
void driveBackward() {
  driveRaw(-(int)calib.speedLeft, -(int)calib.speedRight);
}

// разворот на месте: левый мотор назад, правый вперёд
void rotateLeftRaw() {
  driveRaw(-(int)calib.speedLeft, calib.speedRight);
}

// разворот на месте: левый мотор вперёд, правый назад
void rotateRightRaw() {
  driveRaw(calib.speedLeft, -(int)calib.speedRight);
}

// поворот на заданный угол по откалиброванному времени
void turnByAngle(int angleIndex, bool toLeft) {
  if (angleIndex < 0 || angleIndex >= ANGLE_COUNT) return;
  if (toLeft) rotateLeftRaw();
  else        rotateRightRaw();
  delay(calib.turnMs[angleIndex]);
  stop_motors();
}

// ===================== ПРИМЕНЕНИЕ ВЫБРАННОЙ КОМБИНАЦИИ =====================
// кнопка A: проверяем, как перебираемая комбинация COMBOS[comboIndex]
// крутит мотор выбранной стороны, и при необходимости выставляем инверсию
// так, чтобы эта комбинация означала движение "вперёд".
void applyComboAsForward() {
  Combo c = COMBOS[comboIndex];

  // "вперёд" в схеме setMotor() - это пара IN1=HIGH, IN2=LOW.
  // если перебираемая комбинация совпадает с этим - инверсия не нужна,
  // если это зеркальная комбинация - включаем инверсию мотора.
  bool needInvert;
  if (c.in1 == HIGH && c.in2 == LOW) {
    needInvert = false;
  } else if (c.in1 == LOW && c.in2 == HIGH) {
    needInvert = true;
  } else {
    // комбинации (LOW,LOW) и (HIGH,HIGH) мотор не вращают - пропускаем
    Serial.println(F("Combo gives no motion - pick HIGH/LOW or LOW/HIGH"));
    return;
  }

  if (currentSide == SIDE_LEFT) {
    calib.invertLeft = needInvert;
    Serial.print(F("LEFT forward fixed, invert="));
    Serial.println(needInvert ? F("YES") : F("NO"));
  } else {
    calib.invertRight = needInvert;
    Serial.print(F("RIGHT forward fixed, invert="));
    Serial.println(needInvert ? F("YES") : F("NO"));
  }
}

// ===================== ВЫВОД СОСТОЯНИЯ =====================
void dumpState() {
  Serial.println(F("---- CALIBRATION ----"));
  Serial.print(F("Side: "));
  Serial.println(currentSide == SIDE_LEFT ? F("LEFT") : F("RIGHT"));

  Serial.print(F("Combo idx (Y): "));
  Serial.print(comboIndex);
  Serial.print(F("  -> IN1="));
  Serial.print(COMBOS[comboIndex].in1 == HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F(" IN2="));
  Serial.println(COMBOS[comboIndex].in2 == HIGH ? F("HIGH") : F("LOW"));

  Serial.print(F("Invert L/R: "));
  Serial.print(calib.invertLeft  ? F("YES") : F("NO"));
  Serial.print(F(" / "));
  Serial.println(calib.invertRight ? F("YES") : F("NO"));

  Serial.print(F("Speed L/R: "));
  Serial.print(calib.speedLeft);
  Serial.print(F(" / "));
  Serial.println(calib.speedRight);

  for (int i = 0; i < ANGLE_COUNT; i++) {
    Serial.print(F("Turn "));
    Serial.print(ANGLE_VALUES[i]);
    Serial.print(F(" deg: "));
    Serial.print(calib.turnMs[i]);
    Serial.println(F(" ms"));
  }

  Serial.print(F("Editing param: "));
  Serial.println((int)currentParam);
  Serial.println(F("---------------------"));
}

// ===================== ИЗМЕНЕНИЕ ПАРАМЕТРА КРЕСТОВИНОЙ =====================
// delta = +1 (кнопка >) или -1 (кнопка <)
void changeParam(int delta) {
  switch (currentParam) {
    case P_SPEED_LEFT:
      calib.speedLeft  = constrain((int)calib.speedLeft  + delta * 5, 0, 255);
      break;
    case P_SPEED_RIGHT:
      calib.speedRight = constrain((int)calib.speedRight + delta * 5, 0, 255);
      break;
    case P_TURN_90:
      calib.turnMs[0]  = constrain((int)calib.turnMs[0] + delta * 25, 0, 10000);
      break;
    case P_TURN_180:
      calib.turnMs[1]  = constrain((int)calib.turnMs[1] + delta * 25, 0, 10000);
      break;
    case P_TURN_270:
      calib.turnMs[2]  = constrain((int)calib.turnMs[2] + delta * 25, 0, 10000);
      break;
    case P_TURN_360:
      calib.turnMs[3]  = constrain((int)calib.turnMs[3] + delta * 25, 0, 10000);
      break;
    default:
      break;
  }
}

// ===================== ОБРАБОТКА КОМАНДЫ =====================
void handleCommand(char cmd) {
  switch (cmd) {

    // -------- X: выбор настраиваемой стороны --------
    case CMD_X:
      currentSide = (currentSide == SIDE_LEFT) ? SIDE_RIGHT : SIDE_LEFT;
      Serial.print(F("Side -> "));
      Serial.println(currentSide == SIDE_LEFT ? F("LEFT") : F("RIGHT"));
      break;

    // -------- B: инвертировать направление мотора выбранной стороны --------
    case CMD_B:
      if (currentSide == SIDE_LEFT)  calib.invertLeft  = !calib.invertLeft;
      else                            calib.invertRight = !calib.invertRight;
      Serial.println(F("Motor direction inverted"));
      break;

    // -------- Y: перебор комбинаций HIGH/LOW --------
    case CMD_Y:
      comboIndex = (comboIndex + 1) % 4;
      Serial.print(F("Combo -> "));
      Serial.println(comboIndex);
      break;

    // -------- A: зафиксировать перебираемую комбинацию как "вперёд" --------
    case CMD_A:
      applyComboAsForward();
      break;

    // -------- крестовина: редактирование числовых параметров --------
    case CMD_RT:
      changeParam(+1);
      Serial.println(F("param +"));
      break;
    case CMD_LT:
      changeParam(-1);
      Serial.println(F("param -"));
      break;
    case CMD_UP:
      currentParam = (Param)(((int)currentParam + 1) % PARAM_COUNT);
      Serial.print(F("Param -> "));
      Serial.println((int)currentParam);
      break;
    case CMD_DN:
      currentParam = (Param)(((int)currentParam + PARAM_COUNT - 1) % PARAM_COUNT);
      Serial.print(F("Param -> "));
      Serial.println((int)currentParam);
      break;

    // -------- проверочные заезды --------
    case CMD_TEST_FWD:
      Serial.println(F("TEST forward"));
      driveForward();
      delay(1000);
      stop_motors();
      break;
    case CMD_TEST_BACK:
      Serial.println(F("TEST backward"));
      driveBackward();
      delay(1000);
      stop_motors();
      break;
    case CMD_TEST_L90:
      Serial.println(F("TEST turn left 90"));
      turnByAngle(0, true);
      break;
    case CMD_TEST_R90:
      Serial.println(F("TEST turn right 90"));
      turnByAngle(0, false);
      break;

    // -------- служебные команды --------
    case CMD_STOP:
      stop_motors();
      Serial.println(F("STOP"));
      break;
    case CMD_SAVE:
      saveCalibration();
      break;
    case CMD_RESET:
      loadDefaults();
      Serial.println(F("Defaults restored"));
      break;
    case CMD_DUMP:
      dumpState();
      break;

    default:
      // незнакомый символ игнорируем
      break;
  }
}

// ===================== SETUP =====================
void setup() {
  pinMode(LEFT_EN,  OUTPUT);
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_EN,  OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  stop_motors();

  // аппаратный Serial - это и есть канал с Bluetooth-модулем (UART по воздуху)
  Serial.begin(9600);

  loadCalibration();    // достаём сохранённую калибровку (или дефолты)

  Serial.println(F("Calibration firmware ready"));
  Serial.println(F("X side | B invert | Y combo | A fix fwd"));
  Serial.println(F("< > value | ^ v param | f/b/l/r test | s save | 0 reset | ? dump"));
  dumpState();
}

// ===================== LOOP =====================
void loop() {
  // команды калибровки приходят с аппаратного Serial (Bluetooth-модуль)
  while (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '\r' || cmd == '\n' || cmd == ' ') continue; // пропускаем разделители
    handleCommand(cmd);
  }
}