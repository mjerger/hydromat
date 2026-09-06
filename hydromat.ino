/* 
 * Hydromat 2000
 *
 * Libraries used
 *   Arduino esp8266    3.1.2
 *   ArduinoJson        7.4.3   https://arduinojson.org/
 *   Uptime Library     1.0.0   https://github.com/YiannisBourkelis/Uptime-Library
 *   CronAlarms         0.1.0   https://github.com/Martin-Laclaustra/CronAlarms
 *   FastLED            3.10.5  https://github.com/fastled/fastled
 *   INA219_WE          1.4.1   https://github.com/wollewald/INA219_WE
 *   DallasTemperature  4.0.6   https://github.com/milesburton/Arduino-Temperature-Control-Library
 *   CircularBuffer     1.4.0   https://github.com/rlogiacco/CircularBuffer
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <uri/UriRegex.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <uptime.h>
#include <uptime_formatter.h>
#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"
#include <CronAlarms.h>
#include <Wire.h>

// custom config to make mix of different leds work
#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 1
#define FASTLED_WS2812_T1 250
#define FASTLED_WS2812_T2 330
#define FASTLED_WS2812_T3 650
#include <FastLED.h>

#include "utils.h"
#include "http.h"
#include "config.h"
#include "pumps.h"
#include "switch.h"
#include "lights.h"
#include "battery.h"
#include "powersensor.h"
#include "sht21sensor.h"
#include "waterlevelsensor.h"
#include "dallassensors.h"

#include "secrets.h"

#define SERIAL_ENABLED

const char *hostname = "hydromat";
const char *version = "0.3";

#define PIN_SCL        D1
#define PIN_SDA        D2
#define PIN_PUMP_A     D8
#define PIN_PUMP_B     D3
#define PIN_SWITCH_0   D5
#define PIN_SWITCH_1   D6
#define PIN_LEDS       D7
#define PIN_LEVEL      A0
#define PIN_DS_ONEWIRE D4

Lights<PIN_LEDS> lights;
Switch<PIN_SWITCH_0, PIN_SWITCH_1> swtch;
Battery battery;
PowerSensor powerSensor("main_power", 0x41, 360);       // i2c devices on the same wires
SHT21Sensor caseSensor ("case_temp",  0x40,  60, 5000); // samples 5 sec before power sensor
WaterLevelSensor<PIN_LEVEL> levelSensor("main_tank", 3600, 1000);
DallasSensors<PIN_DS_ONEWIRE, 2> dallasSensors("ext_temp", 240, 2000);

bool handleSysinfo() {
  JsonDocument json;

  json["uptime"] = uptime_formatter::getUptime();
  json["uptime_ms"] = millis();
  json["datetime"] = getCurrentTimeString(true);

  // Network
  json["hostname"] = WiFi.getHostname();
  json["mdns"] = String(hostname) + ".local";
  json["ssid"] = WiFi.SSID();
  json["ip"]   = WiFi.localIP().toString();
  json["rssi"] = WiFi.RSSI();

  // Filesystem
  FSInfo fs_info;
  SPIFFS.info(fs_info);

  json["fs_free"]  = String((double)(fs_info.totalBytes - fs_info.usedBytes) / 1024.0, 1) + "k";
  json["fs_used"]  = String((double)fs_info.usedBytes  / 1024.0, 1) + "k";
  json["fs_total"] = String((double)fs_info.totalBytes / 1024.0, 1) + "k";

  return http::sendJson(json);
}

void setupServer() {
  // routes
  http::server.on("/",                      HTTP_GET,   []() { http::handleFile("/home.html");      });
  http::server.on("/settings",              HTTP_GET,   []() { http::handleFile("/settings.html");  });
  //server.on("/config",                HTTP_ANY,   []() { handleConfig(server);  });
  http::server.on("/sysinfo",               HTTP_GET,   []() { handleSysinfo();  });
  //server.on(UriRegex("/tds/(.*)"),    HTTP_GET,   []() { handleTDS(server.pathArg(0));  });

  // we use our own routing hacky hack
  http::server.onNotFound([]() {
    Serial.print(http::methods[http::server.method()] + " -> " + http::server.uri());
    if (http::handleFile(http::server.uri())) {
      Serial.println(" ok");
    } else {
      Serial.println(" not found");
      http::server.send(404, "text/plain", "404: Not Found");
    }
  });

  // Start Server
  http::server.begin();

  Serial.println(PSTR("HTTP server started"));
}

// handle stupid unreliable wifi
void handleWiFiReconnect() {
  // start true so we can trigger the warning light correctly
  static bool connected = true;
  
  static const EFunc connectedEffect  = Effects::fadeOutAfter(4200, 2300);
  static const EFunc disconnectEffect = Effects::fadeOutAfter(4200, 2300);

  if (WiFi.status() != WL_CONNECTED) {

    static uint32_t last_ms = 0, last_res_ms = 0, last_rec_ms = 0;
    const uint32_t now = millis();

    if (!last_ms     || connected) last_ms = now;
    if (!last_rec_ms || connected) last_rec_ms = now;
    if (!last_res_ms || connected) last_res_ms = now;
    
    if (connected) {
      connected = false;
      updateLeftStatusLight();
    }

    if (now - last_ms > 15000) {
      // every 15secs (synced to 10 pulses) to reset the reconnecting effect
      updateLeftStatusLight();
      Serial.printf(PSTR("WiFi error: %s\n"), getWiFiStatus().c_str());
      last_ms = now;
    }

    if (now - last_rec_ms > 120000) {
      lights.set(STATUS_LEFT, disconnectEffect, CRGB::Red);
      Serial.println(PSTR("WiFi reconnecting"));
      WiFi.disconnect();
      WiFi.begin(secrets::wifi_ssid, secrets::wifi_pwd);
      last_rec_ms = now;
    }

    /*if (now - last_res_ms > 300000) {
      lights.set(STATUS_LEFT, Effects::on, CRGB::Red);
      Serial.println(PSTR("WiFi hard reset"));
      Serial.flush();
      delay(250);
      ESP.restart();
      last_res_ms = now;
    }*/

  } else if (!connected) {
    connected = true;
    lights.set(STATUS_LEFT, connectedEffect, CRGB::Green);
    Serial.printf(PSTR("WiFi successfully connected to %s\n"), WiFi.SSID().c_str());
    Serial.printf(PSTR("IP address: %s\n"), WiFi.localIP().toString().c_str());
  }
}

// switch controls the pump program
void onSwitchChange(int cur, int last) {
  Serial.printf(PSTR("Switch position changed from %d to %d\n"), last, cur);
  Mode mode = (Mode)(cur-1);
  pumps.setMode(mode);
  lights.wakeUp();
}

// turn light on with the pump
void onPumpChange(Pump& pump, uint8_t power) {
  Serial.printf(PSTR("%s set power to %d\n"), pump.getName(), power);
  updateRightStatusLight();
  
  // wake up backlight when a pump is turned on
  if (power)
    lights.wakeUp();
}

// water level indicator and pump lock
void onWaterLevelChange(const WaterLevel& level) {
  Serial.printf(PSTR("Water level changed to %s %d%%\n"), level.name, level.percent);
  updatePumpLockout();
  updateRightStatusLight();
}

// battery low can lock pump
void onBatteryLevelChange(const BatteryLevel& level) {
  Serial.printf(PSTR("Battery level changed to %s\n"), level.name);
  updatePumpLockout();
  updateLeftStatusLight();
}

// lock / unlock pumps
void updatePumpLockout() {
  const bool tooHot = caseSensor.getTemperature() > 60.0f;
  const bool lowPow = battery.getLevel().status <= BATT_LOW;
  const bool lowWater = levelSensor.getLevel().percent <= 25;
  const bool allow = !tooHot && !lowPow && !lowWater;

  if (!allow && !pumps.isLocked()) {
    pumps.lock();
    Serial.print(PSTR("Pumps locked due to"));
    if (tooHot)   Serial.printf(PSTR(" high internal case temperature (%.1f°C)"), caseSensor.getTemperature());
    if (lowPow)   Serial.printf(PSTR(" low battery (%.2fV)"), battery.getVoltage());
    if (lowWater) Serial.printf(PSTR(" low water"));
    Serial.println();

  } else if (allow && pumps.isLocked()) {
    pumps.unlock();
    Serial.println(PSTR("Pumps unlocked"));
  }
}

// right status: pump activity, water level
void updateRightStatusLight() {
  static const EFunc slowPulseEffect = Effects::pulse(3000, 0.05);
  static const EFunc fastPulseEffect = Effects::pulse(1000, 0.05);

  if (pumps.isRunning()) {
    lights.set(STATUS_RIGHT, slowPulseEffect, CRGB::Blue);
  } else { 
    switch (levelSensor.getLevel().percent) {
      case  25: lights.set(STATUS_RIGHT, Effects::on,     CRGB::Red);          break; // too low
      case  50: lights.set(STATUS_RIGHT, slowPulseEffect, CRGB::Yellow);       break; // minimum
      case 100: lights.set(STATUS_RIGHT, fastPulseEffect, CRGB::LightSkyBlue); break; // maximum
      default:  lights.set(STATUS_RIGHT, Effects::off);
    }
  }
}

// left status: wifi, battery status
void updateLeftStatusLight() {
  static const EFunc connectEffect = Effects::blink(1000, 500);
  static const EFunc flashEffect   = Effects::blink(100, 900);
  static const EFunc pulseEffect   = Effects::pulse(2000);
  static const EFunc blinkEffect   = Effects::blink(500, 500);
  
  if (WiFi.status() != WL_CONNECTED) {
    lights.set(STATUS_LEFT, connectEffect, CRGB::Orange);
  } else {
    switch(battery.getLevel().status) {
      case BATT_CRITICAL:   lights.set(STATUS_LEFT, flashEffect, CRGB::Red);   break;
      case BATT_LOW:        lights.set(STATUS_LEFT, pulseEffect, CRGB::Red);   break;
      case BATT_CHARGING:   lights.set(STATUS_LEFT, pulseEffect, CRGB::Green); break;
      case BATT_OVERCHARGE: lights.set(STATUS_LEFT, blinkEffect, CRGB::Red);   break;
      default:              lights.set(STATUS_LEFT, Effects::off);             break;
    }
  }
}


EFunc backlightEffect(uint32_t seed) {
  return [seed] (int i, uint32_t t) -> CRGB {
    static const EFunc glitch = Effects::glitch(seed);

    uint32_t led_15V = 4;
    uint32_t led_0V  = 18;
    if (i >= led_15V && i <= led_0V) {
      uint16_t v = battery.getMillivolts();
      if (v) {
        uint8_t vi = map(v, 0, 15000, led_0V, led_15V);
        if (i >= vi-1 && i <= vi+1) {
          if (v < 12000 ) return CRGB::Red;     // < 25%
          if (v < 12200 ) return CRGB::Orange;  // < 50%
          if (v < 12500 ) return CRGB::Yellow;  // < 75%
          if (v > 14400 ) return CRGB::Red;     // overvoltage
          if (v > 13000 ) return CRGB::Blue;    // charging
          
          return CRGB::Green;                   // 100%
        }
      }
    }

    CRGB bg = glitch(i,t);
    return mult(bg, Effects::PlasmaPurple);
  };
};


void setup() {
  // try to get a random seed
  pinMode(A0, INPUT);
  const uint32_t seed = analogRead(A0) % 16;

  #ifdef SERIAL_ENABLED
    Serial.begin(115200);
  #endif

  // banner
  const char* line = PSTR("----------------------------");
  Serial.printf(PSTR("\n\n\n%s\n hydromat %s       by manu\n"), line, version);
  Serial.printf(PSTR(" built %s %s\n"), __DATE__, __TIME__);
  Serial.println(line);
  Serial.printf(PSTR("RNG seed is %d\n"), seed);

  // switch position sets the switch light
  lights.set(SWITCH, [](int i, uint32_t t) -> CRGB { 
    if (i == 4-swtch.getPos()) 
      return Effects::ON; 
    return Effects::ON % 4;
  }, Effects::PlasmaPurple);

  // turn on light early
  lights.set(BACKLIGHT, backlightEffect(seed));
  lights.init();
  lights.update(0);

  // TODO littlefs
  Serial.print(PSTR("Initializing SPIFFS ... "));
  if (SPIFFS.begin()) {
    Serial.println(PSTR("ok"));
  } else {
    Serial.println(PSTR("failed"));
    lights.errorLoop();
  }

  // loads from fs
  config.load();

  // two pumps
  pumps.add("pump_a", "Main Pump", PIN_PUMP_A, onPumpChange);
  pumps.add("pump_b", "Aux Pump" , PIN_PUMP_B, onPumpChange);
  pumps.init();

  swtch.setOnChange(onSwitchChange);
  swtch.init();

  levelSensor.setOnChange(onWaterLevelChange);
  levelSensor.init();

  battery.setOnChange(onBatteryLevelChange);

  powerSensor.setOnSample([](const PowerSample& sample) { 
    // inaccurate when pumps are running
    if (!pumps.isRunning())
      battery.updateVoltage(sample.batt_mV); 
  });

  caseSensor.setOnSample([](const THSample& _) { 
    updatePumpLockout();
  });

  // i2c
  Wire.begin();
  powerSensor.init();
  
  // OneWire
  dallasSensors.init();

  initTimeZone();
  setupServer();

  // connect to WIFI
  Serial.printf(PSTR("Connecting to %s\n"), secrets::wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.begin(secrets::wifi_ssid, secrets::wifi_pwd);

  Serial.printf(PSTR("MDNS %s.local\n"), hostname);
  MDNS.begin(hostname);
}

void loop(void) {
  handleWiFiReconnect();

  http::server.handleClient();
  MDNS.update();

  Cron.delay();

  // loop timing
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (!last) last = now;
  const uint32_t dt = now - last;
  last = now;

  swtch.update();
  powerSensor.update(dt);
  caseSensor.update(dt);
  levelSensor.update(dt);
  dallasSensors.update(dt);
  pumps.update(dt);

  // lights after internal status changed
  // Note that we run the animation with constant steps,
  // looks less glitchy when loop stalls
  lights.update(16);

  // slow loop
  if (dt > 250)
    Serial.printf(PSTR("Stalled dt = %d ms\n"), dt);

  // cap 60hz
  if (dt < 16)
    FastLED.delay(16 - dt);
}
