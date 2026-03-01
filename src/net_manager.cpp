#include "net_manager.h"

#include "storage.h"

bool NetManager::begin(const JsonDocument &sealedCfg, const JsonDocument *netCfg) {
  (void)sealedCfg;
  WiFi.mode(WIFI_OFF);
  delay(50);
  hasCreds = load_from_json(netCfg);

  if (!hasCreds) {
    setupMode = true;
    start_ap();
    return true;
  }

  setupMode = false;
  connect_sta();
  return true;
}

void NetManager::loop() {
  if (!setupMode && hasCreds && WiFi.status() != WL_CONNECTED) {
    static uint32_t lastRetry = 0;
    if (millis() - lastRetry > 10000) {
      lastRetry = millis();
      connect_sta();
    }
  }
}

bool NetManager::enter_setup_mode() {
  if (setupMode) {
    return true;
  }
  setupMode = true;
  start_ap();
  return true;
}

bool NetManager::save_network_config(const JsonDocument &doc) {
  if (!setupMode) {
    return false;
  }
  if (!write_json("/net.json", doc)) {
    return false;
  }
  return true;
}

void NetManager::get_ip_strings(String &apIp, String &staIp) const {
  apIp = apStarted ? WiFi.softAPIP().toString() : "";
  staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "";
}

long NetManager::rssi() const {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }
  return WiFi.RSSI();
}

String NetManager::chip_ap_ssid() const {
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "MF-IO-SETUP-%06X", ESP.getChipId());
  return String(ssid);
}

JsonArray NetManager::wifi_scan(JsonDocument &outDoc) {
  JsonArray arr = outDoc.createNestedArray("networks");
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED || n == WIFI_SCAN_RUNNING) {
    WiFi.scanNetworks(true);
    return arr;
  }
  for (int i = 0; i < n; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
    o["enc"] = WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  return arr;
}

bool NetManager::load_from_json(const JsonDocument *netCfg) {
  if (netCfg == nullptr) {
    return false;
  }
  cfg.ssid = (*netCfg)["ssid"] | "";
  cfg.password = (*netCfg)["password"] | "";
  cfg.dhcp = (*netCfg)["dhcp"] | true;
  if (!cfg.dhcp) {
    cfg.ip.fromString((*netCfg)["ip"] | "0.0.0.0");
    cfg.gw.fromString((*netCfg)["gw"] | "0.0.0.0");
    cfg.mask.fromString((*netCfg)["mask"] | "255.255.255.0");
    cfg.dns.fromString((*netCfg)["dns"] | "0.0.0.0");
  }
  return cfg.ssid.length() > 0;
}

void NetManager::start_ap() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(chip_ap_ssid().c_str(), "setup1234");
  apStarted = true;
  WiFi.scanNetworks(true);
}

void NetManager::connect_sta() {
  WiFi.mode(setupMode ? WIFI_AP_STA : WIFI_STA);
  if (!cfg.dhcp) {
    WiFi.config(cfg.ip, cfg.gw, cfg.mask, cfg.dns);
  }
  WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
}
