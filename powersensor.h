#pragma once

#include <Wire.h>
#include <INA219_WE.h>
#include "sensor.h"
#include "timer.h"
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
      timer(3600000 / samples_per_hour, true, offset_ms),
      ina219(INA219_WE(addr))
    {}

    float busVoltage()     { return (float)power.bus_mV  / 1000.0f; }
    float busCurrent()     { return (float)power.bus_mA  / 1000.0f; }
    float busPower()       { return (float)power.bus_mW  / 1000.0f; }
    float loadVoltage()    { return (float)power.load_mV / 1000.0f; }
    float batteryVoltage() { return (float)power.batt_mV / 1000.0f; }

    void init() {
      ina219.setADCMode(INA219_SAMPLE_MODE_16);  // 16 sample average
      ina219.setMeasureMode(INA219_CONTINUOUS); 
      ina219.setBusRange(INA219_BRNG_32);

      if (ina219.init()) {
        timer.start();
      } else {
        Serial.printf(PSTR("Failed to init ina219 current sensor on i2c address %s\n"), String(i2c_addr, 16).c_str());
      }
    }

    void update(uint32_t ms) {
      if (timer.update(ms)) {

        PowerSample s;
        s.bus_mV  = ina219.getBusVoltage_V() * 1000.0;
        s.bus_mA  = ina219.getCurrent_mA();
        s.bus_mW  = ina219.getBusPower();
        s.load_mV = ina219.getBusVoltage_V() * 1000.0 + ina219.getShuntVoltage_mV();
        s.batt_mV = s.bus_mV + calc1N5822ForwardVoltage(s.bus_mA);

        Sensor::push(s);
        power = s;

        Serial.printf(PSTR("Power %s bus %dmV, %dmA, %dmW load %dmV batt %dmV\n"), sensorName(), s.bus_mV, s.bus_mA, s.bus_mW, s.load_mV, s.batt_mV);
      }
    }


  private:

    const uint8_t  i2c_addr;
    Timer timer;

    INA219_WE ina219;

    PowerSample power;
};