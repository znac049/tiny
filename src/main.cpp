#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Effect.h"

#define CLOCK_FREQUENCY 80000000

#define PHI_1 0
#define PHI_2 1

#define CHANNEL1_PIN 4
#define CHANNEL2_PIN 0
#define CHANNEL3_PIN 3
#define CHANNEL4_PIN 2

#define DEBUG_PIN 5

// ATtiny85 Pin map
//                        +-\/-+
// Reset/Ain0 (D 5) PB5  1|o   |8  Vcc
//  Xtal/Ain3 (D 3) PB3  2|    |7  PB2 (D 2) Ain1
//  Xtal/Ain2 (D 4) PB4  3|    |6  PB1 (D 1) pwm1
//                  GND  4|    |5  PB0 (D 0) pwm0 
//                        +----+

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

int pwmTicks = 0;
int phase = PHI_1;

volatile int systemTicks = 0;

volatile uint8_t level1 = 0;
volatile uint8_t level2 = 0;
volatile uint8_t level3 = 0;
volatile uint8_t level4 = 0;
//volatile int counter;

Effect *fx[4];

ISR(TIMER0_COMPA_vect) {
  cbi(MCUCR, SE);       // Disable sleep mode

  pwmTicks++;
  systemTicks++;

  digitalWrite(DEBUG_PIN, pwmTicks & 1);

  // Reset counter
  TCNT0 = 0;

  if (pwmTicks >= 128) {
    // Time to switch phase

    pwmTicks = 0;

    if (phase == PHI_1) {
      phase = PHI_2;
      PORTB = 0x0c;
    }
    else {
      phase = PHI_1;
      PORTB = 0x11;
    }
  }

  if (phase == PHI_1) {
    if (pwmTicks > level1) {
      //cbi(PINB, CHANNEL1_PIN);
      digitalWrite(CHANNEL1_PIN, 0);
    }
#if 1
    if (pwmTicks > level2) {
      //cbi(PINB, CHANNEL2_PIN);
      digitalWrite(CHANNEL2_PIN, 0);
    }
#endif
  }
  else {
    if (pwmTicks > level3) {
      //cbi(PINB, CHANNEL3_PIN);
      digitalWrite(CHANNEL3_PIN, 0);
    }
#if 1
    if (pwmTicks > level4) {
      //cbi(PINB, CHANNEL4_PIN);
      digitalWrite(CHANNEL4_PIN, 0);
    }
#endif
  }
}

inline void idle() {
  //cbi(MCUCR, SM0);
  //cbi(MCUCR, SM1);
  sbi(MCUCR, SE);  
  __asm__ __volatile__ ( "sleep" "\n\t" :: );
}

void setup() {
  fx[0] = new Effect(&level1, 0);
  fx[1] = new Effect(&level2, 180);
  fx[2] = new Effect(&level3, 360);
  fx[3] = new Effect(&level4, 540);

  //DDRB = 0x3f;  
  pinMode(CHANNEL1_PIN, OUTPUT);
  pinMode(CHANNEL2_PIN, OUTPUT);
  pinMode(CHANNEL3_PIN, OUTPUT);
  pinMode(CHANNEL4_PIN, OUTPUT);

  pinMode(DEBUG_PIN, OUTPUT);
  
  // All outputs on
  //PORTB = 0x00;
  digitalWrite(CHANNEL1_PIN, 1);
  digitalWrite(CHANNEL2_PIN, 1);
  digitalWrite(CHANNEL3_PIN, 1);
  digitalWrite(CHANNEL4_PIN, 1);

  // Disable interrupts while we set things up
  cli();

  // Disable sleeping, set sleep mode to IDLE
  cbi(MCUCR, SE); 
  cbi(MCUCR, SM0);
  cbi(MCUCR, SM1);

  /*
   * Setup timer 0:
   *  - divide 8MHz system clock by 1
   *  - interrupt on timer compare OCR0A
   */ 

  // Make sure Timer1 is in synchronous mode, i.e. clocked by the system clock.
  cbi(PLLCSR, PCKE);
  
  // Normal mode 
  TCCR0A = 0;

  // Set prescaler to 1
  TCCR0B = 0;
  sbi(TCCR0B, CS00);
  //sbi(TCCR0B, CS01);

  // Reset counter
  TCNT0 = 0;
  
  // Set up the timer compare registers for about 51.2KHz
  OCR0A = 160;
  
  // Enable timer 1 compare interrupt A
  TIMSK = 0;
  sbi(TIMSK, OCIE0A); 

  // Enable interrupts
  sei();
}

void idleFor(int mS) {
#if 1
  register int ticksToWait = mS * 51;

  systemTicks = 0;
  while (systemTicks < ticksToWait)
    idle(); 
#else
  delay(mS);
#endif
}

void loop() {
#if 0
  while (1) {
    digitalWrite(0, 0);
    digitalWrite(2, 1);
    digitalWrite(3, 1);
    digitalWrite(4, 0);

    idleFor(1000);

    digitalWrite(0, 1);
    digitalWrite(2, 0);
    digitalWrite(3, 0);
    digitalWrite(4, 1);

    idleFor(1000);
  }
#else
  /*while(1) {
    while (level1 < 128) {
      level1++;
      idleFor(10);
    }

    while (level1 > 0) {
      level1--;
      idleFor(10);
    }
  }*/
  while (1) {
    for (int i=0; i<4; i++) {
      fx[i]->step();
    }

    idleFor(6);
  }
#endif
}
