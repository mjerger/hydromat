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

    void update(uint32_t ms) {
      if (remaining_ms >= ms) {
        remaining_ms -= ms;
      } else if (remaining_ms > 0) {
        setPower(0);

        const uint32_t runtime_ms = duration_ms-remaining_ms;
        remaining_ms = 0;

        Serial.printf(PSTR("Turn off %s after %dms\n"), name, runtime_ms);
      }
    }

    void turnOnFor(int seconds, uint8_t pwr) {
      const uint8_t p = pwr ? pwr < max_power ? pwr : max_power : max_power;
      setPower(p);

      duration_ms = seconds * 1000;
      remaining_ms = duration_ms;

      Serial.printf(PSTR("Turn on %s for %ds at %d power\n"), name, seconds, p);
    }

    void turnOff() {
      remaining_ms = 0;
      setPower(0);
    }

    void setPower(uint8_t pwr) {
      power = pwr > max_power ? max_power : pwr;;
      analogWrite(pin, power);
      sensor.push(pwr);
      if (onChange)
        onChange(*this, power);
    }

  private:
    const char* name;
    const uint8_t pin;
    const uint8_t max_power;

    const OnChange onChange;
    
    uint8_t power;
    uint32_t remaining_ms;
    uint32_t duration_ms;

    Sensor<uint8_t> sensor;
};

// hmmmm
#define for_each(dev) \
  for (auto& device : devices) \
    if (device) \
      if (auto& dev = device.value(); true)

class Pumps;
extern Pumps pumps;


struct Program {
  CronId id = dtINVALID_ALARM_ID;
  uint32_t duration;
  uint8_t power;
};


struct Device : Pump
{
  Device(
    const char* id, 
    const char* name, 
    uint8_t pin,
    OnChange cb) 
  : 
    Pump(name, pin, cb), 
    id(id)
  {}
  
  const char* id;

  Program programs[2][10];
};


// handles the pumps
class Pumps
{
  public:
    Pumps() : mode(PUMP_OFF) {}

    void init() {
      for_each(pump) {
        // load from config and start the crons
        for (int p=0; p<2; p++) {
          String prog = F("prog_") + String(p+1);
          JsonArray arr = config.get()[pump.id][prog].as<JsonArray>();
          for (int i=0; i<arr.size(); i++) {
            String str = arr[i].as<String>();

            int cpos = str.lastIndexOf("*");
            int spos = str.lastIndexOf("s");
            int ppos = str.lastIndexOf("%");

            String cstr = str.substring(0, cpos+1);
            int seconds = str.substring(cpos+1, spos).toInt();
            int power   = str.substring(spos+1, ppos).toInt();

            auto& prog = pump.programs[p][i];
            prog.id = Cron.create(cstr.begin(), onCronTriggered, false);
            prog.duration = seconds;
            prog.power = (255*(uint32_t)power)/100;

            Serial.printf(PSTR("Add cron #%d for %s program %d: '%s' %ds %d%%\n"), prog.id, pump.getName(), p+1, cstr.c_str(), seconds, power);
          }
        }
      }
    }

    void update(uint32_t ms) {
      for_each(pump)
        pump.update(ms);
    }

    // add a pump (call before init)
    void add(const char* id, const char* name, uint8_t pin, Pump::OnChange cb) {
      for (auto& device : devices) {
        if (!device) {
          device.emplace(id, name, pin, cb);
          break;
        }
      }
    }

    // is any pump running?
    const bool isAllOff() {
      for_each(pump)
        if (pump.getPower())
          return false;
      return true;
    }

    // set mode of all pumps
    void setMode(Mode m) {
      mode = m;
      Serial.printf(PSTR("Pumps set mode %d\n"), m);

      for_each(pump) {
        switch (mode) {
          case PUMP_ON:
          case PUMP_OFF:
            if (mode) 
              pump.turnOnFor(0, 0xff);
            else
              pump.turnOff();

            // disable all crons
            for (int p=0; p<2; p++)
              for (int i=0; i<10; i++)
                Cron.disable(pump.programs[p][i].id);
            break;

          case PROGRAM_1:
            pump.turnOff();
            for (int i=0; i<10; i++) { 
              Cron.enable (pump.programs[0][i].id);
              Cron.disable(pump.programs[1][i].id);
            }
            break;

          case PROGRAM_2:
            pump.turnOff();
            for (int i=0; i<10; i++) { 
              Cron.disable(pump.programs[0][i].id);
              Cron.enable (pump.programs[1][i].id);
            }
            break;
        }
      }
    }

    // set power of all pumps
    void turnOnFor(int seconds, uint8_t pwr) {
      for_each(pump)
        pump.turnOnFor(seconds, pwr);
    }

    void turnOff() {
      for_each(pump) 
        pump.turnOff();
    }

    static void onCronTriggered() { 
      pumps.onCronTriggeredImpl();
    }
    
  private:

    Mode mode;
    std::array<std::optional<Device>, 2> devices;

    void onCronTriggeredImpl() {

      // ignore unless programs are enabled
      if (mode < PROGRAM_1 || mode > PROGRAM_2)
        return;

      const uint8_t program = mode - PROGRAM_1;
      const CronId id = Cron.getTriggeredCronId();
      Serial.printf(PSTR("Cron #%d triggered "), id);
      
      // find the corresponding pump and settings
      for_each(pump) {
        for (int i=0; i<10; i++) {
          auto& prog = pump.programs[program][i];
          if (prog.id == id) {
            Serial.printf("pump %s\n",pump.id);
            pump.turnOnFor(prog.duration, prog.power);
            return;
          }
        }
      }
      Serial.println("but was ignored ??");
    }
};

inline Pumps pumps;
