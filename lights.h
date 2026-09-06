#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include "effects.h"
#include "timer.h"
#include "utils.h"


enum Light {
  SWITCH = 0,
  STATUS_LEFT,
  STATUS_RIGHT,
  BACKLIGHT,
  NUM_LIGHTS,
};

template<uint8_t PIN>
class Lights 
{
  public:

    Lights() : 
      max_brightness(255),
      min_brightness(10),
      brightness(max_brightness),
      sleep_timer(10000),
      fadeout_timer(30000),
      colors  { Effects::ON, Effects::ON, Effects::ON, Effects::ON },
      effects { { Effects::off }, { Effects::off }, { Effects::off }, { Effects::off } },
      fadeout { true, false, false, true }
    {}

    void init() {
      FastLED.addLeds<WS2812, PIN, GRB>(leds, num_leds);
      FastLED.setBrightness(255);
      FastLED.clear();
    }

    void update(uint32_t ms) {
      sleep_timer.update(ms);
      fadeout_timer.update(ms);

      // start dimming?
      if (sleep_timer.ticked()) {
        sleep_timer.reset();
        fadeout_timer.restart();
      }
      // dim backlight
      if (fadeout_timer.ticked()) {
        brightness = min_brightness;
        fadeout_timer.reset();
      } else if (fadeout_timer.active()) {
        float progress = min(1.0, 1.0 - fadeout_timer.progress());
        float b = min_brightness * pow((float)max_brightness / (float)min_brightness, progress);
        brightness = (uint8_t)(b + 0.5);
      }

      // apply the effects
      for (int m=0; m<NUM_LIGHTS; m++) {
        if (effects[m].func != nullptr) {
          effects[m].t += ms;
          for (int i=0; i<lengths[m]; i++) {
            CRGB color = effects[m].func(i, effects[m].t);
            leds[indices[m] + i] = mult(color, colors[m]) % (fadeout[m] ? brightness : 0xff);
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

    void wakeUp() {
      fadeout_timer.stop();
      sleep_timer.restart();
      brightness = max_brightness;
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
    
    const uint8_t max_brightness;
    const uint8_t min_brightness;
    uint8_t brightness;
    Timer sleep_timer;
    Timer fadeout_timer;

    CRGB   colors[NUM_LIGHTS];
    Effect effects[NUM_LIGHTS];
    bool   fadeout[NUM_LIGHTS];

};
