#pragma once

#include <Wire.h>
#include "sensor.h"


struct THSample {
  float temp_c;
  float humid_rel;
};

class TempSensor : Sensor<THSample>
{
  public:

    TempSensor(
      const char* name = "temp",
      uint8_t     addr = 0x40,
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0)
    : 
      Sensor(name),
      sample_ms(3600000 / samples_per_hour),
      sample_offset_ms(offset_ms),
      i2c_addr(addr)
    {}

    void init() {
    }

    void update(int ms) {
      static uint32_t last = sample_offset_ms;
      last += ms;

      if (last >= sample_ms) {
        last -= sample_ms * (last / sample_ms);

        // TODO? handle errors
        THSample s;
        s.temp_c = readTemp();
        s.humid_rel = readHumid();
        Sensor::push(s);

        Serial.printf("Temp %s %.1f°C %.1f%%rH\n", name, s.temp_c, s.humid_rel);
      }
    }

    float readTemp(void) {
      // this can prbly be found in the datasheet, but i got it from here https://github.com/elechouse/SHT21_Arduino
      return (-46.85 + 175.72 / 65536.0 * (float)(readRaw(hold_master ? 0xE3 : 0xF3))); 
    }

    float readHumid(void) {
      // this can prbly be found in the datasheet, but i got it from here https://github.com/elechouse/SHT21_Arduino
      return (-6.0 + 125.0 / 65536.0 * (float)(readRaw(hold_master ? 0xE5 : 0xF5)));
    }

  private:
  
    uint16_t readRaw(uint8_t command)
    {
        uint16_t result;

        Wire.beginTransmission(i2c_addr);
        Wire.write(command);
        delay(100);
        Wire.endTransmission();

        Wire.requestFrom(i2c_addr, (uint8_t)3);
        while(Wire.available() < 3) {
          ; // wait // hmmmmmmm r u sure
        }

        result = ((Wire.read()) << 8);
        result += Wire.read();
        result &= ~0x0003;   // clear two low bits (status bits)
        return result;
    }

    const uint32_t sample_ms;
    const uint32_t sample_offset_ms;
    const uint8_t  i2c_addr;
    
    const bool hold_master = true; // block i2c while taking reading (?)
};