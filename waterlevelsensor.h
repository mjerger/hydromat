#pragma once

#include "sensor.h"

enum WaterStatus
{
  WATER_TOO_LOW,
  WATER_MINIMUM,
  WATER_NORMAL,
  WATER_MAXIMUM
};

class WaterLevel 
{
  public:
    WaterStatus    status;
    const char*    name;
    const uint16_t raw;
    const uint8_t  percent;
};

struct WaterLevelSample 
{
  uint16_t raw;
  uint8_t level;
};


template<uint8_t APIN = A0>
class WaterLevelSensor : public Sensor<WaterLevelSample>
{
  public:

    WaterLevelSensor(
      const char* name = "level",
      uint32_t    samples_per_hour = 3600,
      uint32_t    offset_ms = 0
    ) : 
      Sensor(name),
      sample_ms(3600000 / samples_per_hour),
      sample_offset_ms(offset_ms),
      levels {{ WATER_TOO_LOW, "too_low",    0,  25 },
              { WATER_MINIMUM, "minimum",  350,  50 },
              { WATER_NORMAL,  "normal",   600,  75 },
              { WATER_MAXIMUM, "maximum", 1000, 100 }},
      level(WATER_NORMAL)
    {}

    typedef void (*OnChange)(const WaterLevel& level);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    const WaterLevel& getLevel() const {
      return levels[level];
    }

    void init() {
      pinMode(APIN, INPUT);
    }

    void update(uint32_t ms) {
      static uint32_t last = sample_offset_ms;
      last += ms;

      if (last >= sample_ms) {
        last -= sample_ms * (last / sample_ms);

        WaterLevelSample s;
        
        // TODO? maybe sample a few times?
        s.raw = analogRead(APIN);
        s.level = 0;

        // find next closest level
        for (uint8_t i=3; i>0; i--) {
          if (s.raw >= levels[i].raw) {
            s.level = i;
            break;
          }
        }

        Sensor::push(s);

        if (level != (WaterStatus)s.level) {
          level = (WaterStatus)s.level;
        
          auto& l = levels[s.level];
          Serial.printf(PSTR("Level %s %d %s %d%% (%d raw)\n"), name, s.level, l.name, l.percent, s.raw);
          
          if (onChange)
            onChange(l);
        }

      }
    }


  private:

    const uint32_t sample_ms;
    const uint32_t sample_offset_ms;

    const WaterLevel levels[4];
    WaterStatus level;

    OnChange onChange = nullptr;
};