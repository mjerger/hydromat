#pragma once

#include "sensor.h"
#include "timer.h"


enum WaterLevelID
{
  WATER_TOO_LOW,
  WATER_MINIMUM,
  WATER_NORMAL,
  WATER_MAXIMUM
};

class WaterLevel
{
  public:
    WaterLevelID   level;
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
      timer(3600000 / samples_per_hour, true, offset_ms),
      levels {{ WATER_TOO_LOW, "too_low",    0,  25 },
              { WATER_MINIMUM, "minimum",  350,  50 },
              { WATER_NORMAL,  "normal",   600,  75 },
              { WATER_MAXIMUM, "maximum", 1000, 100 }},
      current_level(WATER_NORMAL)
    {}

    typedef void (*OnChange)(const WaterLevel& level);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    WaterLevelID level() const {
      return current_level;
    }

    void init() {
      pinMode(APIN, INPUT);
    }

    void update(uint32_t ms) {
      if (timer.update(ms)) {

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

        if (current_level != (WaterLevelID)s.level) {
          current_level = (WaterLevelID)s.level;
        
          auto& l = levels[s.level];
          Serial.printf(PSTR("Level %s %d %s %d%% (%d raw)\n"), sensorName(), s.level, l.name, l.percent, s.raw);
          
          if (onChange)
            onChange(l);
        }
      }
    }

  private:

    Timer timer;

    const WaterLevel levels[4];
    WaterLevelID current_level;

    OnChange onChange = nullptr;
};