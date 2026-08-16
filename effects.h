#pragma once

#include <functional>

using EFunc = std::function<CRGB(int i, int t)>;

struct Effect {
  EFunc    func;
  uint32_t t = 0;
};

namespace Effects
{
  const CRGB ON  = CRGB::White;
  const CRGB OFF = CRGB::Black;
  const CRGB PlasmaPurple = CHSV(195, 255, 255);

  const CRGB fract(float f) {
    return ON % (uint8_t)round(f * 255.0f);
  }


  EFunc off = [](int i, int t) {
    return OFF;
  };


  EFunc on = [](int i, int t) {
    return ON;
  };


  EFunc onFor(int ms) {
    return [ms] (int i, int t) -> CRGB {
      if (t > ms)
        return OFF;
      return ON;
    };
  };


  EFunc blink(int on_ms, int off_ms) {
    return [on_ms, off_ms] (int i, int t) -> CRGB {
      if (t % (on_ms+off_ms) > on_ms)
        return OFF;
      return ON;
    };
  };


  EFunc pulse(int ms, float min = 0, float phi = 0) {
    return [ms, min, phi] (int i, int t) -> CRGB {
      float phase = (float)(t % ms) / ms + i * phi;
      float f = (sin(phase * TWO_PI) + 1.0f) * 0.5;
      return fract(min + (f * (1.0f - min)));
    };
  };


  EFunc fadeIn(int ms) {
    return [ms] (int i, int t) -> CRGB {
      if (t >= ms)
        return ON;
      return fract((float)t / (float)ms);
    };
  };


  EFunc rainbow(int ms, int len) {
    return [ms, len] (int i, int t) -> CRGB {
       float phase = (float)(t % ms) / (float)ms + (float)i/(float)len;
       return CHSV((int)(255*phase)%255,255,255);
    };
  };


  // deterministic fast hash — same input always gives same output
  static inline uint32_t hash(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
  }

  EFunc glitchNeon(int numLeds = 26) {
    const uint32_t eventPeriod = 4200;        // ms between possible dropout events
    const uint32_t eventLen = 600;            // how long a dropout event lasts
    const uint32_t flickerSlot = 60;          // ms per flicker subframe within an event
    const uint32_t segLenMin = 2;
    const uint32_t segLenMaxFract=2;

    return [numLeds](int i, int t) -> CRGB {
      uint32_t ut = (uint32_t)t;

      uint32_t eventIdx = ut / eventPeriod;
      uint32_t localT = ut % eventPeriod;

      if (localT < eventLen) {
        uint32_t seed = hash(eventIdx * 0x9E3779B1u);

        // window actually get a dropout
        if ((seed & 0xFF) < 100) {
          int segStart = seed % numLeds;
          int segLen   = segLenMin + ((seed >> 8) % (numLeds/segLenMaxFract-segLenMin)); 
          int rel = (i - segStart + numLeds) % numLeds;

          if (rel < segLen) {
            // stutter: several flicker slots within the event, each independently on/off
            uint32_t slot = localT / flickerSlot;
            uint32_t flickerSeed = hash(seed ^ (slot * 0xB5297A4Du));
            if ((flickerSeed & 0xFF) < 180) { // mostly off during the event
              return Effects::OFF;
            }
          }
        }
      }

      return Effects::ON;
    };
  }

  EFunc glitch(int numLeds = 26) {
    const uint32_t eventPeriod = 4200;        // ms between possible dropout events
    const uint32_t eventLen = 600;            // how long a dropout event lasts
    const uint32_t flickerSlot = 60;          // ms per flicker subframe within an event

    // startup behavior
    const uint32_t startupDuration = 10000;    // ms - startup glitching fades out over this window
    const uint32_t startupEventPeriod = 350;  // much faster event cycling during startup
    const uint32_t startupEventLen = 220;     // most of each cycle is "active"
    const uint32_t startupFlickerSlot = 40;   // faster stutter
    const uint8_t startupOffChanceMax = 220;  // near-total blackout right at t=0

    return [numLeds](int i, int t) -> CRGB {
      uint32_t ut = (uint32_t)t;
      bool off = false;

      // --- normal steady-state dropout layer ---
      {
        uint32_t eventIdx = ut / eventPeriod;
        uint32_t localT = ut % eventPeriod;

        if (localT < eventLen) {
          uint32_t seed = hash(eventIdx * 0x9E3779B1u);
          if ((seed & 0xFF) < 100) {
            int segStart = seed % numLeds;
            int segLen   = 2 + ((seed >> 8) % (numLeds / 2 - 2));
            int rel = (i - segStart + numLeds) % numLeds;
            if (rel < segLen) {
              uint32_t slot = localT / flickerSlot;
              uint32_t flickerSeed = hash(seed ^ (slot * 0xB5297A4Du));
              if ((flickerSeed & 0xFF) < 180) {
                off = true;
              }
            }
          }
        }
      }

      // --- startup surge layer: only active for the first startupDuration ms ---
      if (ut < startupDuration) {
        // 1.0 at t=0, fading to 0.0 at startupDuration
        float progress = 1.0f - ((float)ut / (float)startupDuration);

        uint32_t eventIdx = ut / startupEventPeriod;
        uint32_t localT = ut % startupEventPeriod;

        if (localT < startupEventLen) {
          uint32_t seed = hash(eventIdx * 0x85EBCA6Bu + 0x1234567u);
          // dropout chance itself fades with progress - almost guaranteed early, rare late
          uint8_t dropoutChance = (uint8_t)(progress * 230.0f);
          if ((seed & 0xFF) < dropoutChance) {
            int segStart = seed % numLeds;
            int segLen   = 6 + ((seed >> 8) % (numLeds - 8));
            int rel = (i - segStart + numLeds) % numLeds;
            if (rel < segLen) {
              uint32_t slot = localT / startupFlickerSlot;
              uint32_t flickerSeed = hash(seed ^ (slot * 0xC2B2AE3Du));
              uint8_t offChance = (uint8_t)(progress * startupOffChanceMax);
              if ((flickerSeed & 0xFF) < offChance) {
                off = true;
              }
            }
          }
        }
      }

      float fadeProgress = (ut < startupDuration) ? (float)ut / startupDuration : 1.0f;
      uint8_t fadeInBrightness = applyGamma_video((uint8_t)(55 + fadeProgress * 200), 2.8f);
      return off ? Effects::OFF : Effects::ON % fadeInBrightness;
    };
  }
};