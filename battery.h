#pragma once

enum BatteryStatus 
{
  BATT_CRITICAL,
  BATT_LOW,
  BATT_OK,
  BATT_CHARGING,
  BATT_OVERCHARGE
};

class BatteryLevel 
{
  public:
    BatteryStatus status;
    const char* name;
    const uint16_t mV;
};


class Battery
{
  public:

    Battery() : 
      voltage_mV(0),
      hysteresis_mV(50),
      levels{{ BATT_CRITICAL,   "critical"  , 11000 },
             { BATT_LOW,        "low"       , 11800 },
             { BATT_OK,         "ok"        , 13700 },
             { BATT_CHARGING,   "charging"  , 14400 },
             { BATT_OVERCHARGE, "overcharge", 14500 }},
      status(BATT_OK)
    {}

    typedef void (*OnChange)(const BatteryLevel& level);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    const BatteryLevel& getLevel() {
      return levels[status];
    }

    const float getVoltage() {
      return (float)voltage_mV / 1000.0f;
    }

    const uint8_t getCharge() {
      uint32_t v = voltage_mV;
      if (v <= 11600)       // < 11.6V
        return 0;
      else if (v <= 12200)  // 11.6V to 12.2V
        return ((v - 11600) * 50) / 600;
      else if (v <= 12850)  // 12.2V to 12.85V
        return 50 + ((v - 12200) * 50) / 650;
      else                  // > 12.85V
        return 100;
    }

    void updateVoltage(uint16_t mV) {
      voltage_mV = mV;

      // current level
      const BatteryLevel& level = levels[status];
      uint8_t s = (uint8_t)status;

      // find new level upwards
      bool changed = false;
      if (s < 4 && mV > levels[s].mV + hysteresis_mV) {
        s++;
        changed = true;
      }

      // find new level downwards
      if (!changed && s > 0 && mV < levels[s-1].mV - hysteresis_mV) {
        s--;
        changed = true;
      }

      if (changed) {
        status = (BatteryStatus)s;
        if (onChange)
          onChange(levels[status]);
      }
    }

  private:

    uint16_t voltage_mV;
    const uint16_t hysteresis_mV;

    const BatteryLevel levels[5];

    BatteryStatus status;

    OnChange onChange = nullptr;
};