#pragma once

#include <Arduino.h>

template<uint8_t PIN_A, uint8_t PIN_B>
class Switch 
{
  public:

    Switch() : pos(1) {}

    const int position() { return pos; }

    typedef void (*OnChange)(int current, int last);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    void init() {
      pinMode(PIN_A, INPUT_PULLUP);
      pinMode(PIN_B, INPUT_PULLUP);
    }

    void update() {
      int current = read();
      if (pos != current) {

        // switch contacts bounce during rotation
        delay(50);
        current = read();

        int last = pos;
        pos = current;

        if (onChange)
          onChange(current, last);
      }
    }

    const int read() {
      int a = 1 - digitalRead(PIN_A);
      int b = 1 - digitalRead(PIN_B);
      return ((a<<1) | b) + 1;
    }

  private:
  
    int pos;
    OnChange onChange = nullptr;
};