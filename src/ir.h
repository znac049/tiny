#pragma once

#ifdef HAS_IR

// IR receiver states
#define STATE_IDLE     2
#define STATE_MARK     3
#define STATE_SPACE    4
#define STATE_STOP     5

#define RAWBUF 76         // Length of raw duration buffer

// Timer related
#define CLKFUDGE 5        // fudge factor for clock interrupt overhead

#ifdef ATTINY
#define CLK 256           // max value for clock (Timer 1 - 8 bit)
#else
#define CLK 65536         // Max value for clock (Timer 1 - 16 bit)
#endif
#define PRESCALE 8        // TIMER1 clock prescale

// microseconds per clock interrupt tick
#define USEC_PER_TICK 50

// timer clocks per microsecond
#define CLKS_PER_USEC (SYSCLOCK/PRESCALE/1000000)   

#define INIT_TIMER_COUNT1 (CLK - USEC_PER_TICK*CLKS_PER_USEC + CLKFUDGE)
#define RESET_TIMER1 TCNT1 = INIT_TIMER_COUNT1

// IR detector output is active low
#define MARK  0
#define SPACE 1

#define _GAP 5000 // Minimum gap between transmissions
#define GAP_TICKS (_GAP/USEC_PER_TICK)

#define ERR 0
#define DECODED 1

// Values for decode_type
#define NEC 1
#define UNKNOWN -1

#define NEC_BITS 32

// NEC pulse parameters in usec
#define NEC_HDR_MARK	9000
#define NEC_HDR_SPACE	4500
#define NEC_BIT_MARK	560
#define NEC_ONE_SPACE	1600
#define NEC_ZERO_SPACE 560
#define NEC_RPT_SPACE	2250

#define LTOL (1.0 - TOLERANCE/100.) 
#define UTOL (1.0 + TOLERANCE/100.) 

#define TOLERANCE 30  // percent tolerance in measurements
#define TICKS_LOW(us) (int) (((us)*LTOL/USEC_PER_TICK))
#define TICKS_HIGH(us) (int) (((us)*UTOL/USEC_PER_TICK + 1))

// Marks tend to be 100us too long, and spaces 100us too short
// when received due to sensor lag.
#define MARK_EXCESS 0 //100
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

#endif
