#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct InputChannel {
  uint8_t gpio = 0;
  bool pullup = false;
  bool count_edges = true;
  bool stable_state = false;
  bool raw_state = false;
  uint32_t debounce_ms = 30;
  uint32_t last_change_ms = 0;
  uint32_t counter = 0;
};

struct RelayChannel {
  uint8_t gpio = 0;
  bool active_low = false;
  bool state = false;
};

class IOManager {
 public:
  bool begin(const JsonDocument &cfg);
  void loop();

  bool relay_set(uint8_t id, bool state);
  bool relay_get(uint8_t id) const;

  bool input_get(uint8_t id) const;
  uint32_t input_counter(uint8_t id) const;

  uint8_t setup_button_gpio() const { return setupBtnGpio; }
  bool setup_button_active_low() const { return setupBtnActiveLow; }
  bool setup_button_pressed() const;

 private:
  RelayChannel relays[2];
  InputChannel inputs[4];
  uint8_t setupBtnGpio = 255;
  bool setupBtnActiveLow = true;
  uint32_t defaultDebounce = 30;

  bool read_level(uint8_t gpio, bool pullup) const;
};
