#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "utils.h"


enum Light {
  SWITCH = 0,
  STATUS_LEFT,
  STATUS_RIGHT,
  BACKLIGHT,
  NUM_LIGHTS,

  SWITCH_MARK,
};

template<uint8_t PIN>
class Lights 
{
  public:

    Lights() : 
      colors  { 
        Effects::ON, 
        Effects::ON, 
        Effects::ON, 
        Effects::ON
      },
      effects { 
        { Effects::off  }, 
        { Effects::off },
        { Effects::off },
        { Effects::off }
      }
    {}

    void init() {
      FastLED.addLeds<WS2812, PIN, GRB>(leds, num_leds);
      FastLED.setBrightness(brightness);
      FastLED.clear();
    }

    void update(int ms) {

      // apply the effects
      for (int m=0; m<NUM_LIGHTS; m++) {
        if (effects[m].func != nullptr) {
          effects[m].t += ms;
          for (int i=0; i<lengths[m]; i++) {
            CRGB color = effects[m].func(i, effects[m].t);
            leds[indices[m] + i] = mult(color, colors[m]);
          }
        }
      }

      FastLED.delay(0);
    }

    void set(Light l, EFunc effect, CRGB color) {
      set(l, effect);
      set(l, color);
    }

    void set(Light l, CRGB color) {
      colors[l] = color;
    }

    void set(Light l, EFunc effect) {
      effects[l] = { effect, 0 };
    }

    void errorLoop() {
      while (true) flashError();
    }

  protected:

    void flashError() {
      fill_solid(leds, num_leds, CRGB::Red);
      FastLED.show();
      FastLED.delay(200);
      FastLED.clear(true);
      FastLED.delay(800);
    }

  private:

    static constexpr int lengths[] = {5, 3, 3, 26};
    static constexpr int indices[] = {0, 5, 8, 11};
    static constexpr int num_leds = lengths[SWITCH]  +
                                    lengths[STATUS_LEFT]  +
                                    lengths[STATUS_RIGHT] + 
                                    lengths[BACKLIGHT];

    CRGB leds[num_leds];
    
    uint8_t brightness = 255;

    CRGB colors[NUM_LIGHTS];
    Effect effects[NUM_LIGHTS];
    uint8_t intensities[NUM_LIGHTS];
};
