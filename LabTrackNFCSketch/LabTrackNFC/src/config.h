#pragma once
#include <Arduino.h>
#define CONFIG_PASSWORD "tinapay123"

struct Config {
  String wifi_ssid;
  String wifi_password;
  String flask_ip;
};

bool loadConfig(Config &config);
bool saveConfig(const Config &config);
