#pragma once

#include <Arduino.h>
#include <CircularBuffer.hpp>
#include <time.h>

template<typename sample_type, size_t size=256>
class Sensor
{
  public:

    struct Sample {
      uint32_t time;
      sample_type val;
    };
    
    Sensor (const char* name) : name(name) {}

    typedef void (*OnSample)(const sample_type& val);

    void setOnSample(OnSample cb) {
      onSample = cb;
    }

    const char* getName() { 
      return name; 
    }

    void push(const sample_type& value) {
      time_t now;
      time(&now);

      const Sample& s = {now, value};
      buffer.push(s);

      if (onSample)
        onSample(value);
    }
    
  protected:
  
    const char* name;
    CircularBuffer<Sample, size> buffer;

    OnSample onSample = nullptr;
};