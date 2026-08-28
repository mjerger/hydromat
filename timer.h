#pragma once

#include <Arduino.h>

class Timer
{
  public:

    Timer(int ms, bool repeat = false) : 
      interval_ms(ms),
      repeat(repeat),
      running(true),
      tick(false),
      t(0)
    {}

    bool update(uint32_t ms) {

      if (!running)
        return false;

      t += ms;
      tick = false;
      
      if (t >= interval_ms) {
        if (repeat)
          t -= interval_ms * (t / interval_ms);
        else
          running = false;
        
        tick = true;
      }
      
      return tick;
    }

    uint32_t interval() {
      return interval_ms;
    }

    uint32_t time() {
      return t;
    }

    float progress() {
      return (float) t / (float)interval_ms;
    }

    bool ticked() {
      return tick;
    }

    bool active() {
      return running;
    }
    void start() {
      running = true;
    }

    void stop() {
      running = false;
    }

    void reset() {
      t = 0;
      tick = false;
    }

    void restart() {
      reset();
      start();
    }

  private:
  
    const int interval_ms;
    const bool repeat;
    bool running;
    bool tick;
    uint32_t t;
};

