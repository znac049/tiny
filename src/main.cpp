#include <Arduino.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "lights.h"
#include "ir.h"
#include "Effect.h"

#ifdef HAS_HBRIDGE
extern volatile int systemTicks;

extern volatile uint8_t level1;
extern volatile uint8_t level2;
extern volatile uint8_t level3;
extern volatile uint8_t level4;

Effect *fx[4];
#endif

inline void idle() {
  //cbi(MCUCR, SM0);
  //cbi(MCUCR, SM1);
  sbi(MCUCR, SE);  
  __asm__ __volatile__ ( "sleep" "\n\t" :: );
}

void setup()
{
  const int numSteps = 750;
  const int quarterStep = numSteps/4;

  Serial.begin(38400);

  Serial.println("Starting Wedding Lights controller");

#ifdef HAS_HBRIDGE
  fx[0] = new Effect(&level1, 0, numSteps);
  fx[1] = new Effect(&level2, quarterStep, numSteps);
  fx[2] = new Effect(&level3, quarterStep*2, numSteps);
  fx[3] = new Effect(&level4, quarterStep*3, numSteps);
#endif

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

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

#ifdef ATTINY
  // AtTiny has all sorts of crazy places it can be clocked from
  // We just want to clock from the internal 8MHz clock
  cbi(PLLCSR, PCKE);
#endif
 
#ifdef HAS_HBRIDGE
  init_hbridge();
#endif

#ifdef HAS_IR
  init_ir();
#endif

  // Allow interrupts
  sei();
}

void idleFor(int mS) {
#ifdef HAS_HBRIDGE
  register int ticksToWait = mS * 30;

  systemTicks = 0;
  while (systemTicks < ticksToWait)
    idle(); 
#else
  delay(mS);
#endif
}

void loop() {
  unsigned long button = 0;

  while (1) {  
#ifdef HAS_HBRIDGE
    for (int i=0; i<4; i++) {
      fx[i]->step();
    }
#endif

#ifdef HAS_IR
    if (irparams.rcvstate == STATE_STOP) {
      decode_results_t ir;

      if (decode(&ir) == DECODED) {
        if (ir.decode_type == NEC) {
          if (ir.value != REPEAT) {
            button = ir.value;
          }

          switch (button) {
            // ON button
            case 1086259455:
            case 16753245:
              Serial.println("ON");
              digitalWrite(LED_BUILTIN, HIGH);
              break;

            // OFF button
            case 1086275775:
            case 16769565:
              Serial.println("OFF");
              digitalWrite(LED_BUILTIN, LOW);
              break;

            default:
              Serial.print("Button value: "); Serial.println(button);
              break;
          }
        }

#ifdef HAS_HBRIDGE
        for (int i=0; i<4; i++) {
          fx[i]->toggle();
        }
#endif
      }

      irparams.rcvstate = STATE_IDLE;
      irparams.rawlen = 0;
    }
#endif

    idleFor(6);
  }
}
