#pragma once

#include <Arduino.h>

class Switch 
{
  public:

    Switch(uint8_t pin_a, uint8_t pin_b) : 
      pin_a(pin_a), 
      pin_b(pin_b),
      pos(1)
    {}

    const int getPos() { return pos; }

    typedef void (*OnChange)(int current, int last);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    void init() {
      pinMode(pin_a, INPUT_PULLUP);
      pinMode(pin_b, INPUT_PULLUP);
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
      int a = 1-digitalRead(pin_a);
      int b = 1-digitalRead(pin_b);
      return ((a<<1) | b) + 1;
    }

  private:
  
    const uint8_t pin_a;
    const uint8_t pin_b;
    int pos;
    OnChange onChange = nullptr;
};