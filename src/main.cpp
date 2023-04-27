#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Effect.h"

#define CLOCK_FREQUENCY 80000000

#define HAS_IR 1

#define PHI_1 0
#define PHI_2 1

#define CHANNEL1_PIN 4
#define CHANNEL2_PIN 0
#define CHANNEL3_PIN 3
#define CHANNEL4_PIN 2

#if HAS_IR
#define IR_PIN 1

// receiver states
#define STATE_IDLE     2
#define STATE_MARK     3
#define STATE_SPACE    4
#define STATE_STOP     5

#define RAWBUF 50 //76         // Length of raw duration buffer

// Timer related
#define CLKFUDGE 5        // fudge factor for clock interrupt overhead
#define CLK 256           // max value for clock (timer 2)
#define PRESCALE 4        // TIMER1 clock prescale
#define SYSCLOCK 8000000  // default ATtiny clock
#define USECPERTICK 50  // microseconds per clock interrupt tick

// timer clocks per microsecond
#define CLKSPERUSEC (SYSCLOCK/PRESCALE/1000000)   

#define INIT_TIMER_COUNT1 (CLK - USECPERTICK*CLKSPERUSEC + CLKFUDGE)
#define RESET_TIMER1 TCNT1 = INIT_TIMER_COUNT1

// IR detector output is active low
#define MARK  0
#define SPACE 1

#define _GAP 5000 // Minimum map between transmissions
#define GAP_TICKS (_GAP/USECPERTICK)

// information for the interrupt handler
typedef struct {
  uint8_t recvpin;           // pin for IR data from detector
  uint8_t rcvstate;          // state machine
  unsigned int timer;     // state timer, counts 50uS ticks.
  unsigned int rawbuf[RAWBUF]; // raw data
  uint8_t rawlen;         // counter of entries in rawbuf
} irparams_t;

volatile irparams_t irparams;
#endif

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

#if HAS_IR
ISR(TIM1_OVF_vect)
{
  // Reset Timer 1
  TCNT1 = INIT_TIMER_COUNT1;

  uint8_t irdata = (uint8_t)digitalRead(irparams.recvpin);

  irparams.timer++; // One more 50us tick
  if (irparams.rawlen >= RAWBUF) {
    // Buffer overflow
    irparams.rcvstate = STATE_STOP;
  }
  switch(irparams.rcvstate) {
  case STATE_IDLE: // In the middle of a gap
    if (irdata == MARK) {
      if (irparams.timer < GAP_TICKS) {
        // Not big enough to be a gap.
        irparams.timer = 0;
      } 
      else {
        // gap just ended, record duration and start recording transmission
        irparams.rawlen = 0;
        irparams.rawbuf[irparams.rawlen++] = irparams.timer;
        irparams.timer = 0;
        irparams.rcvstate = STATE_MARK;
      }
    }
    break;
    
  case STATE_MARK: // timing MARK
    if (irdata == SPACE) {   // MARK ended, record time
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_SPACE;
    }
    break;

  case STATE_SPACE: // timing SPACE
    if (irdata == MARK) { // SPACE just ended, record it
      irparams.rawbuf[irparams.rawlen++] = irparams.timer;
      irparams.timer = 0;
      irparams.rcvstate = STATE_MARK;
    } 
    else { // SPACE
      if (irparams.timer > GAP_TICKS) {
        // big SPACE, indicates gap between codes
        // Mark current code as ready for processing
        // Switch to STOP
        // Don't reset timer; keep counting space width
        irparams.rcvstate = STATE_STOP;
      } 
    }
    break;

  case STATE_STOP: // waiting, measuring gap
    if (irdata == MARK) { // reset gap timer
      irparams.timer = 0;
    }
    break;
  }
}
#endif

inline void idle() {
  //cbi(MCUCR, SM0);
  //cbi(MCUCR, SM1);
  sbi(MCUCR, SE);  
  __asm__ __volatile__ ( "sleep" "\n\t" :: );
}

void setup() {
  const int numSteps = 3000;
  const int quarterStep = numSteps/4;

  fx[0] = new Effect(&level1, 0, numSteps);
  fx[1] = new Effect(&level2, quarterStep, numSteps);
  fx[2] = new Effect(&level3, quarterStep*2, numSteps);
  fx[3] = new Effect(&level4, quarterStep*3, numSteps);

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
  register int ticksToWait = mS * 30;

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
