#pragma once

#if defined(UNO)
# define SYSCLOCK 16000000L  // default UNO clock

# define CHANNEL1_PIN_A 12
# define CHANNEL1_PIN_B 11
# define CHANNEL2_PIN_A 10
# define CHANNEL2_PIN_B 9

# define CHANNEL1_PIN_A_MASK (0b00010000)
# define CHANNEL1_PIN_B_MASK (0b00001000)
# define CHANNEL2_PIN_A_MASK (0b00000100)
# define CHANNEL2_PIN_B_MASK (0b00000010)

# define IR_PIN 8

# define DBGMSG(msg) Serial.print(msg)
# define DBGNL DBGMSG("\n")
#elif defined(ATTINY)
# define SYSCLOCK 16000000L  // Using internal PLL - see README for notes on fuses

# define CHANNEL1_PIN_A 4
# define CHANNEL1_PIN_B 3
# define CHANNEL2_PIN_A 2
# define CHANNEL2_PIN_B 1

# define CHANNEL1_PIN_A_MASK (0b00010000)
# define CHANNEL1_PIN_B_MASK (0b00001000)
# define CHANNEL2_PIN_A_MASK (0b00000100)
# define CHANNEL2_PIN_B_MASK (0b00000010)

# define IR_PIN 0

# define DBGMSG(msg) 
# define DBGNL
#else
# error Unsupported board
#endif

#define HAS_HBRIDGE
#define HAS_IR


// ATtiny85 Pin map
//                        +-\/-+
// Reset/Ain0 (D 5) PB5  1|o   |8  Vcc
//  Xtal/Ain3 (D 3) PB3  2|    |7  PB2 (D 2) Ain1
//  Xtal/Ain2 (D 4) PB4  3|    |6  PB1 (D 1) pwm1
//                  GND  4|    |5  PB0 (D 0) pwm0 
//                        +----+

#if 0
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif
#endif

#ifdef HAS_IR

#include "ir.h"

extern volatile irparams_t irparams;

extern void init_ir();
extern int decode(decode_results_t *results);
extern void dump_ir(decode_results_t *results);

#endif

#ifdef HAS_HBRIDGE
extern void init_hbridge();
#endif

