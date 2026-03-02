#include <Arduino.h>
#include <ArduinoJson.h>

#include "analog_manager.h"
#include "io_manager.h"
#include "net_manager.h"
#include "storage.h"
#include "web_server.h"

static DynamicJsonDocument gConfig(4096);
static DynamicJsonDocument gNetConfig(1024);

static IOManager ioMgr;
static AnalogManager analogMgr;
static NetManager netMgr;
static WebServerManager webMgr;

static uint32_t setupPressStart = 0;
static bool setupTriggered = false;
static bool bootOk = false;
static bool bootErrorPrinted = false;

static const char *kDefaultConfigJson = R"JSON({
  "device": {"name": "MF-IO", "fw_profile": "default"},
  "io": {
    "debounce_ms": 30,
    "relays": [
      {"gpio": 5, "active_low": true, "default": 0},
      {"gpio": 4, "active_low": true, "default": 0}
    ],
    "inputs": [
      {"gpio": 14, "pullup": true, "debounce_ms": 30, "count_edges": true},
      {"gpio": 12, "pullup": true, "debounce_ms": 30, "count_edges": true},
      {"gpio": 13, "pullup": true, "debounce_ms": 30, "count_edges": true},
      {"gpio": 16, "pullup": false, "debounce_ms": 30, "count_edges": true}
    ],
    "setup_button": {"gpio": 0, "active_low": true}
  },
  "analog": {
    "mode": "in",
    "in": {"ref_v": 1.0, "gain": 1.0},
    "out": {
      "driver": "pwm_rc",
      "pwm_rc": {"gpio": 2, "min": 0, "max": 1023},
      "mcp4725": {"address": 96, "sda": 4, "scl": 5}
    }
  },
  "security": {"setup_token": "change-me-setup-token"}
})JSON";

static String json_string(JsonVariantConst v, const char *fallback = "") {
  if (v.is<const char *>()) {
    return String(v.as<const char *>());
  }
  return String(fallback);
}

static bool validate_config(const DynamicJsonDocument &cfg) {
  if (!cfg.containsKey("io") || !cfg.containsKey("analog") || !cfg.containsKey("security")) {
    return false;
  }
  JsonObjectConst io = cfg["io"].as<JsonObjectConst>();
  if (io["relays"].as<JsonArrayConst>().size() != 2) {
    return false;
  }
  if (io["inputs"].as<JsonArrayConst>().size() != 4) {
    return false;
  }
  String mode = json_string(cfg["analog"]["mode"]);
  mode.toLowerCase();
  return mode == "in" || mode == "out";
}

static bool load_or_create_default_config(DynamicJsonDocument &cfg) {
  if (read_json("/config.json", cfg)) {
    return true;
  }

  if (storage_exists("/config.json")) {
    Serial.println("/config.json exists but is invalid JSON - keeping file for manual fix");
    return false;
  }

  Serial.println("/config.json missing - writing default config");
  DynamicJsonDocument defaults(4096);
  DeserializationError err = deserializeJson(defaults, kDefaultConfigJson);
  if (err) {
    Serial.printf("Default config parse failed: %s\n", err.c_str());
    return false;
  }
  if (!write_json("/config.json", defaults)) {
    Serial.println("Failed to write default /config.json");
    return false;
  }
  Serial.println("Default /config.json created");

  return read_json("/config.json", cfg);
}

void setup() {
  Serial.begin(74880);
  delay(100);
  Serial.println("\nMF-IO boot");

  if (!init_fs()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  if (!load_or_create_default_config(gConfig)) {
    Serial.println("Failed reading /config.json");
    Serial.println("Hint: if file is missing run pio run -t uploadfs; if parse error, fix JSON manually");
    return;
  }
  if (!validate_config(gConfig)) {
    Serial.println("Invalid /config.json (required fields missing)");
    return;
  }

  if (!storage_exists("/sealed.flag")) {
    set_sealed_flag();
  }

  bool hasNet = read_json("/net.json", gNetConfig);

  if (!ioMgr.begin(gConfig)) {
    Serial.println("IO init failed");
    return;
  }
  if (!analogMgr.begin(gConfig)) {
    Serial.println("Analog init failed");
    return;
  }
  if (!netMgr.begin(gConfig, hasNet ? &gNetConfig : nullptr)) {
    Serial.println("Network init failed");
    return;
  }
  if (!webMgr.begin(&ioMgr, &analogMgr, &netMgr, &gConfig)) {
    Serial.println("Web init failed");
    return;
  }

  bootOk = true;
  Serial.println("MF-IO boot complete");
}

void loop() {
  if (!bootOk) {
    if (!bootErrorPrinted) {
      Serial.println("Boot failed - idle mode. Check /config.json and uploadfs.");
      bootErrorPrinted = true;
    }
    delay(1000);
    return;
  }

  ioMgr.loop();
  netMgr.loop();
  webMgr.loop();

  if (!setupTriggered && ioMgr.setup_button_pressed()) {
    if (setupPressStart == 0) {
      setupPressStart = millis();
    } else if (millis() - setupPressStart >= 5000) {
      setupTriggered = true;
      netMgr.enter_setup_mode();
      Serial.println("Setup mode via button");
    }
  } else {
    setupPressStart = 0;
  }

  delay(2);
}
