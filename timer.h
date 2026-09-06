#pragma once

#include <Arduino.h>

class Timer
{
  public:

    Timer(
      uint32_t ms,
      bool repeat = false,
      uint32_t offset_ms = 0
    ) : 
      interval_ms(ms),
      repeat(repeat),
      offset_ms(offset_ms),
      t(offset_ms),
      running(true),
      tick(false)
    {}

    bool update(uint32_t ms) {

      if (!running)
        return false;

      t += ms;
      tick = false;
      
      if (t >= interval_ms) {
        // skip ticks when we stalled
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
      t = offset_ms;
      tick = false;
    }

    void restart() {
      reset();
      start();
    }

  private:
  
    const uint32_t interval_ms;
    const bool repeat;
    const uint32_t offset_ms;
    uint32_t t;
    bool running;
    bool tick;
};

