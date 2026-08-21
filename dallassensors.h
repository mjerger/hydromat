#pragma once

#include <OneWire.h>
#include <DallasTemperature.h>
#include "sensor.h"

class DallasSensor : public Sensor<float> 
{
  public:
    DallasSensor(const char* name, const DeviceAddress da) : Sensor(name) {
      std::copy(da, da + 8, addr);
    }

    DeviceAddress addr;
};


template<uint8_t PIN, size_t MAX_COUNT>
class DallasSensors
{
  public:
    DallasSensors(
      const char* name = "dallas",
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0)
    : 
      sample_ms(3600000 / samples_per_hour),
      sample_offset_ms(offset_ms),
      dallas(&wire)
    { }

    void init() {
      dallas.begin();

      Serial.printf("Found %d dallas devices\n ", dallas.getDeviceCount());
      
      for (int i=0; i< min(MAX_COUNT, dallas.getDeviceCount()); i++) {
        DeviceAddress addr;
        if (dallas.getAddress(addr, i)) {
          for (auto& sensor : sensors) 
            if (!sensor) {
              const char* name = (String("dallas_") + String(i)).c_str();
              
              // add to our list
              sensor.emplace(name, addr);

              Serial.printf("Added dallas sensor %s addr ", name);
              for (uint8_t i = 0; i < 8; i++) 
                Serial.printf("%02x", addr[i]);
              Serial.println();

              // try to set 9 bit res
              dallas.setResolution(addr, 9);
            }
        }
      }
    }

    void update(int ms) {
      static uint32_t last = sample_offset_ms;
      last += ms;

      if (last >= sample_ms) {
        last -= sample_ms * (last / sample_ms);

        // read all
        dallas.requestTemperatures();
        
        for (auto& sensor : sensors)
          if (sensor) {
            const float temp = dallas.getTempC(sensor.value().addr);
            if (temp == DEVICE_DISCONNECTED_C)
              Serial.printf("Dallas sensor %s did not respond\n", sensor.value().getName());
            else if (temp == DEVICE_FAULT_OPEN_C)
              Serial.printf("Dallas sensor %s: error fault open\n", sensor.value().getName());
            else if (temp == DEVICE_FAULT_SHORTGND_C)
              Serial.printf("Dallas sensor %s: error fault short ground\n", sensor.value().getName());
            else if (temp == DEVICE_FAULT_SHORTVDD_C)
              Serial.printf("Dallas sensor %s: error fault short vdd\n", sensor.value().getName());
            else if (temp == DEVICE_POWER_ON_RESET_C)
              Serial.printf("Dallas sensor %s: error power on reset\n", sensor.value().getName());
            else if (temp == DEVICE_INSUFFICIENT_POWER_C)
              Serial.printf("Dallas sensor %s: error insufficient power\n", sensor.value().getName());
            else
              sensor.value().push(temp);
          }
      }
    }

  private:

    const uint32_t sample_ms;
    const uint32_t sample_offset_ms;

    OneWire wire;
    DallasTemperature dallas;

    std::array<std::optional<DallasSensor>, MAX_COUNT> sensors;
};