#include "io_manager.h"

bool IOManager::begin(const JsonDocument &cfg) {
  JsonObject io = cfg["io"];
  if (io.isNull()) {
    return false;
  }

  defaultDebounce = io["debounce_ms"] | 30;

  JsonArray relayArr = io["relays"].as<JsonArray>();
  if (relayArr.isNull() || relayArr.size() != 2) {
    return false;
  }
  for (uint8_t i = 0; i < 2; i++) {
    JsonObject r = relayArr[i];
    relays[i].gpio = r["gpio"] | 255;
    relays[i].active_low = r["active_low"] | false;
    relays[i].state = r["default"] | false;
    pinMode(relays[i].gpio, OUTPUT);
    digitalWrite(relays[i].gpio, relays[i].active_low ? !relays[i].state : relays[i].state);
  }

  JsonArray inputArr = io["inputs"].as<JsonArray>();
  if (inputArr.isNull() || inputArr.size() != 4) {
    return false;
  }
  for (uint8_t i = 0; i < 4; i++) {
    JsonObject in = inputArr[i];
    inputs[i].gpio = in["gpio"] | 255;
    inputs[i].pullup = in["pullup"] | false;
    inputs[i].count_edges = in["count_edges"] | true;
    inputs[i].debounce_ms = in["debounce_ms"] | defaultDebounce;
    pinMode(inputs[i].gpio, inputs[i].pullup ? INPUT_PULLUP : INPUT);
    inputs[i].stable_state = read_level(inputs[i].gpio, inputs[i].pullup);
    inputs[i].raw_state = inputs[i].stable_state;
    inputs[i].last_change_ms = millis();
  }

  JsonObject setupBtn = io["setup_button"];
  setupBtnGpio = setupBtn["gpio"] | 255;
  setupBtnActiveLow = setupBtn["active_low"] | true;
  if (setupBtnGpio != 255) {
    pinMode(setupBtnGpio, setupBtnActiveLow ? INPUT_PULLUP : INPUT);
  }

  return true;
}

void IOManager::loop() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < 4; i++) {
    bool raw = read_level(inputs[i].gpio, inputs[i].pullup);
    if (raw != inputs[i].raw_state) {
      inputs[i].raw_state = raw;
      inputs[i].last_change_ms = now;
    }

    if ((now - inputs[i].last_change_ms) >= inputs[i].debounce_ms &&
        inputs[i].stable_state != inputs[i].raw_state) {
      inputs[i].stable_state = inputs[i].raw_state;
      if (inputs[i].count_edges) {
        inputs[i].counter++;
      }
    }
  }
}

bool IOManager::relay_set(uint8_t id, bool state) {
  if (id < 1 || id > 2) {
    return false;
  }
  RelayChannel &r = relays[id - 1];
  r.state = state;
  digitalWrite(r.gpio, r.active_low ? !state : state);
  return true;
}

bool IOManager::relay_get(uint8_t id) const {
  if (id < 1 || id > 2) {
    return false;
  }
  return relays[id - 1].state;
}

bool IOManager::input_get(uint8_t id) const {
  if (id < 1 || id > 4) {
    return false;
  }
  return inputs[id - 1].stable_state;
}

uint32_t IOManager::input_counter(uint8_t id) const {
  if (id < 1 || id > 4) {
    return 0;
  }
  return inputs[id - 1].counter;
}

bool IOManager::setup_button_pressed() const {
  if (setupBtnGpio == 255) {
    return false;
  }
  int level = digitalRead(setupBtnGpio);
  return setupBtnActiveLow ? (level == LOW) : (level == HIGH);
}

bool IOManager::read_level(uint8_t gpio, bool pullup) const {
  (void)pullup;
  return digitalRead(gpio) == HIGH;
}
