#pragma once

#include <OneWire.h>
#include <DallasTemperature.h>
#include "sensor.h"
#include "timer.h"


class DallasSensor : public Sensor<float> 
{
  public:
    DallasSensor(
      const String& name,
      uint8_t id,
      const DeviceAddress da)
    : 
     id(id),
     Sensor(strdup(name.c_str())) // copy string but no need to dealloc
    {
      std::copy(da, da + 8, addr);
    }

    uint8_t id;
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
      name(name),
      timer(3600000 / samples_per_hour, true, offset_ms),
      wire(PIN),
      dallas(&wire)
    { }

    void init() {
      dallas.begin();

      auto numFound = dallas.getDeviceCount();
      Serial.printf(PSTR("Found %d dallas devices\n"), numFound);
      
      for (int i=0; i < min(MAX_COUNT, numFound); i++) {
        DeviceAddress addr;
        if (dallas.getAddress(addr, i)) {
          for (auto& sensor : sensors) 
            if (!sensor) {

              // simple id from addr
              uint8_t id = 0;
              for (uint8_t b=0; b<8; b++)
                id ^= addr[b];

              String nameStr = String(name) + String("_") + String(id);

              // create sensor
              sensor.emplace(nameStr, id, addr);

              Serial.printf(PSTR("Added dallas sensor %s id %d addr "), name, id);
              for (uint8_t i = 0; i < 8; i++) 
                Serial.printf("%02x", addr[i]);
              Serial.println();

              // try to set 9 bit res
              dallas.setResolution(addr, 9);
              break;
            }
        }
      }

      if (!numFound)
        timer.stop();
    }

    void update(uint32_t ms) {
      if (timer.update(ms)) {

        // read all
        dallas.requestTemperatures();
        
        for (auto& sensor : sensors) {
          if (sensor) {
            const float temp = dallas.getTempC(sensor.value().addr);
            const char* name = sensor.value().sensorName();

            if (temp == DEVICE_DISCONNECTED_C)
              Serial.printf(PSTR("Dallas sensor %s did not respond\n"), name);
            else if (temp == DEVICE_FAULT_OPEN_C)
              Serial.printf(PSTR("Dallas sensor %s: error fault open\n"), name);
            else if (temp == DEVICE_FAULT_SHORTGND_C)
              Serial.printf(PSTR("Dallas sensor %s: error fault short ground\n"), name);
            else if (temp == DEVICE_FAULT_SHORTVDD_C)
              Serial.printf(PSTR("Dallas sensor %s: error fault short vdd\n"), name);
            else if (temp == DEVICE_POWER_ON_RESET_C)
              Serial.printf(PSTR("Dallas sensor %s: error power on reset\n"), name);
            else if (temp == DEVICE_INSUFFICIENT_POWER_C)
              Serial.printf(PSTR("Dallas sensor %s: error insufficient power\n"), name);
            else {
              sensor.value().push(temp);
              Serial.printf(PSTR("Temp %s %.01f°C\n"), name, temp);
            }
          }
        }
      }
    }

  private:

    const char* name;
    Timer timer;

    OneWire wire;
    DallasTemperature dallas;

    std::array<std::optional<DallasSensor>, MAX_COUNT> sensors;
};