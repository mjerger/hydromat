#pragma once

enum BatteryLevelID
{
  BATT_CRITICAL,
  BATT_LOW,
  BATT_NORMAL,
  BATT_FULL,
  BATT_CHARGING,
  BATT_OVERCHARGE,
  NUM_BATT_LEVELS
};

class BatteryLevel 
{
  public:
    BatteryLevelID level;
    const char* name;
    const uint16_t mV;
    CRGB color;
};

class Battery
{
  public:

    Battery() : 
      voltage_mV(0),
      hysteresis_mV(50),
      levels{{ BATT_CRITICAL,   "critical"  , 11800, CRGB::Red    },
             { BATT_LOW,        "low"       , 12000, CRGB::Orange },
             { BATT_NORMAL,     "normal"    , 12600, CRGB::Yellow },
             { BATT_FULL,       "full"      , 12800, CRGB::Green  },
             { BATT_CHARGING,   "charging"  , 14400, CRGB::Blue   },
             { BATT_OVERCHARGE, "overcharge", 14500, CRGB::Red    }},
      current_level(BATT_NORMAL)
    {}

    typedef void (*OnChange)(const BatteryLevel& level);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    BatteryLevelID level() const {
      return current_level;
    }

    float voltage() const {
      return (float)voltage_mV / 1000.0f;
    }

    uint16_t millivolts() const {
      return voltage_mV;
    }

    uint8_t charge() const {
      uint16_t v = voltage_mV;
      if (v <= 11600)       // < 11.6V
        return 0;
      else if (v <= 12200)  // 11.6V to 12.2V
        return ((v - 11600) * 50) / 600;
      else if (v <= 12850)  // 12.2V to 12.85V
        return 50 + ((v - 12200) * 50) / 650;
      else                  // > 12.85V
        return 100;
    }

    const CRGB& color() const {
      return levels[current_level].color;
    }

    void updateVoltage(uint16_t mV) {
      voltage_mV = mV;

      // current level
      const BatteryLevel& level = levels[current_level];
      uint8_t s = (uint8_t)current_level;

      // find new level upwards with hysteresis
      bool changed = false;
      if (s < NUM_BATT_LEVELS-1 && mV > levels[s].mV + hysteresis_mV) {
        s++;
        changed = true;
      }

      // find new level downwards
      if (!changed && s > 0 && mV < levels[s-1].mV) {
        s--;
        changed = true;
      }

      if (changed) {
        current_level = (BatteryLevelID)s;
        if (onChange)
          onChange(levels[current_level]);
      }
    }

  private:

    uint16_t voltage_mV;
    const uint16_t hysteresis_mV;

    const BatteryLevel levels[NUM_BATT_LEVELS];

    BatteryLevelID current_level;

    OnChange onChange = nullptr;
};