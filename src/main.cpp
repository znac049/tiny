#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "Effect.h"

#define HAS_IR 0

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

#define RAWBUF 76         // Length of raw duration buffer

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

#define ERR 0
#define DECODED 1

// Values for decode_type
#define NEC 1
#define UNKNOWN -1

#define NEC_BITS 32

// pulse parameters in usec
#define NEC_HDR_MARK	9000
#define NEC_HDR_SPACE	4500
#define NEC_BIT_MARK	560
#define NEC_ONE_SPACE	1600
#define NEC_ZERO_SPACE	560
#define NEC_RPT_SPACE	2250

#define LTOL (1.0 - TOLERANCE/100.) 
#define UTOL (1.0 + TOLERANCE/100.) 

#define TOLERANCE 25  // percent tolerance in measurements
#define TICKS_LOW(us) (int) (((us)*LTOL/USECPERTICK))
#define TICKS_HIGH(us) (int) (((us)*UTOL/USECPERTICK + 1))

// Marks tend to be 100us too long, and spaces 100us too short
// when received due to sensor lag.
#define MARK_EXCESS 100
#define MATCH(measured_ticks, desired_us) ((measured_ticks) >= TICKS_LOW(desired_us) && (measured_ticks) <= TICKS_HIGH(desired_us))
#define MATCH_MARK(measured_ticks, desired_us) MATCH(measured_ticks, (desired_us) + MARK_EXCESS)
#define MATCH_SPACE(measured_ticks, desired_us) MATCH((measured_ticks), (desired_us) - MARK_EXCESS)

// Decoded value for NEC when a repeat code is received
#define REPEAT 0xffffffff

// information for the interrupt handler
typedef struct {
  uint8_t rcvstate;          // state machine
  unsigned int timer;     // state timer, counts 50uS ticks.
  unsigned int rawbuf[RAWBUF]; // raw data
  uint8_t rawlen;         // counter of entries in rawbuf
} irparams_t;

typedef struct {
  int decode_type; // NEC, SONY, RC5, UNKNOWN
  unsigned long value; // Decoded value
  int bits; // Number of bits in decoded value
  volatile unsigned int *rawbuf; // Raw intervals in .5 us ticks
  int rawlen; // Number of records in rawbuf.
} decode_results_t;

volatile irparams_t irparams;
#endif

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

  uint8_t irdata = (uint8_t)digitalRead(IR_PIN);

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
  const int numSteps = 750;
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

#if HAS_IR
  pinMode(IR_PIN, INPUT);
#endif  

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
  
  // Enable timer 0 compare interrupt A
  TIMSK = 0;
  sbi(TIMSK, OCIE0A); 

#if HAS_IR
  // Initialise state
  irparams.rcvstate = STATE_IDLE;
  irparams.timer = 0;
  irparams.rawlen=0;

  // Setup Timer 1

  // Prescale /4 (8M/4 = 0.5 microseconds per tick)
  // Therefore, the timer interval can range from 0.5 to 128 microseconds
  // depending on the reset value (255 to 0)
  TCCR1 = _BV(CS11) | _BV(CS10);

  // enable eimer 1 overflow interrupt
  //TIMSK |= _BV(TOIE1);
  sbi(TIMSK, TOIE1);

  RESET_TIMER1;
#endif

  // Enable interrupts
  sei();
}

#if HAS_IR
long decodeNEC(decode_results_t *results) {
  long data = 0;
  int offset = 1; // Skip first space
  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], NEC_HDR_MARK)) {
    return ERR;
  }
  offset++;
  // Check for repeat
  if (irparams.rawlen == 4 &&
    MATCH_SPACE(results->rawbuf[offset], NEC_RPT_SPACE) &&
    MATCH_MARK(results->rawbuf[offset+1], NEC_BIT_MARK)) {
    results->bits = 0;
    results->value = REPEAT;
    results->decode_type = NEC;
    return DECODED;
  }
  if (irparams.rawlen < 2 * NEC_BITS + 4) {
    return ERR;
  }
  // Initial space  
  if (!MATCH_SPACE(results->rawbuf[offset], NEC_HDR_SPACE)) {
    return ERR;
  }
  offset++;
  for (int i = 0; i < NEC_BITS; i++) {
    if (!MATCH_MARK(results->rawbuf[offset], NEC_BIT_MARK)) {
      return ERR;
    }
    offset++;
    if (MATCH_SPACE(results->rawbuf[offset], NEC_ONE_SPACE)) {
      data = (data << 1) | 1;
    } 
    else if (MATCH_SPACE(results->rawbuf[offset], NEC_ZERO_SPACE)) {
      data <<= 1;
    } 
    else {
      return ERR;
    }
    offset++;
  }
  // Success
  results->bits = NEC_BITS;
  results->value = data;
  results->decode_type = NEC;
  return DECODED;
}

int decode(decode_results_t *results) {
  results->rawbuf = irparams.rawbuf;
  results->rawlen = irparams.rawlen;
  if (irparams.rcvstate != STATE_STOP) {
    return ERR;
  }

  if (decodeNEC(results)) {
    return DECODED;
  }

  if (results->rawlen >= 6) {
    // Only return raw buffer if at least 6 bits
    results->decode_type = UNKNOWN;
    results->bits = 0;
    results->value = 0;
    return DECODED;
  }
  // Throw away and start over
  irparams.rcvstate = STATE_IDLE;
  irparams.rawlen=0;

  return ERR;
}
#endif

void idleFor(int mS) {
  register int ticksToWait = mS * 30;

  systemTicks = 0;
  while (systemTicks < ticksToWait)
    idle(); 
}

void loop() {
  while (1) {
    for (int i=0; i<4; i++) {
      fx[i]->step();
    }

#if HAS_IR
    if (irparams.rcvstate == STATE_STOP) {
      decode_results_t ir;

      //if (decode(&ir) == DECODED) {
        decode(&ir);
        for (int i=0; i<4; i++) {
          fx[i]->toggle();
        }
      //}
      irparams.rcvstate = STATE_IDLE;
      irparams.rawlen = 0;
    }
#endif

    idleFor(6);
  }
}
