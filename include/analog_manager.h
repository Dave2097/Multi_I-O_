#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>

enum class AnalogMode {
  IN,
  OUT
};

class AnalogManager {
 public:
  bool begin(const JsonDocument &cfg);

  int adc_read_raw();
  float adc_read_volts();

  bool set_out_value(int value);
  int out_value() const { return currentOutValue; }

  AnalogMode mode() const { return analogMode; }
  String mode_str() const { return analogMode == AnalogMode::IN ? "in" : "out"; }
  String status() const { return statusText; }
  String driver() const { return outDriver; }

 private:
  AnalogMode analogMode = AnalogMode::IN;
  String outDriver = "pwm_rc";
  String statusText = "ok";

  // input scaling
  float inRefV = 1.0f;
  float inGain = 1.0f;

  // pwm_rc
  uint8_t pwmGpio = 255;
  int pwmMin = 0;
  int pwmMax = 1023;

  // mcp4725
  uint8_t mcpAddress = 0x60;
  uint8_t i2cSda = 4;
  uint8_t i2cScl = 5;
  bool mcpAvailable = false;

  int currentOutValue = 0;

  bool probe_mcp4725();
  bool mcp4725_out_set(int value);
};
