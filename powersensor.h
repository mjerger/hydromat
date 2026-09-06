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
      uint32_t    update_ms = 1000,
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0
    ) : 
      Sensor(name),
      i2c_addr(addr),
      updateTimer(update_ms, true, offset_ms),
      sampleTimer(3600000 / samples_per_hour, true, offset_ms),
      ina219(INA219_WE(addr))
    {}

    typedef void (*OnUpdate)(const PowerSample& val);

    void setOnUpdate(OnUpdate cb) {
      onUpdate = cb;
    }

    float busVoltage()     { return (float)reading.bus_mV  / 1000.0f; }
    float busCurrent()     { return (float)reading.bus_mA  / 1000.0f; }
    float busPower()       { return (float)reading.bus_mW  / 1000.0f; }
    float loadVoltage()    { return (float)reading.load_mV / 1000.0f; }
    float batteryVoltage() { return (float)reading.batt_mV / 1000.0f; }

    void init() {
      ina219.setADCMode(INA219_SAMPLE_MODE_16);  // 16 sample average
      ina219.setMeasureMode(INA219_CONTINUOUS); 
      ina219.setBusRange(INA219_BRNG_32);

      if (ina219.init()) {
        updateTimer.start();
        sampleTimer.start();
      } else {
        Serial.printf(PSTR("Failed to init ina219 current sensor on i2c address %s\n"), String(i2c_addr, 16).c_str());
      }

      reading = read();
    }

    void update(uint32_t ms) {
      updateTimer.update(ms);
      sampleTimer.update(ms);

      if (updateTimer.ticked() || sampleTimer.ticked()) {

        reading = read();

        if (onUpdate)
          onUpdate(reading);

        if (sampleTimer.ticked()) {
          Sensor::push(reading);
          Serial.printf(PSTR("Power %s bus %dmV, %dmA, %dmW load %dmV batt %dmV\n"), 
            sensorName(), reading.bus_mV, reading.bus_mA, reading.bus_mW, reading.load_mV, reading.batt_mV);
        }
      }
    }

    PowerSample read() {
      PowerSample s;
      s.bus_mV  = ina219.getBusVoltage_V() * 1000.0;
      s.bus_mA  = ina219.getCurrent_mA();
      s.bus_mW  = ina219.getBusPower();
      s.load_mV = ina219.getBusVoltage_V() * 1000.0 + ina219.getShuntVoltage_mV();
      s.batt_mV = s.bus_mV + calc1N5822ForwardVoltage(s.bus_mA);
      return s;
    }

  private:

    const uint8_t  i2c_addr;
    Timer sampleTimer;
    Timer updateTimer;

    INA219_WE ina219;

    PowerSample reading;

    OnUpdate onUpdate = nullptr;
};