#pragma once

#include <ESP8266WebServer.h>
#include "utils.h"

namespace http
{
  ESP8266WebServer server(80);

  const String methods[8]{ "ANY", "GET", "HEAD", "POST", "PUT", "PATCH", "DELETE", "OPTIONS" };

  bool sendJson(JsonDocument json) {
    String string;
    serializeJson(json, string);
    server.send(200, "application/json", string);
    return true;
  }

  bool sendText(String text) {
    server.send(200, "text/plain", text);
    return true;
  }

  bool sendOK() {
    server.send(200);
    return true;
  }

  bool sendRedirect(String location) {
    server.sendHeader("Location", location);
    server.send(303);
    return true;
  }

  bool sendError() {
    server.send(500, "text/plain", "500: Internal Server Error");
    return true;
  }

  String getContentType(String filename) {
    if (filename.endsWith(".html") || filename.endsWith(".htm")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js"))  return "application/javascript";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    else if (filename.endsWith(".png")) return "image/png";
    return "text/plain";
  }

  // Response with file from FS
  bool handleFile(String path) {
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";

    // Send requested (compressed) file
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {

      if (SPIFFS.exists(pathWithGz)) path += ".gz";

      File file = SPIFFS.open(path, "r");
      size_t sent = server.streamFile(file, contentType);
      file.close();

      return true;
    }

    return false;
  }

};