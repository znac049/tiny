#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include "lights.h"
#include "ir.h"

#ifdef HAS_IR

volatile irparams_t irparams;

#ifdef ATTINY
ISR(TIM1_OVF_vect)
#else
ISR(TIMER1_OVF_vect)
#endif
{
  // Reset Timer 1
#ifdef ATTINY
  TCNT1 = INIT_TIMER_COUNT1;
#else
  TCNT1H = 0xFF;
  TCNT1L = (uint8_t) INIT_TIMER_COUNT1;
#endif

  uint8_t irdata = (uint8_t)digitalRead(IR_PIN);

  irparams.timer++; // One more 25us tick
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

void init_timer1()
{
  // Initialise state
  irparams.rcvstate = STATE_IDLE;
  irparams.timer = 0;
  irparams.rawlen=0;

  /*
   * Setup Timer 1
   *   on the ATTiny, this is an 8-bit timer
   *   on the Mega/UNO, this is a 16-bit timer
   * 
   *   on both, it is an up counter
  */

  // Prescale /8 (16M/8 = 0.5 microseconds per tick)
  // Therefore, the timer interval can range from 0.5 to 128 microseconds
  // depending on the reset value (255 to 0)
#ifdef ATTINY
  TCCR1 = _BV(CS12);                // Clock / 8
#else
  TCCR1A = 0;
  TCCR1B = _BV(CS11);               // Clock / 8
  TCCR1C = 0;
#endif

  // Enable Timer 1 overflow interrupt
#ifdef ATTINY
  sbi(TIMSK, TOIE1);
#else
  TIMSK1 = _BV(TOIE1);
#endif

  // Reset Timer 1
#ifdef ATTINY
  TCNT1 = INIT_TIMER_COUNT1;
#else
  TCNT1H = 0xFF;
  TCNT1L = (uint8_t) INIT_TIMER_COUNT1;
#endif
}

void init_ir()
{
  pinMode(IR_PIN, INPUT);

  init_timer1();
}

long decodeNEC(decode_results_t *results) {
  long data = 0;
  int offset = 1; // Skip first space
  // Initial mark
  if (!MATCH_MARK(results->rawbuf[offset], NEC_HDR_MARK)) {
    //Serial.println("Initial MARK missing");
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
    //Serial.println("NEC Repeat");
    return DECODED;
  }
  if (irparams.rawlen < 2 * NEC_BITS + 4) {
    //Serial.println("Not enough raw data");
    return ERR;
  }
  // Initial space  
  if (!MATCH_SPACE(results->rawbuf[offset], NEC_HDR_SPACE)) {
    //Serial.println("Initial SPACE missing");
    return ERR;
  }
  offset++;
  for (int i = 0; i < NEC_BITS; i++) {
    if (!MATCH_MARK(results->rawbuf[offset], NEC_BIT_MARK)) {
      //Serial.print("NEC Bit #"); Serial.print(i); Serial.print(" - "); Serial.print(results->rawbuf[offset]); Serial.println(" - MATCH_MARK failed");
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
      //Serial.print("NEC Bit #"); Serial.print(i); Serial.print(" - "); Serial.print(results->rawbuf[offset]); Serial.println(" - MATCH_SPACE failed");
      return ERR;
    }
    offset++;
  }
  // Success
  results->bits = NEC_BITS;
  results->value = data;
  results->decode_type = NEC;

  //Serial.print("Decoded data: "); Serial.println(data);
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

#ifdef DEBUG
void print_range(int centre, const char *tag) {
  Serial.print(tag);
  Serial.print(" range(");
  Serial.print(centre);
  Serial.print("): ");
  Serial.print(TICKS_LOW(centre));
  Serial.print(" - ");
  Serial.println(TICKS_HIGH(centre));
}

void dump_ir(decode_results_t *results)
{
  Serial.println("IR Results structure");
  Serial.print(" Decode type: "); Serial.println(results->decode_type);
  Serial.print("       Value: "); Serial.println(results->value, HEX);
  Serial.print("      RawLen:" ); Serial.println(results->rawlen);
  Serial.print("      # Bits: "); Serial.println(results->bits);
  Serial.print("    Raw data: ");
  for (int i=0; i<results->rawlen; i++) {
    if (i % 16 == 0) {
      if (i != 0) { 
        Serial.println("");
        Serial.print("              ");
      }
      if (i<10) {
        Serial.print(" ");
      }
      Serial.print(i); Serial.print(": ");
    }
    else { 
      Serial.print("  ");
    }
    Serial.print(results->rawbuf[i]);
  }
  Serial.println("");

  Serial.println("Interrupt structure");
  Serial.print("       State: "); Serial.println(irparams.rcvstate);
  Serial.print("       Timer: "); Serial.println(irparams.timer);

  Serial.println("Random stuff");
  Serial.print("LTOL: "); Serial.println(LTOL);
  Serial.print("UTOL: "); Serial.println(UTOL);

  print_range(NEC_HDR_MARK, "NEC_HDR_MARK");
  print_range(NEC_HDR_SPACE, "NEC_HDR_SPACE");
  print_range(NEC_BIT_MARK, "NEC_BIT_SPACE");
  print_range(NEC_ONE_SPACE, "NEC_ONE_SPACE");
  print_range(NEC_ZERO_SPACE, "NEC_ZERO_SPACE");

  show_calcs(31);
  show_calcs(11);
}
#endif /* DEBUG */

#endif /* HAS_IR */