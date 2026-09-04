#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>


static const char configPreset[] PROGMEM = R"(
{
  "pump_a" : {
      "prog_1" : [
          "*    *             * * * 30s 100%",
          "0    10-15         * * * 30s 100%",
          "*/30 16-20         * * * 42s 100%",
          "0    21-23/2,0-9/2 * * * 30s 100%"
      ],
      "prog_2" : [ "*/10 * * * * 42s 100%" ]
  },
  "pump_b" : {
      "prog_1" : [
          "1 10-20         * * * 23s 100%",
          "1 21-23/2,0-9/2 * * * 23s 100%"
      ],
      "prog_2" : [ "*/10 * * * * 42s 100%" ]
  }
}
)";


class Config 
{
  public:

    Config() : filename("/config.json") {}

    JsonDocument& get() { return doc; }

    void load() {
      if (!SPIFFS.exists(filename)) {
        Serial.printf(PSTR("Config file %s not found\n"), filename);
        savePreset();
      }

      File file = SPIFFS.open(filename, "r");
      ReadBufferingStream bs(file, 64);

      auto error = deserializeJson(doc, bs);
      if (error) {
        Serial.printf(PSTR("Could not parse config file %s: %s\n"), filename, error.c_str());
        file.close();
        savePreset();
        deserializeJson(doc, configPreset);
      }

      file.close();
    }

    void save() {
      File file = SPIFFS.open(filename, "w");
      String data;
      serializeJson(doc, data);
      file.write(data.c_str(), data.length());
      file.close();
    }

  private:
    
    void savePreset() {
      Serial.printf(PSTR("Restoring preset config to %s\n"), filename);
      File file = SPIFFS.open(filename, "w");
      size_t len = strlen_P(configPreset);
      const size_t bsize = 128;
      char buffer[bsize];
      
      for (size_t i = 0; i < len; i += bsize) {
        size_t chunk = (len - i) > bsize ? bsize : (len - i);
        memcpy_P(buffer, configPreset + i, chunk);
        file.write((const uint8_t*)buffer, chunk);
      }
      file.close();
    }

    const char* filename;
    JsonDocument doc;
};

inline Config config;
