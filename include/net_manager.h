#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>

struct NetConfig {
  String ssid;
  String password;
  bool dhcp = true;
  IPAddress ip;
  IPAddress gw;
  IPAddress mask;
  IPAddress dns;
};

class NetManager {
 public:
  bool begin(const JsonDocument &sealedCfg, const JsonDocument *netCfg);
  void loop();

  bool in_setup_mode() const { return setupMode; }
  bool has_credentials() const { return hasCreds; }
  bool sta_connected() const { return WiFi.status() == WL_CONNECTED; }

  bool enter_setup_mode();
  bool save_network_config(const JsonDocument &doc);

  void get_ip_strings(String &apIp, String &staIp) const;
  long rssi() const;

  String chip_ap_ssid() const;

  JsonArray wifi_scan(JsonDocument &outDoc);

 private:
  NetConfig cfg;
  bool setupMode = false;
  bool hasCreds = false;
  bool apStarted = false;
  uint32_t staConnectStartMs = 0;
  uint32_t lastStaRetryMs = 0;

  bool load_from_json(const JsonDocument *netCfg);
  void start_ap();
  void connect_sta();
};
