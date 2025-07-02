#pragma once
#include <Arduino.h>
#define PN532_SCK  3
#define PN532_MISO 5
#define PN532_MOSI 7
#define PN532_SS 9
#define BUZZER_PIN 39
#define BLUE_LED_PIN 18
#define GREEN_LED_PIN 37
#define RED_LED_PIN 35
#define BLOCK_TO_USE 4

String generateUID(); 
String getTimeStamp();

void log_event(const String& uid, const String& action);
void setupFeedbackPins();
void feedbackSuccess();
void feedbackError();
void feedbackProcessing();
void feedbackIdle();
void feedbackReturnAccepted();
void runLEDTest();
void runBuzzerTest();