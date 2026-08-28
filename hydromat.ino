/* 
 *  Hydromat 2000
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <uri/UriRegex.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <uptime.h>
#include <uptime_formatter.h>
#include "ESP8266TimerInterrupt.h"
#include "ESP8266_ISR_Timer.h"
#include <CircularBuffer.hpp>
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
#include "powersensor.h"
#include "sht21sensor.h"
#include "levelsensor.h"
#include "dallassensors.h"

#include "secrets.h"

#define SERIAL_ENABLED

const char *hostname = "hydromat";
const char *version = "0.2";

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

PowerSensor powerSensor("main_power", 0x41, 240);       // i2c devices on the same wires
SHT21Sensor caseSensor ("case_temp",  0x40,  60, 5000); // samples 5 sec before power sensor

LevelSensor<PIN_LEVEL> levelSensor("main_tank", 3600, 1000);

DallasSensors<PIN_DS_ONEWIRE, 2> dallasSensors("ext_temp", 240, 2000);


bool handleSysinfo()
{
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


void setupServer()
{
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

  Serial.println("HTTP server started");
}


void setupOTA()
{
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPasswordHash(secrets::ota_pwd_hash);
  //ArduinoOTA.setPassword("testtest");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    SPIFFS.end();
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)         Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
  });

  ArduinoOTA.begin();
}


void setup() 
{ 
  lights.set(SWITCH, [](int i, int t) -> CRGB { 
    if (i == 4-swtch.getPos()) 
      return Effects::ON; 
    return Effects::ON % 4;
  }, Effects::PlasmaPurple);

  lights.set(BACKLIGHT, Effects::glitch(), Effects::PlasmaPurple);
  //lights.set(STATUS_LEFT, Effects::blink(100, 2300), CRGB::Orange);
  lights.init();
  lights.update(0);

  // turn light on only when a pump is on
  static const EFunc pumpPulseEffect = Effects::pulse(2300, 0.05);
  auto onPumpChange = [](Pump& pump, uint8_t power) { 
    Serial.printf("%s set power %d\n", pump.getName(), power);
    if (!power && pumps.isAllOff())
      lights.set(STATUS_RIGHT, Effects::off);
    else
      lights.set(STATUS_RIGHT, pumpPulseEffect, CRGB::Blue);
  };

  // two pumps
  pumps.add("pump_a", "Main Pump", PIN_PUMP_A, onPumpChange);
  pumps.add("pump_b", "Aux Pump" , PIN_PUMP_B, onPumpChange);

  // switch sets the pump's program
  swtch.hookOnChange([](int cur, int last) { 
    Serial.printf("Switch pos %d -> %d\n", last, cur);
    Mode mode = (Mode)(cur-1);
    pumps.setMode(mode);
  });

  // react on water level changed
  levelSensor.hookOnChange([](const Level& level) { 
    Serial.printf("Level changed to %s %d%%\n", level.name, level.percent);
  });

  #ifdef SERIAL_ENABLED
    Serial.begin(115200);
  #endif
  
  auto line = F("----------------------------");
  Serial.printf("\n\n\n%s\n hydromat %s       by manu\n", line, version);
  Serial.printf(" built %s %s\n", __DATE__, __TIME__);
  Serial.println(line);
  Serial.print("\n\nInitializing SPIFFS ... ");
  if (SPIFFS.begin()) {
    Serial.println("ok");
  } else {
    Serial.println("failed");
    lights.errorLoop();
  }

  config.load();

  pumps.init();
  swtch.init();
  levelSensor.init();

  // i2c sensors
  Wire.begin();
  powerSensor.init();
  caseSensor.init();
  dallasSensors.init();

  initTimeZone();
  setupServer();

  // connect to WIFI
  Serial.println("Connecting to " + String(secrets::wifi_ssid));
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(hostname);
  WiFi.begin(secrets::wifi_ssid, secrets::wifi_pwd);

  Serial.println("MDNS " + String(hostname) + ".local");
  MDNS.begin(hostname);

  setupOTA();
}


void handleWiFiReconnect() 
{
  static bool connected = false;

  if (WiFi.status() != WL_CONNECTED) {

    static uint32_t last_ms = 0, last_res_ms = 0, last_rec_ms = 0;
    const uint32_t now = millis();

    if (!last_ms     || connected) last_ms = now;
    if (!last_rec_ms || connected) last_rec_ms = now;
    if (!last_res_ms || connected) last_res_ms = now;
    
    connected = false;
    
    if (now - last_ms > 15000) {
      Serial.println(getWiFiStatus());
      last_ms = now;
    }

    if (now - last_rec_ms > 60000) {
      Serial.println("Reconnecting");
      WiFi.disconnect();
      WiFi.begin(secrets::wifi_ssid, secrets::wifi_pwd);
      last_rec_ms = now;
    }

    if (now - last_res_ms > 300000) {
      Serial.println("Hard reset ");
      Serial.flush();
      delay(250);
      ESP.restart();
      last_res_ms = now;
    }

  } else if (!connected) {
    connected = true;
    Serial.println("Successfully connected to " + WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}


void loop(void) 
{
  handleWiFiReconnect();

  ArduinoOTA.handle();
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
  pumps.update(dt);
  powerSensor.update(dt);
  caseSensor.update(dt);
  levelSensor.update(dt);
  dallasSensors.update(dt);

  // lights after internal status changed
  lights.update(dt);

  // slow loop
  if (dt > 250)
    Serial.println("Stalled dt = "+ String(dt)+ "ms");

  // cap 32hz
  if (dt < 16)
    FastLED.delay(16 - dt);
}
