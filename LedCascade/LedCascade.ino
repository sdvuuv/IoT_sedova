
const int BASE_PERIOD_INTERVALS = 15;
const int NUM_LEDS = 5;

// массив для хранения значений счетчиков для каждого светодиода
volatile unsigned int ledCounters[NUM_LEDS] = {0};

void setup() {
  cli();
  DDRB |= 0b00011111;

  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS12);
  OCR1A = 624;
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

ISR(TIMER1_COMPA_vect) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledCounters[i]++;
    unsigned int targetPeriod = (i + 1) * BASE_PERIOD_INTERVALS;

    // проверяем достиг ли счетчик вычисленного периода
    if (ledCounters[i] >= targetPeriod) {
      // инвертируем 
      PORTB ^= (1 << i);

      ledCounters[i] = 0;
    }
  }
}

void loop() {
}
