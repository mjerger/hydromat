#pragma once

#include <Arduino.h>
#include <functional>
#include <array>
#include <optional>
#include <CronAlarms.h>
#include "config.h"
#include "sensor.h"


enum Mode
{
  PUMP_OFF = 0,
  PROGRAM_1,
  PROGRAM_2,
  PUMP_ON,
};


class Pump
{
  public:
    typedef void (*OnChange)(Pump&, uint8_t power);

    Pump(const char* name, uint8_t pin, OnChange cb) :
      name(name), 
      pin(pin), 
      max_power(200),
      onChange(cb),
      sensor(name)
    {}

    const char* getName()   { return name; }
    uint8_t     getPower()  { return power; }
    const auto& getSensor() { return sensor; }

    void init() {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, 0);
    }

    void update(int ms) {
      if (duration_ms >= ms) {
        duration_ms -= ms;
      } else if (duration_ms) {
        duration_ms = 0;
        setPower(0);
      }
    }

    void turnOnFor(int seconds, uint8_t pwr) {
      duration_ms = seconds * 1000;
      setPower(pwr ? pwr < max_power ? pwr : max_power : max_power);
    }

    void turnOff() {
      duration_ms = 0;
      setPower(0);
    }

    void setPower(uint8_t pwr) {
      power = pwr > max_power ? max_power : pwr;;
      analogWrite(pin, power);
      sensor.push(pwr);
      onChange(*this, power);
    }

  private:
    const char* name;
    const uint8_t pin;
    const uint8_t max_power;

    const OnChange onChange;
    
    uint8_t power;
    uint32_t duration_ms;

    Sensor<uint8_t> sensor;
};


// handles the pumps
class Pumps
{
  public:
    Pumps() : mode(PUMP_OFF) {}

    typedef void (*OnChange)(Pump&, uint8_t power);

    void setOnChange(OnChange cb) {
      onChange = cb;
    }

    void init() {

      for (auto& device : devices) {
        if (!device) continue;

        auto& dev = device.value();
        
        // load from config and start the crons
        for (int p=0; p<2; p++) {
          String prog = String("prog_") + String(p);
          JsonArray arr = config.get()[dev.id][prog].as<JsonArray>();
          for (int i=0; i<arr.size(); i++) {
            String str = arr[i].as<String>();

            int cpos = str.lastIndexOf("*");
            int spos = str.lastIndexOf("s");
            int ppos = str.lastIndexOf("%");

            String cstr = str.substring(0, cpos);
            int seconds = str.substring(cpos, spos-1).toInt();
            int power   = str.substring(spos, ppos-1).toInt();

            dev.programs[p][i] = Cron.create(
              cstr.begin(), 
              onCronTriggered,
              false
            );

          }
        }
      }
    }

    void update(int ms) {
      for (auto& device : devices)
        if (device)
          device.value().pump.update(ms);
    }

    // add a pump (call before init)
    void add(const char* id, const char* name, uint8_t pin) {
      for (auto& device : devices) {
        if (!device) {
          device.emplace(id, name, pin, onChange);
          break;
        }
      }
    }

    // is any pump running?
    const bool isAllOff() {
      for (auto& device : devices) {
        if (device)
          if (device.value().pump.getPower())
            return false;
      }
      return true;
    }

    // set mode of all pumps
    void setMode(Mode m) {
      mode = m;

      for (auto& device : devices) {
        if (!device) continue;
        auto& dev = device.value();
          
        switch (mode) {
          case PUMP_ON:
          case PUMP_OFF:
            if (mode) 
              turnOnFor(0, 0xff);
            else
              turnOff();

            // disable all crons
            for (int p=0; p<2; p++)
              for (int i=0; i<10; i++)
                Cron.disable(dev.programs[p][i]);
            break;

          case PROGRAM_1:
            for (int i=0; i<10; i++) { 
              Cron.enable (dev.programs[1][i]);
              Cron.disable(dev.programs[2][i]);
            }
            break;

          case PROGRAM_2:
            for (int i=0; i<10; i++) { 
              Cron.disable(dev.programs[1][i]);
              Cron.enable (dev.programs[2][i]);
            }
            break;
        }
      }
    }

    // set power of all pumps
    void turnOnFor(int seconds, uint8_t pwr) {
      for (auto& device : devices) {
        if (!device) continue;
        auto& pump = device.value().pump;
        pump.turnOnFor(seconds, pwr);
      }
    }

    void turnOff() {
      for (auto& device : devices) {
        if (!device) continue;
        auto& pump = device.value().pump;
        pump.turnOff();
      }
    }
    
  private:

    Mode mode;
    OnChange onChange = nullptr;

    struct Device
    {
      Device(
        const char* id, 
        const char* name, 
        uint8_t pin,
        OnChange cb) 
      : 
        pump(name, pin, cb), 
        id(id)
      {}

      Pump pump;
      const char* id;
      CronId programs[2][10];
    };

    std::array<std::optional<Device>, 2> devices;

    static void onCronTriggered() {

    }
  };


inline Pumps pumps;
