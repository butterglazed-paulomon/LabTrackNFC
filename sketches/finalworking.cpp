#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define BUTTON_PIN 0
#define LED_BORROW 14
#define LED_RETURN 15
#define BUZZER_PIN 13

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
AsyncWebServer server(80);
Preferences prefs;

const String formURL = "https://forms.gle/JMqC2oakcRS4HuhK6";

struct Config {
  String ssid;
  String password;
  String pc_ip;
} config;

String currentMode = "borrow";
String lastUID = "";
unsigned long lastScan = 0;
const unsigned long cooldown = 3000;
bool nfcReady = false;

void beep(int freq = 1200, int dur = 100) {
  tone(BUZZER_PIN, freq, dur);
}

void updateLEDs() {
  digitalWrite(LED_BORROW, currentMode == "borrow" ? HIGH : LOW);
  digitalWrite(LED_RETURN, currentMode == "return" ? HIGH : LOW);
}

void loadConfig() {
  prefs.begin("labtrack", true);
  String jsonStr = prefs.getString("config", "");
  prefs.end();

  if (jsonStr.length()) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, jsonStr) == DeserializationError::Ok) {
      config.ssid = doc["ssid"] | "";
      config.password = doc["password"] | "";
      config.pc_ip = doc["pc_ip"] | "";
    }
  }
}

void saveConfig() {
  StaticJsonDocument<256> doc;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["pc_ip"] = config.pc_ip;
  String out;
  serializeJson(doc, out);

  prefs.begin("labtrack", false);
  prefs.putString("config", out);
  prefs.end();
}
void postToPC(String uid, String action) {
  if (WiFi.status() != WL_CONNECTED || config.pc_ip == "") return;

  HTTPClient http;
  String url = "http://" + config.pc_ip + ":8000/receive";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<128> doc;
  doc["uid"] = uid;
  doc["action"] = action;
  String payload;
  serializeJson(doc, payload);

  http.POST(payload);
  http.end();
}

void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    String html = "<html><head><title>LabTrack NFC</title><style>"
    "body {background: url('https://raw.githubusercontent.com/butterglazed-paulomon/pauzen/main/index1.png') center/cover no-repeat;"
    "font-family: 'Segoe UI', sans-serif; color:white; text-align:center; margin:0; padding:0;}"
    ".container {background: rgba(0,0,0,0.5); min-height:100vh; padding:30px 0;}"
    "button, input[type=submit] {font-size:18px; padding:10px 30px; margin:8px; border:none; border-radius:4px; background:#1e90ff; color:white; cursor:pointer;}"
    "button:hover, input[type=submit]:hover {background:#0b75c9;}"
    "input[type=text] {padding:8px; font-size:16px; border-radius:4px; border:1px solid #ccc; margin:10px;}"
    "a {color:lightblue; text-decoration:none;}"
    "</style></head><body><div class='container'>"
    "<h2>LabTrack NFC</h2><h3>Current Mode: <span style='color:yellow'>" + currentMode + "</span></h3>"
    "<a href='/set?mode=borrow'><button>Borrow</button></a><a href='/set?mode=return'><button>Return</button></a>"
    "<p><a href='" + formURL + "' target='_blank'>Open Borrowing Form</a></p>"
    "<br><img src='https://api.qrserver.com/v1/create-qr-code/?data=" + formURL + "&size=200x200'>"
    "<br>"
    "<hr><h3>!Test Mode!</h3><form action='/simulate' method='GET'>"
    "UID: <input type='text' name='uid'><br><br>"
    "<input type='submit' name='action' value='Simulate Borrow'> "
    "<input type='submit' name='action' value='Simulate Return'></form>"
    "<p>Last UID: " + lastUID + "</p><hr><a href='/config'><button>Configuration</button></a>"
    "</div></body></html>";

    req->send(200, "text/html", html);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("mode")) {
      currentMode = req->getParam("mode")->value();
      updateLEDs();
      beep();
    }
    req->redirect("/");
  });

  server.on("/simulate", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (req->hasParam("uid") && req->hasParam("action")) {
      String uid = req->getParam("uid")->value();
      String action = req->getParam("action")->value();
      action.toLowerCase();
      if (action.indexOf("borrow") >= 0) action = "borrow";
      if (action.indexOf("return") >= 0) action = "return";
      lastUID = uid;
      postToPC(uid, action);
      beep();
    }
    req->redirect("/");
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    String html = "<html><body><h3>Configure Wi-Fi and PC IP</h3>";
    html += "<form action='/save'>";
    html += "SSID: <input name='ssid' value='" + config.ssid + "'><br>";
    html += "Password: <input name='password' value='" + config.password + "'><br>";
    html += "PC IP: <input name='pc_ip' value='" + config.pc_ip + "'><br>";
    html += "<input type='submit' value='Save'></form>";
    html += "<form action='/restart'><button>Reboot Device</button></form></body></html>";
    req->send(200, "text/html", html);
  });

  server.on("/save", HTTP_GET, [](AsyncWebServerRequest *req) {
    config.ssid = req->getParam("ssid")->value();
    config.password = req->getParam("password")->value();
    config.pc_ip = req->getParam("pc_ip")->value();
    saveConfig();
    req->send(200, "text/html", "<p>Saved. Rebooting...</p>");
    delay(1000);
    ESP.restart();
  });

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "text/plain", "Restarting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BORROW, OUTPUT);
  pinMode(LED_RETURN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  updateLEDs();

  loadConfig();
  WiFi.begin(config.ssid.c_str(), config.password.c_str());

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(300);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP("Paulomon_LabTrack_Config", "k41z3nj0y1!");
  }

  nfc.begin();
  uint32_t version = nfc.getFirmwareVersion();
  if (version) {
    nfc.SAMConfig();
    nfcReady = true;
  }

  setupWeb();
}

void loop() {
  static bool lastBtn = HIGH;
  bool nowBtn = digitalRead(BUTTON_PIN);

  if (lastBtn == HIGH && nowBtn == LOW) {
    currentMode = (currentMode == "borrow") ? "return" : "borrow";
    updateLEDs();
    beep();
    delay(300);
  }
  lastBtn = nowBtn;

  if (nfcReady && millis() - lastScan > cooldown) {
    uint8_t uid[7]; uint8_t uidLength;
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      String uidStr = "";
      for (uint8_t i = 0; i < uidLength; i++) {
        uidStr += String(uid[i], HEX);
      }
      lastUID = uidStr;
      postToPC(uidStr, currentMode);
      beep();
      lastScan = millis();
    }
  }
}
