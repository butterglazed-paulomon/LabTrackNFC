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

// Mode tracking
String currentMode = "borrow";

// URLs
const String scriptURL = "https://script.google.com/macros/s/AKfycbyhdC1DBCO8XCf3dBgMK1IBgta4-ocV8fh_wNemzRBc68YWaKZnbFnh9HKeYfbxex8-/exec";
const String formURL = "https://forms.gle/JMqC2oakcRS4HuhK6";

void setupWebServer();
void updateLEDs();
void writeTag(String uid);
String readTag();
void clearTag();
void sendWebhook(String uid, String action);
String generateUID();

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
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532");
  } else {
    nfc.SAMConfig();
    Serial.println("PN532 NFC ready");
  }

  setupWebServer();
  updateLEDs();
}

void loop() {
  static bool lastButton = HIGH;
  bool nowButton = digitalRead(BUTTON_PIN);
  if (lastButton == HIGH && nowButton == LOW) {
    currentMode = (currentMode == "borrow") ? "return" : "borrow";
    Serial.println("Toggled via button: " + currentMode);
    updateLEDs();
    delay(500); // debounce
  }
  lastButton = nowButton;

  uint8_t success;
  uint8_t tagUid[7];
  uint8_t tagUidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, tagUid, &tagUidLength);

  if (!success) {
    delay(200);
    return;
  }

  if (currentMode == "borrow") {
    String uid = generateUID();
    writeTag(uid);
    sendWebhook(uid, "borrow");
  } else if (currentMode == "return") {
    String uid = readTag();
    if (uid != "") {
      sendWebhook(uid, "return");
      clearTag();
    }
  }

  delay(100);
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 10000) {
    Serial.println("Current mode: " + currentMode);
    lastLog = millis();
  }
}

void setupWebServer() {
  Serial.println("Setting up async web server...");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Serving root page. Current mode: " + currentMode);
    String html = "<html><head><title>Paulomon LabTrack NFC</title><style>";
    html += "body {background-image: url('https://raw.githubusercontent.com/butterglazed-paulomon/pauzen/main/index1.png');";
    html += "background-size: cover; background-repeat: no-repeat; background-position: center;";
    html += "font-family: sans-serif; text-align: center; color: white; margin: 0; padding: 0;}";
    html += "button { font-size: 20px; padding: 10px 30px; margin: 10px; }";
    html += ".container { padding-top: 30px; background-color: rgba(0,0,0,0.5); min-height: 100vh; }";
    html += "</style></head><body><div class='container'>";
    html += "<h2>LabTrack NFC</h2>";
    html += "<h3>Current Mode: <span style='color:yellow'>" + currentMode + "</span></h3>";
    html += "<a href='/set?mode=borrow'><button>Borrow</button></a>";
    html += "<a href='/set?mode=return'><button>Return</button></a>";
    html += "<p>Scan NFC tag after selecting mode.</p>";
    html += "<p><a href='" + formURL + "' target='_blank' style='color:lightblue'>Open Borrowing Form</a></p>";
    html += "<br><img src='https://api.qrserver.com/v1/create-qr-code/?data=" + formURL + "&size=150x150'>";
    html += "</div></body></html>";
    request->send(200, "text/html", html);
  });

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("mode")) {
        String modeParam = request->getParam("mode")->value();
        modeParam.toLowerCase();

        if (modeParam == "borrow" || modeParam == "return") {
          currentMode = modeParam;
          updateLEDs();
          Serial.println("Mode set via web to: " + currentMode);
        } else {
          Serial.println("Invalid mode received: " + modeParam);
        }
      } else {
        Serial.println("No mode param in request");
      }

      String html = "<html><body><h2>Set mode to " + currentMode + "</h2><script>setTimeout(()=>{window.location='/'},1000);</script></body></html>";
      request->send(200, "text/html", html);
    });

      server.begin();
      Serial.println("Async web server started");
      Serial.println("Mode manually set to: " + currentMode);
}

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
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = scriptURL + "?uid=" + uid + "&action=" + action;
    Serial.println("Sending: " + url);
    http.begin(url);
    int httpCode = http.GET();
    Serial.println("HTTP: " + String(httpCode));
    Serial.println("Body: " + http.getString());
    http.end();
  } else {
    Serial.println("WiFi not connected.");
  }
}

String generateUID() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot fetch UID");
    return "OFFLINE_UID";
  }

  HTTPClient http;
  String url = scriptURL + "?action=get_next_uid";
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String uid = http.getString();
    uid.trim();
    Serial.println("Fetched UID from sheet: " + uid);
    http.end();
    return uid;
  } else {
    Serial.println("Failed to get UID from server");
    http.end();
    return "FETCH_FAIL";
  }
}
