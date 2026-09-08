#pragma once

#include <Arduino.h>
#include <type_traits>
#include <utility>
#include <CircularBuffer.hpp>
#include <time.h>
#include <array>
#include <optional>


// magic
template<typename T, typename = void>
struct has_toString : std::false_type {};
template<typename T>
struct has_toString<T, std::void_t<decltype(std::declval<T>().toString())>>
    : std::true_type {};
    

template<typename sample_type>
struct Sample {
  uint32_t time;
  sample_type val;

  String toString() const {
      return String(time) + " " + valToString();
  }

  private:
    String valToString() const {
      if constexpr (has_toString<sample_type>::value)
        return val.toString();
      else if constexpr (std::is_same_v<sample_type, bool>)
        return val ? "true" : "false";
      else if constexpr (std::is_floating_point_v<sample_type>)
        return String(val, 3); // 2 decimal places
      else
        return String(val);
    }
};


class SensorBase
{
  public:
    using LineEmitter = std::function<void(const String&)>;
    
    explicit SensorBase(const char* name) : sensorName(name) {}
    virtual ~SensorBase() = default;

    const char* name() const { return sensorName; }

    virtual void emit(const LineEmitter& emit) const = 0;

  private:
    const char* sensorName;
};


#define for_each_sensor(sens) \
  for (auto sens : registry) \
    if (sens) \


class Sensors
{
  public:

    template<typename T>
    void add(T& sensor) {
      for (auto sens : registry) {
        if (!sens) {
          sens = &sensor;
          break;
        }
      }
    }

    bool has(String name) {
      for_each_sensor(sensor)
        if (strcmp(sensor->name(), name.c_str()) == 0)
          return true;
    }

    SensorBase* get(String name) {
      for_each_sensor(sensor)
        if (strcmp(sensor->name(), name.c_str()) == 0)
          return sensor;
    }

  private:

    std::array<SensorBase*, 10> registry;
};

inline Sensors sensors;


template<typename sample_type, size_t size=256>
class Sensor : public SensorBase
{
  public:

    Sensor (const char* name) : SensorBase(name) {
      sensors.add(*this);
    }

    typedef void (*OnSample)(const sample_type& val);

    void setOnSample(OnSample cb) {
      onSample = cb;
    }

    const sample_type& lastSample() const {
      return buffer.last().val;
    }

    uint16_t sampleCount() const {
      return buffer.size();
    }

    void push(const sample_type& value) {
      time_t now;
      time(&now);

      Sample<sample_type> s{ static_cast<uint32_t>(now), value };
      buffer.push(s);

      if (onSample)
        onSample(value);
    }

    void emit(const LineEmitter& emitter) const override {
      for (size_t i = 0; i < buffer.size(); ++i) {
        emitter(buffer[i].toString());
      }
    }

  protected:
    CircularBuffer<Sample<sample_type>, size> buffer;

  private:
    OnSample onSample = nullptr;
};