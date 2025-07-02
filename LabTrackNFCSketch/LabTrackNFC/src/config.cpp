#include "USB.h"
#define Serial USBSerial
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#define CONFIG_FILE "/config.json"

bool loadConfig(Config &config) {
  if (!LittleFS.exists(CONFIG_FILE)) return false;
  File file = LittleFS.open(CONFIG_FILE, "r");
  if (!file) return false;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, file);
  if (err) return false;

  config.wifi_ssid = doc["wifi_ssid"] | "";
  config.wifi_password = doc["wifi_password"] | "";
  config.flask_ip = doc["flask_ip"] | "";

  return true;
}

bool saveConfig(const Config &config) {
  File file = LittleFS.open(CONFIG_FILE, "w");
  if (!file) return false;

  JsonDocument doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["flask_ip"] = config.flask_ip;

  return serializeJson(doc, file) > 0;
}
