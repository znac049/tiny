#pragma once

class Effect {
  private:
    volatile uint8_t *level;
    int16_t stepNum;
    int16_t numSteps = 720;
    uint8_t blackout = 0;

  public:
    Effect(volatile uint8_t *channelData, int firstStep, int nSteps);
    void step();
    int getNumSteps();
};