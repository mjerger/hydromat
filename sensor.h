#pragma once

#include <Arduino.h>
#include <CircularBuffer.hpp>
#include <time.h>

template<typename sample_type>
struct Sample {
  uint32_t time;
  sample_type val;
};
    
template<typename sample_type, size_t size=256>
class Sensor
{
  public:
    
    Sensor (const char* name) : name(name) {}

    typedef void (*OnSample)(const sample_type& val);

    void setOnSample(OnSample cb) {
      onSample = cb;
    }

    const char* getName() const { 
      return name; 
    }

    const sample_type& getLastSample() const {
      return buffer.last().val;
    }

    uint16_t getSampleCount() const {
      return buffer.size();
    }

    void push(const sample_type& value) {
      time_t now;
      time(&now);

      const Sample<sample_type>& s = {now, value};
      buffer.push(s);

      if (onSample)
        onSample(value);
    }
    
  protected:
  
    const char* name;
    CircularBuffer<Sample<sample_type>, size> buffer;

    OnSample onSample = nullptr;
};