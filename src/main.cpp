#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#define CLOCK_FREQUENCY 80000000
#define CYCLES_PER_SECOND 200
#define PRESCALER CLOCK_FREQUENCY / (CYCLES_PER_SECOND * 512)

#define POSITIVE_PHASE_PIN 3
#define NEGATIVE_PHASE_PIN 4

#define POSITIVE_PHASE 0
#define NEGATIVE_PHASE 1

// ATtiny85 Pin map
//                        +-\/-+
// Reset/Ain0 (D 5) PB5  1|o   |8  Vcc
//  Xtal/Ain3 (D 3) PB3  2|    |7  PB2 (D 2) Ain1
//  Xtal/Ain2 (D 4) PB4  3|    |6  PB1 (D 1) pwm1
//                  GND  4|    |5  PB0 (D 0) pwm0 <-- connect to gate of FET
//                        +----+

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

volatile uint8_t led_state = 0;

volatile int ticks = 0;
volatile int spade = 0;

volatile static int positive_level = 8;
volatile static int negative_level = 127;
volatile static int counter;

static int state = POSITIVE_PHASE;
static int cycles = 0;

static int sineTable[] = {
  0, 2, 4, 7, 9, 11, 13, 16, 18, 20,
  22, 24, 27, 29, 31, 33, 35, 37, 40, 42,
  44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 
  64, 66, 68, 70, 72, 73, 75, 77, 79, 81,
  82, 84, 86, 87, 89, 91, 92, 94, 95, 97,
  98, 99, 101, 102, 104, 105, 106, 107, 109, 110,
  111, 112, 113, 114, 115, 116, 117, 118, 119, 119, 
  120, 121, 122, 122, 123, 124, 124, 125, 125, 126, 
  126, 126, 127, 127, 127, 128, 128, 128, 128, 128
};

ISR(TIMER0_COMPA_vect) {
  sbi(MCUCR, SE);       // Disable sleep mode

  ticks++;
  spade++;

  TCNT0 = 0;

  if (ticks == 128) {
    ticks = 0;
    cycles++;

    if (state == POSITIVE_PHASE) {
      state = NEGATIVE_PHASE;
      digitalWrite(POSITIVE_PHASE_PIN, 0);
      if (negative_level == 0) {
        digitalWrite(NEGATIVE_PHASE_PIN, 0);
      }
      else {
        digitalWrite(NEGATIVE_PHASE_PIN, 1);
        counter = negative_level;
      }
    }
    else {
      state = POSITIVE_PHASE;
      digitalWrite(NEGATIVE_PHASE_PIN, 0);
      if (positive_level == 0) {
        digitalWrite(POSITIVE_PHASE_PIN, 0);
      }
      else {
        digitalWrite(POSITIVE_PHASE_PIN, 1);
        counter = positive_level;
      }
    }
  }

  counter--;
  if (counter <= 0) {
    digitalWrite(POSITIVE_PHASE_PIN, 0);
    digitalWrite(NEGATIVE_PHASE_PIN, 0);    
  }
}

inline void idle() {
  cbi(MCUCR, SM0);
  cbi(MCUCR, SM1);
  sbi(MCUCR, SE);  
  __asm__ __volatile__ ( "sleep" "\n\t" :: );
}

void setup() {
  pinMode(POSITIVE_PHASE_PIN, OUTPUT);
  pinMode(NEGATIVE_PHASE_PIN, OUTPUT);
  
  pinMode(LED_BUILTIN, OUTPUT);

  // Disable interrupts while we set things up
  cli();

  // Disable sleeping, set sleep mode to IDLE
  cbi(MCUCR, SE); 
  cbi(MCUCR, SM0);
  cbi(MCUCR, SM1);

  /*
   * Set timer 0:
   *  - divide 8MHz system clock by 64
   *  - interrupt on timer compare OCR0A
   */ 

  // Make sure Timer1 is in synchronous mode, i.e. clocked by the system clock.
  cbi(PLLCSR, PCKE);
  
  // Normal mode 
  TCCR0A = 0;

  // Set prescaler to 64
  TCCR0B = 0;
  sbi(TCCR0B, CS00);
  sbi(TCCR0B, CS01);

  // Enable timer 1 compare interrupt A
  TIMSK = 0;
  sbi(TIMSK, OCIE0A); 

  // Set up the timer compare registers
  OCR0A = 9;
  
  // Reset counter
  TCNT0 = 0;
  
  // Enable interrupts
  sei();
}

void idleFor(int mS) {
  register int spades = mS * 25;

  spade = 0;
  while (spade < spades)
    idle(); 
}

void setBrightness(int channel, int value) {

}

void loop() {
  // Ramp both channels up..
  for (int angle=0; angle<90; angle++) {
    negative_level = sineTable[angle];
    positive_level = sineTable[angle];
    idleFor(5);
  }
  
  while (1) {   
    // dim -ve channel
    for (int angle=89; angle!=0; angle--) {
      negative_level = sineTable[angle];
      idleFor(6);
    }

    // bring -ve channel back up
    for (int angle=0; angle<90; angle++) {
      negative_level = sineTable[angle];
      idleFor(6);
    }

    idleFor(15);
    
    // dim +ve channel
    for (int angle=89; angle!=0; angle--) {
      positive_level = sineTable[angle];
      idleFor(6);
    }

    // bring +ve channel back up
    for (int angle=0; angle<90; angle++) {
      positive_level = sineTable[angle];
      idleFor(6);
    }

    idleFor(15);
  }
}
