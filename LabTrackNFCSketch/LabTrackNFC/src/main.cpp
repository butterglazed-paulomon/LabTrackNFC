#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <time.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "webserver.h"
#include "nfc.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

Config config;
AsyncWebServer server(80);

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("LabTrackSystem", "tinapay123");

  Serial.println("\n[INFO] Access Point Started");
  Serial.print("[INFO] SSID: LabTrackSystem\n[INFO] IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Starting LabTrack System ===");

  if (!LittleFS.begin()) {
    Serial.println("[ERROR] Failed to mount LittleFS");
    return;
  } else {
    Serial.println("[INFO] LittleFS mounted");
  }

  bool hasConfig = loadConfig(config);
  bool missingCreds = config.wifi_ssid.isEmpty() || config.wifi_password.isEmpty();

  if (!hasConfig || missingCreds) {
    Serial.println("[WARN] No valid WiFi config found. Starting Access Point...");
    startAccessPoint();
  } else {
    Serial.printf("[INFO] Connecting to WiFi SSID: %s\n", config.wifi_ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());

    unsigned long startAttemptTime = millis();
    const unsigned long timeout = 8000;

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
      Serial.print(".");
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[INFO] WiFi connected!");
      Serial.print("[INFO] IP Address: ");
      Serial.println(WiFi.localIP());

      if (MDNS.begin("labtrack")) {
        Serial.println("[INFO] mDNS started → http://labtrack.local");
      } else {
        Serial.println("[WARN] mDNS setup failed");
      }

      // Add NTP time sync (UTC+8 for PH)
      configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        Serial.println("[INFO] Time synchronized via NTP");
      } else {
        Serial.println("[WARN] Failed to obtain time from NTP");
      }

    } else {
      Serial.println("\n[ERROR] WiFi failed. Starting Access Point...");
      startAccessPoint();
    }
  }

  setupWebServer(server, config);
  server.begin();
  Serial.println("[INFO] Web server started.");

  setupNFC();  
  extern std::queue<String> pendingQueue;
  loadPendingQueue(pendingQueue);
}

void loop() {
  checkNFC();
  feedbackIdle();
}
