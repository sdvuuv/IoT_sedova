
### Задача: 
Реализовать асинхронную последовательную передачу данных (UART) — передачу (TX) и приём (RX) — используя только GPIO, аппаратный таймер и прерывания, без использования аппаратного модуля USART.



### Требования:
- Процессы RX и TX работают в фоновом режиме, управляются таймерами.Пользовательский код только записывает данные в буфер или читает их.
- Используются кольцевые буферы для TX и RX. Нужно обрабатывать переполнение буфера (потерю данных).
- RX-буфер хранит данные, пока пользователь явно не считает их.
- Таймер должен использоваться в режиме прерываний для формирования точного временного интервала (без busy-wait циклов).
- RX-буфер контролируется пользовательским кодом (uart_read(), uart_read_string()), но процесс приёма данных имеет высший приоритет, так как он происходит в реальном времени. Нельзя допускать потери входящих данных.
- Для обнаружения стартового бита рекомендуется использовать аппаратное внешнее прерывание. После этого таймер управляет выборкой и синхронизацией битов. Допустимо использовать attachInterrupt, но предпочтительно работать напрямую с регистрами.
- Используется Timer1 для приёма и передачи.
- Нельзя использовать delay() или millis().
- Настройка пинов — только через DDRx, PINx, PORTx (без pinMode() и аналогичных функций).
- Использовать volatile глобальные переменные для взаимодействия между ISR и основным кодом.
- Обработчики прерываний (ISR) должны быть короткими и эффективными.\


Tinkercad: [https://www.tinkercad.com/things/3YSGYSi5E6c/editel?returnTo=%2Fdashboard%2Fdesigns%2Fcircuits&sharecode=ZXkgO58JOg-rwHCj4b8KEmdw6zBxzT8THlMu9R6v9uc](https://www.tinkercad.com/things/3YSGYSi5E6c/editel?returnTo=%2Fdashboard%2Fdesigns%2Fcircuits&sharecode=HIHzZxc5UMoz6hBBSGyg_x4hKGWqj-S6ZZ888ZBllcA)

### Рассмотрим код и логику его работы.

##### Глобальные константы и переменные
```c++

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


```
- BUF_SIZE - размер кольцевого буфера (64 байта)
- BUF_MASK - битовая маска (63 или 0b00111111) позволяет заменить операцию деления с остатком % на побитовую операцию & при вычислении индексов буфера
- bit_period_ticks - колво тиков таймера, соответствующее длительности одного бита
- tx_buf, rx_buf -  для хранения очереди отправки и принятых данных
- head, tail - индексы куда писать и откуда читать у кольцевого буфера

##### API Функции
##### 1. Настройка скорости
```c++

void uart_set_baudrate(int rate) {
  bit_period_ticks = (F_CPU / 8) / rate;
}

```
- вычисляет значение для таймера. F_CPU

##### 2. Проверка доступных данных

```c++

uint8_t uart_available() {
  return (rx_head - rx_tail) & BUF_MASK;
}

```

- возвращает кол-во байт в буфере приема
- спользует побитовое & для обработки переполнения счетчиков head и tail

##### 3. Чтение байта

```c++

char uart_read() {
  if (rx_head == rx_tail) return 0;
  char data = rx_buf[rx_tail];
  rx_tail = (rx_tail + 1) & BUF_MASK;
  return data;
}

```

- извлекает старейший байт из rx_buf по индексу tail
- увеличивает tail с учетом кольцевой структуры

##### 4. Отправка байта
```c++

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

```
- Добавляет байт в буфер отправки tx_buf

- if (!tx_active) если передача в данный момент не идет инициирует её
- - Забирает байт из буфера
- - Устанавливает линию TX в 0
- - Включает прерывание таймера


##### Вспомогательные функции
```c++

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


```

- uart_send_string - побайтово отправляет строку вызывая uart_send
- uart_read_string - считывает все накопленные байты из буфера в переданный массив


### setup()
```c++

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

```

- настраивает ввод вывод через регистры
- иницилизирует Timer1 в нормальном режиме с делителем 8
- внешнее прерывание INT0, которое будет отслеживать начало передачи


### loop()
```c++


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

```

- читает данные функцией uart_read_string и отправляет их обратно


### Обработчики прерываний
##### ISR(INT0_vect)
```c++


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

```
- cрабатывает при переходе линии RX из 1 в 0
- отключает само себя (EIMSK) чтобы не реагировать на изменения данных 
- запускает таймер канала B (OCIE1B), который начнет считывать биты через 1.5 периода

##### ISR(TIMER1_COMPB_vect)
```c++

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


```
- вызывается таймером  в середине каждого бита
- считывает состояние пина и формирует байт
- после 8 бита сохраняет данные и снова включает внешнее прерывание INT0 для ожидания следующего байта

##### ISR(TIMER1_COMPA_vect)
```c++


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
```
- работает независимо от RX, используя канал A таймера
- выставляет уровень на пине TX в соответствии с текущим битом
- использует сдвиг вправо для подготовки следующего бита
- автоматически берет следующий байт из буфера, если он не пуст, или отключает таймер если данных нет