// пины
#define PIN_TX 3 // PD3
#define PIN_RX 2 // PD2


//  хранение длительности бита
volatile uint16_t bit_period_ticks = 208; 

// буферы
#define BUF_SIZE 64
#define BUF_MASK (BUF_SIZE - 1)

volatile char tx_buf[BUF_SIZE];
volatile uint8_t tx_head = 0;
volatile uint8_t tx_tail = 0;

volatile char rx_buf[BUF_SIZE];
volatile uint8_t rx_head = 0;
volatile uint8_t rx_tail = 0;

// Состояние TX
volatile bool tx_active = false;
volatile uint8_t tx_byte;
volatile uint8_t tx_bit_pos;

// Состояние RX
volatile bool rx_sampling = false;
volatile uint8_t rx_byte;
volatile uint8_t rx_bit_pos;


void uart_set_baudrate(int rate) {
  bit_period_ticks = (F_CPU / 8) / rate;
}

uint8_t uart_available() {
  return (rx_head - rx_tail) & BUF_MASK;
}

char uart_read() {
  if (rx_head == rx_tail) return 0;
  char data = rx_buf[rx_tail];
  rx_tail = (rx_tail + 1) & BUF_MASK;
  return data;
}

void uart_send(char b) {
  uint8_t next = (tx_head + 1) & BUF_MASK;
  // если буфер полон - ждем
  while (next == tx_tail); 

  tx_buf[tx_head] = b;
  tx_head = next;

  noInterrupts();
  if (!tx_active) {
    tx_active = true;
    tx_byte = tx_buf[tx_tail];
    tx_tail = (tx_tail + 1) & BUF_MASK;
    tx_bit_pos = 0;

    PORTD &= ~_BV(PIN_TX);

    // прерывание TX
    OCR1A = TCNT1 + bit_period_ticks;
    TIFR1 |= _BV(OCF1A);   
    TIMSK1 |= _BV(OCIE1A); 
  }
  interrupts();
}

void uart_send_string(const char *msg) {
  while (*msg) {
    uart_send(*msg++);
  }
}

bool uart_read_string(char *rx_data) {
  bool any_data = false;
  int i = 0;
  while (uart_available()) {
    rx_data[i++] = uart_read();
    any_data = true;
  }
  rx_data[i] = '\0'; 
  return any_data;
}



void setup() {
  // пины
  DDRD |= _BV(PIN_TX);  // TX выход
  PORTD |= _BV(PIN_TX); // TX высокий 
  DDRD &= ~_BV(PIN_RX); // RX вход
  PORTD |= _BV(PIN_RX); // RX подтяжка

  Serial.begin(9600); 

  noInterrupts();
  
  TCCR1A = 0;
  TCCR1B = _BV(CS11); 
  TCNT1 = 0;

  EICRA = _BV(ISC01); 
  EIMSK |= _BV(INT0); 

  interrupts();
  
  uart_set_baudrate(9600);
  Serial.println("System Ready");
}

void loop() {
  char buf[65];
  
  if (uart_read_string(buf)) {
    Serial.print(buf); 
  }

  if (Serial.available()) {
    char c = Serial.read();
    uart_send(c);
  }
}


ISR(INT0_vect) {
  if (!rx_sampling) {
    rx_sampling = true;
    rx_bit_pos = 0;
    rx_byte = 0;

    EIMSK &= ~_BV(INT0);

    OCR1B = TCNT1 + (bit_period_ticks + (bit_period_ticks >> 1));
    
    TIFR1 |= _BV(OCF1B);
    TIMSK1 |= _BV(OCIE1B);
  }
}

ISR(TIMER1_COMPB_vect) {
  OCR1B += bit_period_ticks; 

  if (rx_bit_pos < 8) {
    // чтение бита
    if (PIND & _BV(PIN_RX)) {
      rx_byte |= (1 << rx_bit_pos);
    }
    rx_bit_pos++;
  } else {

    uint8_t next = (rx_head + 1) & BUF_MASK;
    if (next != rx_tail) {
      rx_buf[rx_head] = rx_byte;
      rx_head = next;
    }
    
    rx_sampling = false;
    TIMSK1 &= ~_BV(OCIE1B); 
    EIFR |= _BV(INTF0);
    EIMSK |= _BV(INT0);
  }
}

ISR(TIMER1_COMPA_vect) {
  if (tx_bit_pos < 8) {
    if (tx_byte & 1) PORTD |= _BV(PIN_TX);
    else             PORTD &= ~_BV(PIN_TX);
    
    tx_byte >>= 1;
    tx_bit_pos++;
    OCR1A += bit_period_ticks;
  } else if (tx_bit_pos == 8) {
    PORTD |= _BV(PIN_TX);
    tx_bit_pos++;
    OCR1A += bit_period_ticks;
  } else {
    // проверяем буфер
    if (tx_head != tx_tail) {
      tx_byte = tx_buf[tx_tail];
      tx_tail = (tx_tail + 1) & BUF_MASK;
      tx_bit_pos = 0;
      
      PORTD &= ~_BV(PIN_TX);
      OCR1A += bit_period_ticks;
    } else {
      tx_active = false;
      TIMSK1 &= ~_BV(OCIE1A);
    }
  }
}