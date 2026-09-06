#pragma once

#include <Wire.h>
#include "sensor.h"
#include "timer.h"


struct THSample {
  float temp_c;
  float humid_rel;
};


class SHT21Sensor : public Sensor<THSample>
{
  public:

    SHT21Sensor(
      const char* name = "temp",
      uint8_t     addr = 0x40,
      uint32_t    samples_per_hour = 240,
      uint32_t    offset_ms = 0
    ) :
      Sensor(name),
      i2c_addr(addr),
      timer(3600000 / samples_per_hour, true, offset_ms)
    {}

    float temperature() { return reading.temp_c;    }
    float humidity()    { return reading.humid_rel; }

    void init() {
      reading = read();
    }

    void update(uint32_t ms) {
      if (timer.update(ms)) {
        reading = read();
        Sensor::push(reading);

        Serial.printf(PSTR("Temp %s %.1f°C %.1f%%rH\n"), sensorName(), reading.temp_c, reading.humid_rel);
      }
    }

    THSample read() {
      THSample s;
      s.temp_c = readT();
      s.humid_rel = readH();
      return s;
    }

  private:


    float readT() {
      // this can prbly be found in the datasheet, but i got it from here https://github.com/elechouse/SHT21_Arduino
      return (-46.85 + 175.72 / 65536.0 * (float)(readRaw(hold_master ? 0xE3 : 0xF3))); 
    }

    float readH() {
      // this can prbly be found in the datasheet, but i got it from here https://github.com/elechouse/SHT21_Arduino
      return (-6.0 + 125.0 / 65536.0 * (float)(readRaw(hold_master ? 0xE5 : 0xF5)));
    }

  
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

    const uint8_t  i2c_addr;
    Timer timer;
    
    const bool hold_master = true; // block i2c while taking reading (?)

    THSample reading;
};