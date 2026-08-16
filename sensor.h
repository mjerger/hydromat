#pragma once

#include <Arduino.h>
#include <CircularBuffer.hpp>
#include <time.h>

template<typename sample_type, size_t size=256>
class Sensor
{
  public:

    Sensor (const char* name) : name(name) {}

    struct Sample {
      uint32_t time;
      sample_type val;
    };

    void push(sample_type value) {
      time_t now;
      time(&now);

      buffer.push({now, value});
    }

  protected:
  
    const char* name;
    CircularBuffer<Sample, size> buffer;
};