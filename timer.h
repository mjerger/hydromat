#pragma once

#include <Arduino.h>

class Timer
{
  public:

    Timer(int ms) : 
      interval_ms(ms), 
      last(0)
    {}

    bool tick(int ms) {
      last += ms;

      if (last >= interval_ms) {
        last -= interval_ms * (last / interval_ms);
        return true;
      }
      
      return false;
    }

    void reset() { last = 0; }

  private:
  
    const int interval_ms;
    int last;
};

