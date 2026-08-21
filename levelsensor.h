#pragma once

#include "sensor.h"


class Level {
  public:
    const char* name;
    const uint16_t raw;
    const uint8_t percent;
};


struct LevelSample {
  uint16_t raw;
  uint8_t level;
};


template<uint8_t APIN = A0>
class LevelSensor : Sensor<LevelSample>
{
  public:

    LevelSensor(
      const char* name = "level",
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0)
    : 
      Sensor(name),
      sample_ms(3600000 / samples_per_hour),
      sample_offset_ms(offset_ms),
      levels {{"too_low",    0,  25},
              {"minimum",  100,  50},
              {"normal",   500,  70},
              {"maximum", 1000, 100}}
    {}

    void init() {
      pinMode(APIN, INPUT);
    }

    void update(int ms) {
      static uint32_t last = sample_offset_ms;
      last += ms;

      if (last >= sample_ms) {
        last -= sample_ms * (last / sample_ms);

        LevelSample s;
        
        // TODO? maybe sample a few times?
        s.raw = analogRead(APIN);
        s.level = 0;

        // find next closest level
        for (uint8_t i=4; i>0; i--) {
          if (s.raw >= levels[i].raw) {
            s.level = i;
            break;
          }
        }

        Sensor::push(s);

        Serial.printf("Level %s %d%% (%d raw)\n", name, s.level, s.raw);
      }
    }
    
  private:

    const Level levels[4];

    const uint32_t sample_ms;
    const uint32_t sample_offset_ms;
};