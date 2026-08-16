#pragma once
#include <time.h>
#include <ESP8266WiFi.h>

// STRING AND NUMBER STUFF ///////////////////////

String leadingZero(int num) {
  if (num < 10) return "0" + String(num);
  return String(num);
}

template<typename T> T 
clamp(T value, T min, T max) {
  if (value > max) return max;
  if (value < min) return min;
  return value;
}

const CRGB& mult(CRGB& a, const CRGB& b) {
  a.r = ((int)a.r * (int)b.r) / 0xff;
  a.g = ((int)a.g * (int)b.g) / 0xff;
  a.b = ((int)a.b * (int)b.b) / 0xff;
  return a;
}

// TIME STUFF ////////////////////////////////////

const char* tz_str = "CET-1CEST,M3.5.0,M10.5.0/3";

void initTimeZone() {
  configTime(tz_str, "at.pool.ntp.org");

  time_t now;
  do {
    time(&now);
    delay(100);
  }  while (!now);

  setenv("TZ", tz_str, 1);
  tzset();
}


const String weekdays[7]{ "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
const String weekdays_short[7]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

String getCurrentTimeString(bool long_day = false) {
  time_t now;
  time(&now);

  tm t;
  localtime_r(&now, &t);

  auto& days = (long_day ? weekdays : weekdays_short);

  return days[t.tm_wday]           + " " + 
         leadingZero(t.tm_mday)    + "." + 
         leadingZero(t.tm_mon + 1) + "." + 
         String(t.tm_year + 1900)  + " " + 
         leadingZero(t.tm_hour)    + ":" +
         leadingZero(t.tm_min)     + ":" + 
         leadingZero(t.tm_sec);
}

// OTHER STUFF ////////////////////////////////////

const String getWiFiStatus() {
  wl_status_t status = WiFi.status();
  switch (status) {
    case WL_IDLE_STATUS     : return "Idle"; 
    case WL_NO_SSID_AVAIL   : return "SSID cannot be reached";
    case WL_SCAN_COMPLETED  : return "Scan completed";
    case WL_CONNECT_FAILED  : return "Connection failed";
    case WL_CONNECTION_LOST : return "Connection lost";
    case WL_WRONG_PASSWORD  : return "Wrong password";
    case WL_DISCONNECTED    : return "Disconnected";
    default:                  return "WiFi.status() = " + String(status);
  }
}
