#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// Pin Definitions
#define SDA_PIN 21
#define SCL_PIN 22
#define BUTTON_PIN 0
#define LED_BORROW 14
#define LED_RETURN 15
#define BUZZER_PIN 13

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
AsyncWebServer server(80);

const char* ssid = "@tinkeringwithPaulomon";
const char* password = "k41z3nj0y1!";
const String formURL = "https://forms.gle/JMqC2oakcRS4HuhK6";

String currentMode = "borrow";
bool nfcReady = false;

void updateIndicators() {
  digitalWrite(LED_BORROW, currentMode == "borrow" ? HIGH : LOW);
  digitalWrite(LED_RETURN, currentMode == "return" ? HIGH : LOW);
  tone(BUZZER_PIN, 1200, 100);  // Short beep
}

// Simulated UID for testing
String simulateUID() {
  return currentMode + "_TEST_UID";
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><title>LabTrack NFC</title><style>";
    html += "body {background-image: url('https://raw.githubusercontent.com/butterglazed-paulomon/pauzen/main/index1.png');";
    html += "background-size: cover; background-repeat: no-repeat; background-position: center;";
    html += "font-family: sans-serif; text-align: center; color: white; margin: 0; padding: 0;}";
    html += "button { font-size: 20px; padding: 10px 30px; margin: 10px; }";
    html += ".container { padding-top: 30px; background-color: rgba(0,0,0,0.5); min-height: 100vh; }";
    html += "</style></head><body><div class='container'>";
    html += "<h2>LabTrack NFC</h2>";
    html += "<h3>Current Mode: <span style='color:yellow'>" + currentMode + "</span></h3>";
    html += "<p>NFC Reader Status: <span style='color:" + String(nfcReady ? "lightgreen" : "red") + "'>" + (nfcReady ? "Ready" : "Not Found") + "</span></p>";
    html += "<a href='/set?mode=borrow'><button>Borrow</button></a>";
    html += "<a href='/set?mode=return'><button>Return</button></a>";
    html += "<hr><h3>!Test Mode!</h3>";
    html += "<form action='/simulate' method='GET'>";
    html += "UID: <input type='text' name='uid' placeholder='Enter UID'><br><br>";
    html += "<input type='submit' name='action' value='Simulate Borrow'> ";
    html += "<input type='submit' name='action' value='Simulate Return'>";
    html += "</form>";
    html += "<p><a href='" + formURL + "' target='_blank' style='color:lightblue'>Open Borrowing Form</a></p>";
    html += "<br><img src='https://api.qrserver.com/v1/create-qr-code/?data=" + formURL + "&size=150x150'>";
    html += "</div></body></html>";

    request->send(200, "text/html", html);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      String mode = request->getParam("mode")->value();
      if (mode == "borrow" || mode == "return") {
        currentMode = mode;
        updateIndicators();
        Serial.println("Web → Mode switched to: " + currentMode);
      }
    }
    request->redirect("/");
  });

  server.on("/simulate", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("uid") && request->hasParam("action")) {
      String uid = request->getParam("uid")->value();
      String action = request->getParam("action")->value();
      action.toLowerCase();

      if (action == "simulate borrow") action = "borrow";
      if (action == "simulate return") action = "return";

      Serial.println("[Simulate] " + action + " for UID: " + uid);
      tone(BUZZER_PIN, 1500, 200);
      request->send(200, "text/html", "<html><body><h2>✅ Simulated " + action + " for UID: " + uid + "</h2><a href='/'>Back</a></body></html>");
    } else {
      request->send(400, "text/plain", "Missing uid or action.");
    }
  });

  server.begin();
  Serial.println("Web server started!");
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BORROW, OUTPUT);
  pinMode(LED_RETURN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // NFC Setup
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("⚠️  PN532 not found!");
    nfcReady = false;
  } else {
    nfc.SAMConfig();
    nfcReady = true;
    Serial.println("✅ PN532 NFC ready");
  }

  setupWebServer();
  updateIndicators();
}

void loop() {
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && buttonState == LOW) {
    currentMode = (currentMode == "borrow") ? "return" : "borrow";
    updateIndicators();
    Serial.println("Button → Toggled mode to: " + currentMode);
    delay(300);
  }
  lastButtonState = buttonState;

  delay(100);
}
