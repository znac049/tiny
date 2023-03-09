#pragma once

class Effect {
  private:
    volatile uint8_t *level;
    int stepNum;

  public:
    Effect(volatile uint8_t *channelData, int firstStep);
    void step();
};