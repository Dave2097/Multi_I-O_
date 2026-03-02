#include "web_server.h"

#include <LittleFS.h>

#include "storage.h"

static String json_string(JsonVariantConst v, const char *fallback = "") {
  if (v.is<const char *>()) {
    return String(v.as<const char *>());
  }
  return String(fallback);
}

bool WebServerManager::begin(IOManager *io, AnalogManager *analog, NetManager *net, const JsonDocument *sealedCfg) {
  ioMgr = io;
  analogMgr = analog;
  netMgr = net;
  sealedConfig = sealedCfg;

  setup_routes();

  server.begin();
  ws.begin();
  ws.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    handle_ws_event(num, type, payload, length);
  });
  return true;
}

void WebServerManager::loop() {
  server.handleClient();
  ws.loop();

  if (millis() - lastPushMs >= 250) {
    notify_state();
    lastPushMs = millis();
  }
}

void WebServerManager::notify_state() {
  DynamicJsonDocument doc = build_state_doc();
  String payload;
  serializeJson(doc, payload);
  ws.broadcastTXT(payload);
}

void WebServerManager::setup_routes() {
  server.on("/", HTTP_GET, [this]() {
    const char *page = setup_mode_allowed() ? "/setup.html" : "/index.html";
    File f = LittleFS.open(page, "r");
    if (!f) {
      server.send(404,
                  "text/plain",
                  String(page + 1) + " missing (run: pio run -t uploadfs)");
      return;
    }
    server.sendHeader("Cache-Control", "no-store, max-age=0");
    server.streamFile(f, "text/html");
    f.close();
  });
  server.on("/dashboard", HTTP_GET, [this]() {
    File f = LittleFS.open("/index.html", "r");
    if (!f) {
      server.send(404, "text/plain", "index.html missing (run: pio run -t uploadfs)");
      return;
    }
    server.sendHeader("Cache-Control", "no-store, max-age=0");
    server.streamFile(f, "text/html");
    f.close();
  });

  server.on("/generate_204", HTTP_ANY, [this]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.on("/hotspot-detect.html", HTTP_ANY, [this]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.on("/ncsi.txt", HTTP_ANY, [this]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  server.on("/fwlink", HTTP_ANY, [this]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });


  server.on("/setup", HTTP_GET, [this]() {
    if (!setup_mode_allowed()) {
      server.send(403, "application/json", "{\"error\":\"setup mode required\"}");
      return;
    }
    File f = LittleFS.open("/setup.html", "r");
    if (!f) {
      server.send(404, "text/plain", "setup.html missing");
      return;
    }
    server.sendHeader("Cache-Control", "no-store, max-age=0");
    server.streamFile(f, "text/html");
    f.close();
  });

  server.on("/api/state", HTTP_GET, [this]() {
    DynamicJsonDocument doc = build_state_doc();
    send_json(doc);
  });

  server.on("/api/relay/1", HTTP_POST, [this]() {
    DynamicJsonDocument req(256);
    if (!parse_json_body(req)) {
      return;
    }
    bool state = (req["state"] | 0) == 1;
    ioMgr->relay_set(1, state);
    DynamicJsonDocument doc = build_state_doc();
    send_json(doc);
  });

  server.on("/api/relay/2", HTTP_POST, [this]() {
    DynamicJsonDocument req(256);
    if (!parse_json_body(req)) {
      return;
    }
    bool state = (req["state"] | 0) == 1;
    ioMgr->relay_set(2, state);
    DynamicJsonDocument doc = build_state_doc();
    send_json(doc);
  });

  server.on("/api/analog/out", HTTP_POST, [this]() {
    if (analogMgr->mode() != AnalogMode::OUT) {
      server.send(400, "application/json", "{\"error\":\"analog mode is in\"}");
      return;
    }
    DynamicJsonDocument req(256);
    if (!parse_json_body(req)) {
      return;
    }
    int value = req["value"] | 0;
    bool ok = analogMgr->set_out_value(value);
    DynamicJsonDocument doc(256);
    doc["ok"] = ok;
    doc["status"] = analogMgr->status();
    doc["value"] = analogMgr->out_value();
    send_json(doc, ok ? 200 : 500);
  });

  server.on("/api/config/network", HTTP_GET, [this]() {
    if (!setup_mode_allowed()) {
      server.send(403, "application/json", "{\"error\":\"setup mode required\"}");
      return;
    }

    DynamicJsonDocument netDoc(512);
    if (!read_json("/net.json", netDoc)) {
      netDoc.clear();
      netDoc["ssid"] = "";
      netDoc["dhcp"] = true;
    }
    netDoc.remove("password");
    send_json(netDoc);
  });

  server.on("/api/config/network", HTTP_POST, [this]() {
    if (!setup_mode_allowed()) {
      server.send(403, "application/json", "{\"error\":\"setup mode required\"}");
      return;
    }
    DynamicJsonDocument req(1024);
    if (!parse_json_body(req, 1024)) {
      return;
    }
    if (!req.containsKey("ssid") || !req.containsKey("password")) {
      server.send(400, "application/json", "{\"error\":\"ssid/password required\"}");
      return;
    }

    if (!netMgr->save_network_config(req)) {
      server.send(500, "application/json", "{\"error\":\"failed to save\"}");
      return;
    }
    DynamicJsonDocument doc(256);
    doc["ok"] = true;
    doc["rebooting"] = true;
    send_json(doc);
    delay(400);
    ESP.restart();
  });

  server.on("/api/wifi/scan", HTTP_GET, [this]() {
    if (!setup_mode_allowed()) {
      server.send(403, "application/json", "{\"error\":\"setup mode required\"}");
      return;
    }
    DynamicJsonDocument doc(2048);
    netMgr->wifi_scan(doc);
    send_json(doc);
  });

  server.on("/api/enter-setup", HTTP_POST, [this]() {
    DynamicJsonDocument req(256);
    if (!parse_json_body(req)) {
      return;
    }

    String token = json_string(req["token"]);
    String expected = json_string((*sealedConfig)["security"]["setup_token"]);
    if (token.length() == 0 || token != expected) {
      server.send(403, "application/json", "{\"error\":\"invalid token\"}");
      return;
    }

    netMgr->enter_setup_mode();
    DynamicJsonDocument doc(256);
    doc["ok"] = true;
    doc["setup_mode"] = true;
    send_json(doc);
  });

  server.onNotFound([this]() {
    if (setup_mode_allowed()) {
      server.sendHeader("Location", "/setup", true);
      server.send(302, "text/plain", "Redirecting to /setup");
      return;
    }

    String path = server.uri();
    if (path.endsWith("/")) {
      path += "index.html";
    }

    if (LittleFS.exists(path)) {
      File f = LittleFS.open(path, "r");
      server.sendHeader("Cache-Control", "no-store, max-age=0");
      server.streamFile(f, content_type(path));
      f.close();
      return;
    }

    if (setup_mode_allowed()) {
      server.sendHeader("Location", "/setup", true);
      server.send(302, "text/plain", "Redirecting to /setup");
      return;
    }

    server.send(404, "text/plain", "Not found (run: pio run -t uploadfs)");
  });
}

bool WebServerManager::parse_json_body(DynamicJsonDocument &doc, size_t cap) {
  (void)cap;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"json body required\"}");
    return false;
  }
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"invalid json\"}");
    return false;
  }
  return true;
}

void WebServerManager::send_json(JsonDocument &doc, int code) {
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

DynamicJsonDocument WebServerManager::build_state_doc() {
  DynamicJsonDocument doc(2048);

  JsonObject relays = doc.createNestedObject("relays");
  relays["1"] = ioMgr->relay_get(1);
  relays["2"] = ioMgr->relay_get(2);

  JsonArray ins = doc.createNestedArray("inputs");
  for (int i = 1; i <= 4; i++) {
    JsonObject in = ins.createNestedObject();
    in["id"] = i;
    in["state"] = ioMgr->input_get(i);
    in["counter"] = ioMgr->input_counter(i);
  }

  JsonObject analog = doc.createNestedObject("analog");
  analog["mode"] = analogMgr->mode_str();
  analog["driver"] = analogMgr->driver();
  analog["status"] = analogMgr->status();
  if (analogMgr->mode() == AnalogMode::IN) {
    analog["raw"] = analogMgr->adc_read_raw();
    analog["volts"] = analogMgr->adc_read_volts();
  } else {
    analog["value"] = analogMgr->out_value();
  }

  JsonObject sys = doc.createNestedObject("system");
  String apIp, staIp;
  netMgr->get_ip_strings(apIp, staIp);
  sys["ap_ip"] = apIp;
  sys["ip"] = staIp;
  sys["rssi"] = netMgr->rssi();
  sys["uptime_ms"] = millis();
  sys["fw_profile"] = json_string((*sealedConfig)["device"]["fw_profile"], "default");
  sys["setup_mode"] = netMgr->in_setup_mode();

  return doc;
}

bool WebServerManager::setup_mode_allowed() const {
  return netMgr->in_setup_mode();
}

void WebServerManager::handle_ws_event(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  (void)payload;
  (void)length;
  if (type == WStype_CONNECTED) {
    DynamicJsonDocument doc = build_state_doc();
    String msg;
    serializeJson(doc, msg);
    ws.sendTXT(num, msg);
  }
}

String WebServerManager::content_type(const String &path) const {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".js")) return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  return "text/plain";
}
