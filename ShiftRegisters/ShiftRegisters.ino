#include <avr/io.h>
#include <avr/interrupt.h>
const bool IS_COMMON_ANODE = false; 


volatile uint8_t counter = 0; // Счетчик от 00 до 99
volatile bool updateDisplay = true; // Флаг необходимости обновления

const uint8_t digitMap[10] = {
  0b10111011, 
  0b00001010, 
  0b01110011, 
  0b01011011, 
  0b11001010,  
  0b11011001, 
  0b11111001, 
  0b00001011, 
  0b11111011, 
  0b11011011  
};


//  для прямого управления портами
#define DATA_PIN  7
#define LATCH_PIN 5
#define CLOCK_PIN 3

#define DATA_HIGH  (PORTD |= (1 << DATA_PIN))
#define DATA_LOW   (PORTD &= ~(1 << DATA_PIN))
#define LATCH_HIGH (PORTD |= (1 << LATCH_PIN))
#define LATCH_LOW  (PORTD &= ~(1 << LATCH_PIN))
#define CLOCK_HIGH (PORTD |= (1 << CLOCK_PIN))
#define CLOCK_LOW  (PORTD &= ~(1 << CLOCK_PIN))

void setup() {
  // Настройка пинов на выход через регистр DDRD
  DDRD |= (1 << DATA_PIN) | (1 << LATCH_PIN) | (1 << CLOCK_PIN);
  
  // Инициализация портов в LOW
  PORTD &= ~((1 << DATA_PIN) | (1 << LATCH_PIN) | (1 << CLOCK_PIN));

  Serial.begin(9600);
  Serial.println(F("System Start. Enter 0-99 to set counter."));

  cli(); // Отключаем прерывания на время настройки
  
  TCCR1A = 0; // Очистка регистров управления таймером
  TCCR1B = 0;
  TCNT1  = 0; // Сброс счетчика

  // Частота = 16 МГц
  // Предделитель = 1024
  // (16000000 / 1024) - 1 = 15624
  OCR1A = 15624;

  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);

  sei(); 
  
  // Принудительно показываем 00
  forceUpdate(); 
}

void loop() {
  if (Serial.available() > 0) {
    int val = Serial.parseInt();
    
    // Очистка буфера от лишних символов
    while (Serial.available() > 0) Serial.read();

    if (val >= 0 && val <= 99) {
      // обновление переменной counter
      cli(); 
      counter = (uint8_t)val;
      sei();
      
      Serial.print(F("Counter set to: "));
      Serial.println(val);
      forceUpdate();
    } else {
      Serial.println(F("Error: Only 0-99 allowed"));
    }
  }
}

// для программного сдвига байта
inline void shiftOutByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    if (data & 0x80) {
      DATA_HIGH;
    } else {
      DATA_LOW;
    }
    
    // Импульс тактирования
    CLOCK_HIGH;
    CLOCK_LOW;
    
    data <<= 1; // Сдвиг влево
  }
}

// Функция обновления регистров
void updateShiftRegisters(uint8_t num) {
  uint8_t tens = num / 10;
  uint8_t ones = num % 10;
  
  uint8_t segTens = digitMap[tens];
  uint8_t segOnes = digitMap[ones];

  if (IS_COMMON_ANODE) {
    segTens = ~segTens;
    segOnes = ~segOnes;
  }
  
  LATCH_LOW;  
  
  shiftOutByte(segTens); 
  shiftOutByte(segOnes);
    
  LATCH_HIGH;
}

void forceUpdate() {
  cli();
  updateShiftRegisters(counter);
  sei();
}

ISR(TIMER1_COMPA_vect) {
  counter++;
  if (counter > 99) {
    counter = 0;
  }
  updateShiftRegisters(counter);
}