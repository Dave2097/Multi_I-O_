#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

#include "analog_manager.h"
#include "io_manager.h"
#include "net_manager.h"

class WebServerManager {
 public:
  bool begin(IOManager *io, AnalogManager *analog, NetManager *net, const JsonDocument *sealedCfg);
  void loop();

  void notify_state();

 private:
  ESP8266WebServer server{80};
  WebSocketsServer ws{81};

  IOManager *ioMgr = nullptr;
  AnalogManager *analogMgr = nullptr;
  NetManager *netMgr = nullptr;
  const JsonDocument *sealedConfig = nullptr;

  uint32_t lastPushMs = 0;

  void setup_routes();
  bool parse_json_body(DynamicJsonDocument &doc, size_t cap = 1024);
  void send_json(JsonDocument &doc, int code = 200);

  DynamicJsonDocument build_state_doc();
  bool setup_mode_allowed() const;

  void handle_ws_event(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

  String content_type(const String &path) const;
};
