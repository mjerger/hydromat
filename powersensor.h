#pragma once

#include <Wire.h>
#include <INA219_WE.h>
#include "sensor.h"


struct PowerSample {
  int bus_mV;
  int bus_mA;
  int bus_mW;
  int load_mV;
};

class PowerSensor : Sensor<PowerSample>
{
  public:

    PowerSensor(
      const char* name = "power",
      uint8_t     addr = 0x40,
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0)
    : 
      Sensor(name),
      sample_ms(3600000 / samples_per_hour),
      sample_offset_ms(offset_ms),
      i2c_addr(addr),
      ina219(INA219_WE(addr))
    {}

    void init() {
      ina219.setADCMode(INA219_SAMPLE_MODE_16);  // 16 sample average
      ina219.setMeasureMode(INA219_CONTINUOUS); 
      ina219.setBusRange(INA219_BRNG_32);

      if (!ina219.init()) {
        Serial.println("Failed to init ina219 current sensor on i2c address " + String(i2c_addr, 16));
      }
    }

    void update(int ms) {
      static uint32_t last = sample_offset_ms;
      last += ms;

      if (last >= sample_ms) {
        last -= sample_ms * (last / sample_ms);

        PowerSample s;
        s.bus_mV  = ina219.getBusVoltage_V() * 1000.0;
        s.bus_mA  = ina219.getCurrent_mA();
        s.bus_mW  = ina219.getBusPower();
        s.load_mV = ina219.getBusVoltage_V() * 1000.0 + ina219.getShuntVoltage_mV();

        Sensor::push(s);

        Serial.printf("Power %s %dmV, %dmA, %dmW, %dmV\n", name, s.bus_mV, s.bus_mA, s.bus_mW, s.load_mV);
      }
    }

  private:

    const uint32_t sample_ms;
    const uint32_t sample_offset_ms;
    const uint8_t  i2c_addr;
    
    INA219_WE ina219;
};