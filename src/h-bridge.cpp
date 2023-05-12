#include <Arduino.h>

#include "lights.h"
#include "h-bridge.h"

int pwmTicks = 128;
uint8_t phase = PHI_1;
volatile uint8_t running = 1;

volatile int systemTicks = 0;

volatile uint8_t level1 = 0;
volatile uint8_t level2 = 0;
volatile uint8_t level3 = 0;
volatile uint8_t level4 = 0;
//volatile int counter;

ISR(TIMER0_COMPA_vect) {
  MCUCR &= ~(_BV(SE));    // Disable sleep mode

  systemTicks++;

  // Reset counter
  TCNT0 = 0;

  if (running == 0) {
    return;
  }

  pwmTicks++;

  if (pwmTicks >= 128) {
    // Time to switch phase

    pwmTicks = 0;

    if (phase == PHI_1) {
      phase = PHI_2;
      //PORTB = 0x0c;
      //PORTB = _BV(CHANNEL1_PIN_B) | _BV(CHANNEL2_PIN_B);
      PORTB |= CHANNEL1_PIN_B_MASK | CHANNEL2_PIN_B_MASK;
      PORTB &= ~(CHANNEL1_PIN_A_MASK | CHANNEL2_PIN_A_MASK);
    }
    else {
      phase = PHI_1;
      //PORTB = 0x11;
      //PORTB = _BV(CHANNEL1_PIN_A) | _BV(CHANNEL2_PIN_A);
      PORTB |= CHANNEL1_PIN_A_MASK | CHANNEL2_PIN_A_MASK;
      PORTB &= ~(CHANNEL1_PIN_B_MASK | CHANNEL2_PIN_B_MASK);
    }
  }

  if (phase == PHI_1) {
    if (pwmTicks > level1) {
      //cbi(PINB, CHANNEL1_PIN_A);
      //digitalWrite(CHANNEL1_PIN_A, 0);
      PORTB &= ~CHANNEL1_PIN_A_MASK;
    }
    if (pwmTicks > level2) {
      //cbi(PINB, CHANNEL1_PIN_B);
      //digitalWrite(CHANNEL2_PIN_A, 0);
      PORTB &= ~CHANNEL2_PIN_A_MASK;
    }
  }
  else {
    if (pwmTicks > level3) {
      //cbi(PINB, CHANNEL2_PIN_A);
      //digitalWrite(CHANNEL1_PIN_B, 0);
      PORTB &= ~CHANNEL1_PIN_B_MASK;
    }
    if (pwmTicks > level4) {
      //cbi(PINB, CHANNEL2_PIN_B);
      //digitalWrite(CHANNEL2_PIN_B, 0);
      PORTB &= ~CHANNEL2_PIN_B_MASK;
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
  //TCCR0B = 0;
  TCCR0B = _BV(CS00);

  // Reset counter
  TCNT0 = 0;
  
  // Set up the timer compare registers for about 51.2KHz
  OCR0A = 160;

#ifdef ATTINY
  // Enable timer 0 compare interrupt A
  TIMSK = _BV(OCIE0A);
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

  // All outputs off
  digitalWrite(CHANNEL1_PIN_A, LOW);
  digitalWrite(CHANNEL1_PIN_B, LOW);
  digitalWrite(CHANNEL2_PIN_A, LOW);
  digitalWrite(CHANNEL2_PIN_B, LOW);

  setup_timer0();
}
