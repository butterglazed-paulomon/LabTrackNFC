#pragma once
#include <Arduino.h>

void setupNFC();
void checkNFC();

bool isTagBlank();
bool writeUIDToTag(const String& uid);
bool wipeTag();
void handleCardTap();
int sendWebhook(const String&, const String&);

String manualReadNFC();
bool manualWriteNFC(const String& content);
bool manualWipeNFC();
