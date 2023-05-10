#include <Arduino.h>

#include "lights.h"
#include "h-bridge.h"

int pwmTicks = 0;
int phase = PHI_1;

volatile int systemTicks = 0;

volatile uint8_t level1 = 0;
volatile uint8_t level2 = 0;
volatile uint8_t level3 = 0;
volatile uint8_t level4 = 0;
//volatile int counter;

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
      //PORTB = 0x0c;
      PORTB = _BV(CHANNEL1_PIN_B) | _BV(CHANNEL2_PIN_B);
    }
    else {
      phase = PHI_1;
      //PORTB = 0x11;
      PORTB = _BV(CHANNEL1_PIN_A) | _BV(CHANNEL2_PIN_A);
    }
  }

  if (phase == PHI_1) {
    if (pwmTicks > level1) {
      //cbi(PINB, CHANNEL1_PIN_A);
      digitalWrite(CHANNEL1_PIN_A, 0);
    }
    if (pwmTicks > level2) {
      //cbi(PINB, CHANNEL1_PIN_B);
      digitalWrite(CHANNEL2_PIN_A, 0);
    }
  }
  else {
    if (pwmTicks > level3) {
      //cbi(PINB, CHANNEL2_PIN_A);
      digitalWrite(CHANNEL1_PIN_B, 0);
    }
    if (pwmTicks > level4) {
      //cbi(PINB, CHANNEL2_PIN_B);
      digitalWrite(CHANNEL2_PIN_B, 0);
    }
  }
}

/*
 * Timer zero is used for the H-Bridge PWM
 */
void setup_timer0()
{
  // Normal mode 
  TCCR0A = 0;

  // Set prescaler to 1
  TCCR0B = 0;
  sbi(TCCR0B, CS00);
  
  // Reset counter
  TCNT0 = 0;
  
  // Set up the timer compare registers for about 51.2KHz
  OCR0A = 160;
  
#ifdef ATTINY
  // Enable timer 0 compare interrupt A
  TIMSK = 0;
  sbi(TIMSK, OCIE0A); 
#else
  TIMSK0 = _BV(OCIE0A);
#endif
}

void init_hbridge()
{
  pinMode(CHANNEL1_PIN_A, OUTPUT);
  pinMode(CHANNEL1_PIN_B, OUTPUT);
  pinMode(CHANNEL2_PIN_A, OUTPUT);
  pinMode(CHANNEL2_PIN_B, OUTPUT);

  // All outputs on
  digitalWrite(CHANNEL1_PIN_A, 1);
  digitalWrite(CHANNEL1_PIN_B, 1);
  digitalWrite(CHANNEL2_PIN_A, 1);
  digitalWrite(CHANNEL2_PIN_B, 1);

  setup_timer0();
}