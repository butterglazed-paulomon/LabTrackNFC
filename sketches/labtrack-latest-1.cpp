#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

// NFC and LED pins
#define SDA_PIN 21
#define SCL_PIN 22
#define BUTTON_PIN 0
#define LED_BORROW 14
#define LED_RETURN 15

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
AsyncWebServer server(80);

// Wi-Fi credentials
const char* ssid = "@tinkeringwithPaulomon";
const char* password = "k41z3nj0y1!";

// URLs
const String scriptURL = "https://dd4b7a8f-e408-4133-8a49-6d416d6c830f-00-36ddiurx0izrl.sisko.replit.dev/hook";
const String formURL = "https://forms.gle/JMqC2oakcRS4HuhK6";

String currentMode = "borrow";

void updateLEDs() {
  digitalWrite(LED_BORROW, currentMode == "borrow" ? HIGH : LOW);
  digitalWrite(LED_RETURN, currentMode == "return" ? HIGH : LOW);
}

void writeTag(String uid) {
  uint8_t buffer[32] = {0};
  uid.getBytes(buffer, sizeof(buffer));
  for (int i = 0; i < 8; i++) {
    nfc.ntag2xx_WritePage(4 + i, &buffer[i * 4]);
  }
  Serial.println("Wrote UID to tag: " + uid);
}

String readTag() {
  char readBuf[33];
  for (int i = 0; i < 8; i++) {
    uint8_t page[4];
    nfc.ntag2xx_ReadPage(4 + i, page);
    memcpy(&readBuf[i * 4], page, 4);
  }
  readBuf[32] = '\0';
  String uid = String(readBuf);
  uid.trim();
  Serial.println("Read UID: " + uid);
  return uid;
}

void clearTag() {
  uint8_t empty[4] = {0, 0, 0, 0};
  for (int i = 0; i < 8; i++) {
    nfc.ntag2xx_WritePage(4 + i, empty);
  }
  Serial.println("Tag cleared");
}

void sendWebhook(String uid, String action) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Webhook] WiFi not connected");
    return;
  }

  HTTPClient http;
  String fullURL = scriptURL + "?uid=" + uid + "&action=" + action;
  Serial.println("[Webhook] Connecting to: " + fullURL);

  http.setReuse(false);
  http.begin(fullURL);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.printf("[Webhook] Response code: %d\n", httpCode);
    String payload = http.getString();
    Serial.println("[Webhook] Response body: " + payload);
  } else {
    Serial.printf("[Webhook] GET failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  delay(200);  // Avoid heap pile-up
}

String generateUID() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot fetch UID");
    return "OFFLINE_UID";
  }

  HTTPClient http;
  String url = scriptURL + "?action=get_next_uid";
  http.setReuse(false);
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String uid = http.getString();
    uid.trim();
    Serial.println("Fetched UID: " + uid);
    http.end();
    return uid;
  } else {
    Serial.println("Failed to get UID");
    http.end();
    return "FETCH_FAIL";
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><title>LabTrack NFC</title><style>";
    html += "body {background-image: url('https://raw.githubusercontent.com/butterglazed-paulomon/pauzen/main/index1.png');";
    html += "background-size: cover; font-family: sans-serif; text-align: center; color: white; margin: 0;}";
    html += "button { font-size: 20px; padding: 10px 30px; margin: 10px; }";
    html += ".container { padding-top: 30px; background-color: rgba(0,0,0,0.5); min-height: 100vh; }";
    html += "</style></head><body><div class='container'>";
    html += "<h2>LabTrack NFC</h2>";
    html += "<h3>Current Mode: <span style='color:yellow'>" + currentMode + "</span></h3>";
    html += "<a href='/set?mode=borrow'><button>Borrow</button></a>";
    html += "<a href='/set?mode=return'><button>Return</button></a>";
    html += "<hr><h3>!Test Mode!</h3>";
    html += "<form action='/simulate' method='GET'>";
    html += "UID: <input type='text' name='uid'><br><br>";
    html += "<input type='submit' name='action' value='Simulate Borrow'> ";
    html += "<input type='submit' name='action' value='Simulate Return'>";
    html += "</form>";
    html += "<p><a href='" + formURL + "' target='_blank' style='color:lightblue'>Open Borrowing Form</a></p>";
    html += "<br><img src='https://api.qrserver.com/v1/create-qr-code/?data=" + formURL + "&size=150x150'>";
    html += "</div></body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/simulate", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("uid") && request->hasParam("action")) {
      String uid = request->getParam("uid")->value();
      String action = request->getParam("action")->value();
      action.toLowerCase();
      if (action == "simulate borrow") action = "borrow";
      if (action == "simulate return") action = "return";
      sendWebhook(uid, action);
      String html = "<html><body><h2>✅ Simulated " + action + " for UID: " + uid + "</h2><a href='/'>Back</a></body></html>";
      request->send(200, "text/html", html);
    } else {
      request->send(400, "text/plain", "Missing uid or action.");
    }
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      String modeParam = request->getParam("mode")->value();
      if (modeParam == "borrow" || modeParam == "return") {
        currentMode = modeParam;
        updateLEDs();
        Serial.println("Mode set: " + currentMode);
      }
    }
    request->send(200, "text/html", "<html><body><h2>Mode set</h2><script>setTimeout(()=>{window.location='/'},1000);</script></body></html>");
  });

  server.begin();
  Serial.println("Async web server started!");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BORROW, OUTPUT);
  pinMode(LED_RETURN, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println("PN532 not found");
  } else {
    nfc.SAMConfig();
    Serial.println("PN532 ready");
  }

  setupWebServer();
  updateLEDs();
}

void loop() {
  static bool lastButton = HIGH;
  bool nowButton = digitalRead(BUTTON_PIN);
  if (lastButton == HIGH && nowButton == LOW) {
    currentMode = (currentMode == "borrow") ? "return" : "borrow";
    updateLEDs();
    delay(300);
  }
  lastButton = nowButton;

  uint8_t success;
  uint8_t tagUid[7];
  uint8_t tagUidLength;
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tagUid, &tagUidLength);
  if (!success) {
    delay(300);
    return;
  }

  if (currentMode == "borrow") {
    String uid = generateUID();
    if (uid.length() < 4 || uid == "FETCH_FAIL") return;
    writeTag(uid);
    sendWebhook(uid, "borrow");
  } else if (currentMode == "return") {
    String uid = readTag();
    if (uid.length() < 4) return;
    sendWebhook(uid, "return");
    clearTag();
  }

  Serial.printf("[Memory] Heap: %u\n", ESP.getFreeHeap());
  delay(200);
}
