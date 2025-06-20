#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Pin Definitions
#define SDA_PIN        3     // GPIO3
#define SCL_PIN        5     // GPIO5
#define BUTTON_PIN     7     // GPIO7
#define LED_BORROW     9     // GPIO9 (Green LED)
#define LED_RETURN     10    // GPIO10 (Red LED)
#define BUZZER_PIN     13    // GPIO13

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

// Wi-Fi
const char* ssid = "@tinkeringwithPaulomon";
const char* password = "k41z3nj0y1!";

// B2 server
const char* serverHost = "http://192.168.1.100";  // B2's IP
const char* endpoint = "/receive";               // POST endpoint on B2

// State
String mode = "borrow";
bool lastButtonState = HIGH;

void updateIndicators() {
  digitalWrite(LED_BORROW, mode == "borrow");
  digitalWrite(LED_RETURN, mode == "return");
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BORROW, OUTPUT);
  pinMode(LED_RETURN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_BORROW, LOW);
  digitalWrite(LED_RETURN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  nfc.begin();
  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    Serial.println("PN532 not found");
    while (1) delay(1000);
  }
  nfc.SAMConfig();
  Serial.println("PN532 ready");

  updateIndicators();
}

void sendToB2(String uid, String action) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi, skipping send.");
    return;
  }

  HTTPClient http;
  String url = String(serverHost) + endpoint;
  http.begin(url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "uid=" + uid + "&action=" + action;
  int code = http.POST(body);

  Serial.printf("[HTTP] POST %s => Code: %d\n", url.c_str(), code);
  http.end();
}

void loop() {
  // Toggle Mode via Button
  bool buttonNow = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonNow == LOW) {
    mode = (mode == "borrow") ? "return" : "borrow";
    Serial.println("Mode toggled: " + mode);
    updateIndicators();
    delay(500);
  }
  lastButtonState = buttonNow;

  // Look for tag
  uint8_t success;
  uint8_t uid[7];
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  if (success) {
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      uidStr += String(uid[i], HEX);
    }
    uidStr.toUpperCase();
    Serial.println("Detected tag UID: " + uidStr);

    // Beep feedback
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);

    sendToB2(uidStr, mode);
    delay(1500);  // Debounce / scan delay
  }

  delay(100);
}
