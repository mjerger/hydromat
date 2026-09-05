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
      tolerance_mV(200),
      levels{{ BATT_CRITICAL,   "critical"  , 10000 },
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

    void updateVoltage(uint16_t mV) {
      voltage_mV = mV;

      // current level
      const BatteryLevel& level = levels[status];
      uint8_t s = (uint8_t)status;

      // find new level upwards
      bool changed = false;
      while (s < 4 && mV > level.mV + tolerance_mV) {
        s++;
        changed = true;
      }

      // find new level downwards
      if (!changed) {
        while (s > 0 && mV < level.mV - tolerance_mV) {
          s--;
          changed = true;
        }
      }

      if (changed) {
        status = (BatteryStatus)s;
        if (onChange)
          onChange(levels[status]);
      }
    }

  private:

    uint16_t voltage_mV;
    const uint16_t tolerance_mV;

    const BatteryLevel levels[5];

    BatteryStatus status;

    OnChange onChange = nullptr;
};