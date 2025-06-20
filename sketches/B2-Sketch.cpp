#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

// --- Serial communication with B1 ---
#define RXD2 16  // B2 RX from B1 TX
HardwareSerial B1Serial(1);

// --- Wi-Fi ---
const char* ssid = "@tinkeringwithPaulomon";
const char* password = "k41z3nj0y1!";

// --- Webhook Target ---
const String scriptURL = "https://dd4b7a8f-e408-4133-8a49-6d416d6c830f-00-36ddiurx0izrl.sisko.replit.dev/hook";
const String formURL   = "https://forms.gle/JMqC2oakcRS4HuhK6";

// --- Preferences ---
Preferences preferences;
String currentMode = "borrow";

// --- Web Server ---
AsyncWebServer server(80);

// --- UI Templates ---
#define MAIN_HTML_BUFFER_SIZE 2048

const char PROGMEM HTML_HEADER_STATIC[] = "<html><head><title>LabTrack NFC</title><style>"
"body {background-image: url('https://raw.githubusercontent.com/butterglazed-paulomon/pauzen/main/index1.png');"
"background-size: cover; font-family: sans-serif; text-align: center; color: white; margin: 0;}"
"button { font-size: 20px; padding: 10px 30px; margin: 10px; }"
".container { padding-top: 30px; background-color: rgba(0,0,0,0.5); min-height: 100vh; }"
"</style></head><body><div class='container'>"
"<h2>LabTrack NFC</h2>"
"<h3>Current Mode: <span style='color:yellow'>%s</span></h3>"
"<a href='/set?mode=borrow'><button>Borrow</button></a>"
"<a href='/set?mode=return'><button>Return</button></a>"
"<hr><h3>!Test Mode!</h3>"
"<form action='/simulate' method='GET'>"
"UID: <input type='text' name='uid'><br><br>"
"<input type='submit' name='action' value='Simulate Borrow'> "
"<input type='submit' name='action' value='Simulate Return'>"
"</form>"
"<p><a href='%s' target='_blank' style='color:lightblue'>Open Borrowing Form</a></p>"
"<br><img src='https://api.qrserver.com/v1/create-qr-code/?data=%s&size=150x150'>";
const char PROGMEM HTML_FOOTER_STATIC[] = "</div></body></html>";
const char PROGMEM HTML_SIMULATE_SUCCESS_FORMAT[] = "<html><body><h2>✅ Simulated %s for UID: %s</h2><a href='/'>Back</a></body></html>";
const char PROGMEM HTML_SET_MODE_STATIC[] = "<html><body><h2>Mode set</h2><script>setTimeout(()=>{window.location='/'},1000);</script></body></html>";

// --- Webhook Sending ---
void sendWebhook(const char* uid, const char* action) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Webhook: WiFi not connected");
    return;
  }

  char urlBuffer[256];
  snprintf(urlBuffer, sizeof(urlBuffer), "%s?uid=%s&action=%s", scriptURL.c_str(), uid, action);
  Serial.println("Webhook: " + String(urlBuffer));

  HTTPClient http;
  http.setReuse(false);
  http.begin(urlBuffer);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.printf("Webhook OK [%d]: %s\n", httpCode, payload.c_str());
  } else {
    Serial.printf("Webhook Error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

// --- Web UI Setup ---
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    char html[MAIN_HTML_BUFFER_SIZE];
    snprintf_P(html, sizeof(html), PSTR(HTML_HEADER_STATIC), currentMode.c_str(), formURL.c_str(), formURL.c_str());
    strlcat_P(html, PSTR(HTML_FOOTER_STATIC), sizeof(html));
    request->send(200, "text/html", html);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      String newMode = request->getParam("mode")->value();
      if (newMode == "borrow" || newMode == "return") {
        currentMode = newMode;
        preferences.begin("app_config", false);
        preferences.putString("mode", currentMode);
        preferences.end();
        Serial.println("Mode changed to: " + currentMode);
      }
    }
    request->send_P(200, "text/html", PSTR(HTML_SET_MODE_STATIC));
  });

  server.on("/simulate", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("uid") && request->hasParam("action")) {
      String uid = request->getParam("uid")->value();
      String action = request->getParam("action")->value();
      action.toLowerCase();
      if (action == "simulate borrow") action = "borrow";
      if (action == "simulate return") action = "return";

      sendWebhook(uid.c_str(), action.c_str());

      char response[200];
      snprintf_P(response, sizeof(response), PSTR(HTML_SIMULATE_SUCCESS_FORMAT), action.c_str(), uid.c_str());
      request->send(200, "text/html", response);
    } else {
      request->send(400, "text/plain", "Missing uid or action");
    }
  });

  server.begin();
  Serial.println("Web server started.");
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  B1Serial.begin(9600, SERIAL_8N1, RXD2, -1);

  preferences.begin("app_config", true);
  currentMode = preferences.getString("mode", "borrow");
  preferences.end();
  Serial.println("Loaded mode: " + currentMode);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  setupWebServer();
}

// --- Main Loop ---
void loop() {
  if (B1Serial.available()) {
    String line = B1Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    int sep = line.indexOf('|');
    if (sep > 0) {
      String uid = line.substring(0, sep);
      String action = line.substring(sep + 1);
      action.toLowerCase();
      Serial.printf("From B1 ➜ UID: %s | Action: %s\n", uid.c_str(), action.c_str());
      sendWebhook(uid.c_str(), action.c_str());
    }
  }

  delay(100);
}
