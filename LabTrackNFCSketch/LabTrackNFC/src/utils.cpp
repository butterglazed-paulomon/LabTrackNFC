#include <Arduino.h>            
#include "USB.h"
#define Serial USBSerial
#include "utils.h"
#include <ctime>
#include <FS.h>
#include <LittleFS.h>

String generateUID() {
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String uid = "";
  for (int i = 0; i < 6; i++) {
    uid += charset[random(0, sizeof(charset) - 1)];
  }
  return uid;
}

String getTimeStamp() {
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "Unknown";
    }
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

void log_event(const String& uid, const String& action) {
    File file = LittleFS.open("/transactions.log", FILE_APPEND);
    if (file) {
        file.printf("[%s] %s → %s\n", getTimeStamp().c_str(), uid.c_str(), action.c_str());
        file.close();
    }
}
void setupFeedbackPins() {
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
}

void beep(int duration = 100) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
}

void feedbackSuccess() {
    digitalWrite(GREEN_LED_PIN, HIGH);
    beep(100);
    delay(2000);
    digitalWrite(GREEN_LED_PIN, LOW);
}

void feedbackError() {
    digitalWrite(RED_LED_PIN, HIGH);
    beep(500);
    delay(2000);
    digitalWrite(RED_LED_PIN, LOW);
}

void feedbackProcessing() {
    for (int i = 0; i < 4; i++) {
        digitalWrite(GREEN_LED_PIN, i % 2 == 0);
        digitalWrite(RED_LED_PIN, i % 2 == 1);
        beep(100);
        delay(200);
    }
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
}

void feedbackIdle() {
    digitalWrite(BLUE_LED_PIN, HIGH);
    delay(100);
    digitalWrite(BLUE_LED_PIN, LOW);
    delay(900);
}
void feedbackReturnAccepted() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(GREEN_LED_PIN, HIGH);
        beep(80);
        delay(100);
        digitalWrite(GREEN_LED_PIN, LOW);
        delay(100);
    }
}

void runLEDTest() {
    digitalWrite(BLUE_LED_PIN, HIGH);
    delay(300);
    digitalWrite(BLUE_LED_PIN, LOW);
    delay(200);

    digitalWrite(RED_LED_PIN, HIGH);
    delay(300);
    digitalWrite(RED_LED_PIN, LOW);
    delay(200);

    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(300);
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(200);
}

void runBuzzerTest() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
    }
}
