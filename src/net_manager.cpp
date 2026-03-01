#include "net_manager.h"

#include "storage.h"

static String json_string(JsonVariantConst v, const char *fallback = "") {
  if (v.is<const char *>()) {
    return String(v.as<const char *>());
  }
  return String(fallback);
}

bool NetManager::begin(const JsonDocument &sealedCfg, const JsonDocument *netCfg) {
  (void)sealedCfg;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);

  hasCreds = load_from_json(netCfg);
  Serial.printf("[NET] has credentials: %s\n", hasCreds ? "yes" : "no");

  if (!hasCreds) {
    setupMode = true;
    start_ap();
    return true;
  }

  setupMode = false;
  staConnectStartMs = millis();
  connect_sta();
  return true;
}

void NetManager::loop() {
  if (setupMode) {
    return;
  }

  if (!hasCreds) {
    setupMode = true;
    start_ap();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  uint32_t now = millis();
  if (now - lastStaRetryMs > 10000) {
    lastStaRetryMs = now;
    connect_sta();
  }

  // Fallback: if STA cannot connect for 30s, open setup AP automatically.
  if (now - staConnectStartMs > 30000) {
    Serial.println("[NET] STA connect timeout -> entering setup AP mode");
    setupMode = true;
    start_ap();
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

  cfg.ssid = json_string((*netCfg)["ssid"]);
  cfg.ssid.trim();
  cfg.password = json_string((*netCfg)["password"]);
  cfg.dhcp = (*netCfg)["dhcp"] | true;
  if (!cfg.dhcp) {
    cfg.ip.fromString(json_string((*netCfg)["ip"], "0.0.0.0"));
    cfg.gw.fromString(json_string((*netCfg)["gw"], "0.0.0.0"));
    cfg.mask.fromString(json_string((*netCfg)["mask"], "255.255.255.0"));
    cfg.dns.fromString(json_string((*netCfg)["dns"], "0.0.0.0"));
  }
  return cfg.ssid.length() > 0;
}

void NetManager::start_ap() {
  String ssid = chip_ap_ssid();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  apStarted = WiFi.softAP(ssid.c_str(), "setup1234");

  // Fallback to AP-only mode when AP+STA is unstable.
  if (!apStarted) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    apStarted = WiFi.softAP(ssid.c_str(), "setup1234");
  }

  Serial.printf("[NET] AP start %s, SSID=%s, IP=%s\n",
                apStarted ? "OK" : "FAILED",
                ssid.c_str(),
                WiFi.softAPIP().toString().c_str());

  WiFi.scanNetworks(true);
}

void NetManager::connect_sta() {
  WiFi.mode(setupMode ? WIFI_AP_STA : WIFI_STA);
  if (!cfg.dhcp) {
    WiFi.config(cfg.ip, cfg.gw, cfg.mask, cfg.dns);
  }
  WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
  Serial.printf("[NET] STA connecting to SSID=%s\n", cfg.ssid.c_str());
}
