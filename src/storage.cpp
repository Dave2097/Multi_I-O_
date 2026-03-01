#include "storage.h"

#include <LittleFS.h>

bool init_fs() {
  return LittleFS.begin();
}

bool storage_exists(const String &path) {
  return LittleFS.exists(path);
}

bool read_json(const String &path, DynamicJsonDocument &doc) {
  if (!LittleFS.exists(path)) {
    Serial.printf("[FS] missing: %s\n", path.c_str());
    return false;
  }
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[FS] open failed: %s\n", path.c_str());
    return false;
  }
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[FS] json parse error in %s: %s\n", path.c_str(), err.c_str());
    return false;
  }
  return true;
}

bool write_json(const String &path, const JsonDocument &doc) {
  File f = LittleFS.open(path, "w");
  if (!f) {
    return false;
  }
  if (serializeJson(doc, f) == 0) {
    f.close();
    return false;
  }
  f.close();
  return true;
}

bool set_sealed_flag() {
  File f = LittleFS.open("/sealed.flag", "w");
  if (!f) {
    return false;
  }
  f.print("sealed");
  f.close();
  return true;
}
