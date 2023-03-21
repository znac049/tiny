#pragma once

class Effect {
  private:
    volatile uint8_t *level;
    int stepNum;
    int numSteps = 720;

  public:
    Effect(volatile uint8_t *channelData, int firstStep, int nSteps);
    void step();
    int getNumSteps();
};