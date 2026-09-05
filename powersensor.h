#pragma once

#include <Wire.h>
#include <INA219_WE.h>
#include "sensor.h"
#include "utils.h"


struct PowerSample {
  uint16_t bus_mV;
  uint16_t bus_mA;
  uint16_t bus_mW;
  uint16_t load_mV;
  uint16_t batt_mV;
};

class PowerSensor : public Sensor<PowerSample>
{
  public:

    PowerSensor (
      const char* name = "power",
      uint8_t     addr = 0x40,
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0
    ) : 
      Sensor(name),
      i2c_addr(addr),
      sample_ms(3600000 / samples_per_hour),
      sample_offset_ms(offset_ms),
      ina219(INA219_WE(addr))
    {}

    uint16_t getVoltage()        { return power.bus_mV; }
    uint16_t getCurrent()        { return power.bus_mA; }
    uint16_t getPower()          { return power.bus_mW; }
    uint16_t getLoadVoltage()    { return power.load_mV; }
    uint16_t getBatteryVoltage() { return power.batt_mV; }

    void init() {
      ina219.setADCMode(INA219_SAMPLE_MODE_16);  // 16 sample average
      ina219.setMeasureMode(INA219_CONTINUOUS); 
      ina219.setBusRange(INA219_BRNG_32);

      if (!ina219.init()) {
        Serial.printf(PSTR("Failed to init ina219 current sensor on i2c address %s\n"), String(i2c_addr, 16).c_str());
      }
    }

    void update(uint32_t ms) {
      static uint32_t last = sample_offset_ms;
      last += ms;

      if (last >= sample_ms) {
        last -= sample_ms * (last / sample_ms);

        PowerSample s;
        s.bus_mV  = ina219.getBusVoltage_V() * 1000.0;
        s.bus_mA  = ina219.getCurrent_mA();
        s.bus_mW  = ina219.getBusPower();
        s.load_mV = ina219.getBusVoltage_V() * 1000.0 + ina219.getShuntVoltage_mV();
        s.batt_mV = s.bus_mV + calc1N5822ForwardVoltage(s.bus_mA);

        Sensor::push(s);
        power = s;

        Serial.printf(PSTR("Power %s %dmV, %dmA, %dmW, %dmV\n"), name, s.bus_mV, s.bus_mA, s.bus_mW, s.load_mV);
      }
    }


  private:

    const uint8_t  i2c_addr;
    const uint32_t sample_ms;
    const uint32_t sample_offset_ms;

    INA219_WE ina219;

    PowerSample power;
};