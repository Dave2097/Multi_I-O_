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

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\nMF-IO boot");

  if (!init_fs()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  if (!read_json("/config.json", gConfig) || !validate_config(gConfig)) {
    Serial.println("Invalid /config.json");
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
