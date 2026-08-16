#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

class Config 
{
  public:

    Config() : filename("/config.json") {}

    JsonDocument& get() { return config; }
    

    void load() {
      if (!SPIFFS.exists(filename)) {
        Serial.printf("Config file %s not found, restoring to defaults.\n", filename);
        savePreset();
      }

      File file = SPIFFS.open(filename, "r");
      auto error = deserializeJson(config, file);

      if (error) {
        file.close();
        Serial.printf("Could not parse config file %s, restoring to defaults.\n", filename);
        savePreset();
        deserializeJson(config, preset);
      }

      file.close();

    }

    void save() {
      File file = SPIFFS.open(filename, "w");
      String data;
      serializeJson(config, data);
      file.write(data.c_str(), data.length());
      file.close();
    }

    void savePreset() {
      File file = SPIFFS.open(filename, "w");
      file.write(preset, strlen(preset));
    }

  private:

    const char* filename;
    JsonDocument config;

    const char* preset = R"({
      "pump_a" : { 
          "prog_1" : [ 
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
      })";
};

inline Config config;
