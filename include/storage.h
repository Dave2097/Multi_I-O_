#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

bool init_fs();
bool storage_exists(const String &path);
bool read_json(const String &path, DynamicJsonDocument &doc);
bool write_json(const String &path, const JsonDocument &doc);
bool set_sealed_flag();
