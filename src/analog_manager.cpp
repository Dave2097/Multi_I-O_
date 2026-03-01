#include "analog_manager.h"

static String json_string(JsonVariantConst v, const char *fallback = "") {
  if (v.is<const char *>()) {
    return String(v.as<const char *>());
  }
  return String(fallback);
}

bool AnalogManager::begin(const JsonDocument &cfg) {
  JsonObject analog = cfg["analog"];
  if (analog.isNull()) {
    statusText = "error";
    return false;
  }

  String modeStr = json_string(analog["mode"], "in");
  modeStr.toLowerCase();
  analogMode = (modeStr == "out") ? AnalogMode::OUT : AnalogMode::IN;

  JsonObject in = analog["in"];
  inRefV = in["ref_v"] | 1.0f;
  inGain = in["gain"] | 1.0f;

  if (analogMode == AnalogMode::OUT) {
    JsonObject out = analog["out"];
    outDriver = json_string(out["driver"], "pwm_rc");
    outDriver.toLowerCase();

    if (outDriver == "pwm_rc") {
      JsonObject pwm = out["pwm_rc"];
      pwmGpio = pwm["gpio"] | 2;
      pwmMin = pwm["min"] | 0;
      pwmMax = pwm["max"] | 1023;
      pinMode(pwmGpio, OUTPUT);
      analogWriteRange(1023);
      analogWriteFreq(1000);
      statusText = "ok";
    } else if (outDriver == "mcp4725") {
      JsonObject m = out["mcp4725"];
      mcpAddress = m["address"] | 0x60;
      i2cSda = m["sda"] | 4;
      i2cScl = m["scl"] | 5;
      Wire.begin(i2cSda, i2cScl);
      mcpAvailable = probe_mcp4725();
      statusText = mcpAvailable ? "ok" : "error";
    } else {
      statusText = "error";
      return false;
    }
  } else {
    statusText = "ok";
  }

  return true;
}

int AnalogManager::adc_read_raw() {
  return analogRead(A0);
}

float AnalogManager::adc_read_volts() {
  int raw = adc_read_raw();
  return (raw / 1023.0f) * inRefV * inGain;
}

bool AnalogManager::set_out_value(int value) {
  if (analogMode != AnalogMode::OUT) {
    return false;
  }

  value = constrain(value, 0, 4095);
  currentOutValue = value;

  if (outDriver == "pwm_rc") {
    int pwmValue = map(value, 0, 4095, pwmMin, pwmMax);
    analogWrite(pwmGpio, pwmValue);
    statusText = "ok";
    return true;
  }

  if (outDriver == "mcp4725") {
    bool ok = mcp4725_out_set(value);
    statusText = ok ? "ok" : "error";
    return ok;
  }

  statusText = "error";
  return false;
}

bool AnalogManager::probe_mcp4725() {
  Wire.beginTransmission(mcpAddress);
  return Wire.endTransmission() == 0;
}

bool AnalogManager::mcp4725_out_set(int value) {
  if (!mcpAvailable) {
    mcpAvailable = probe_mcp4725();
    if (!mcpAvailable) {
      return false;
    }
  }

  Wire.beginTransmission(mcpAddress);
  Wire.write(0x40);
  Wire.write((value >> 4) & 0xFF);
  Wire.write((value & 0x0F) << 4);
  uint8_t res = Wire.endTransmission();
  if (res != 0) {
    mcpAvailable = false;
    return false;
  }
  return true;
}
