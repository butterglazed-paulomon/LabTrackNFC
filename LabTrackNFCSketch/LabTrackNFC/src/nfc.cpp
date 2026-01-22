#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <Adafruit_PN532.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include "nfc.h"
#include <LittleFS.h>
#include <queue>
#define BLOCK_TO_USE 4 


extern Config config;
std::queue<String> pendingQueue;
unsigned long lastWriteTime = 0;
const unsigned long writeDebounceDelay = 2000; // 2 seconds
bool hasWrittenToCurrentCard = false;
String lastCardID = "";



Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

uint8_t currentUid[7];
uint8_t currentUidLength = 0;

uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };


// --- NFC Setup ---
void setupNFC() {
    nfc.begin();
    while (!nfc.getFirmwareVersion()) {
        Serial.println("[WARN] PN532 not detected. Retrying in 3 seconds...");
        delay(3000);  // wait 3 seconds before retrying
        nfc.begin();  // try to initialize again
    }
    nfc.SAMConfig();
    Serial.println("[INFO] PN532 initialized.");

    setupFeedbackPins();
}

// --- NFC Read/Write ---
bool authenticateBlock(uint8_t block) {
    return nfc.mifareclassic_AuthenticateBlock(currentUid, currentUidLength, block, 0, keya);
}

bool isTagBlank() {
    if (!authenticateBlock(BLOCK_TO_USE)) return false;

    uint8_t buffer[16];
    if (!nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, buffer)) return false;

    for (int i = 0; i < 16; i++) {
        if (buffer[i] != 0x00) return false;
    }
    return true;
}

bool writeUIDToTag(const String& uid) {
    //nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLength);
    if (!authenticateBlock(BLOCK_TO_USE)) return false;

    uint8_t data[16] = { 0 };
    for (int i = 0; i < uid.length() && i < 16; i++) {
        data[i] = uid[i];
    }

    //return nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, data);
    bool success = nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, data);
    if (!success) {
        Serial.println("[ERROR] WriteDataBlock failed.");
    } else {
        Serial.println("[WRITE] UID written to tag successfully.");
    }
    return success;
}

String readUIDFromTag() {
    if (!authenticateBlock(BLOCK_TO_USE)) return "";

    uint8_t buffer[16];
    if (!nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, buffer)) return "";

    String uid = "";
    for (int i = 0; i < 16; i++) {
        if (buffer[i] == 0x00) break;
        uid += (char)buffer[i];
    }
    return uid;
}

bool wipeTag() {
    digitalWrite(RED_LED_PIN, HIGH); 

    bool allSuccess = true;
    for (uint8_t block = 4; block < 64; block++) {
        if ((block + 1) % 4 == 0) continue;

        if (!nfc.mifareclassic_AuthenticateBlock(currentUid, currentUidLength, block, 0, keya)) {
            Serial.printf("[ERROR] Auth failed for block %d\n", block);
            allSuccess = false;
            continue;
        }

        uint8_t blank[16] = {0};
        if (!nfc.mifareclassic_WriteDataBlock(block, blank)) {
            Serial.printf("[ERROR] Write failed for block %d\n", block);
            allSuccess = false;
        } else {
            Serial.printf("[WIPE] Block %d wiped.\n", block);
        }
    }

    digitalWrite(RED_LED_PIN, LOW);

    if (allSuccess) {
        // Blink green 3x to indicate success
        for (int i = 0; i < 3; i++) {
            digitalWrite(GREEN_LED_PIN, HIGH);
            delay(200);
            digitalWrite(GREEN_LED_PIN, LOW);
            delay(200);
        }
    } else {
        feedbackError();  // Red LED + tone
    }

    return allSuccess;
}



// --- Webhook ---
int sendWebhook(const String& uid, const String& actionType, String& responseBody) {
    WiFiClient client;
    HTTPClient http;

    String url = "http://" + config.flask_ip + ":8000/receive";
    http.begin(client, url);

    ArduinoJson::StaticJsonDocument<256> doc;
    doc["uid"] = uid;
    doc["type"] = actionType;

    String payload;
    serializeJson(doc, payload);

    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);

    responseBody = http.getString();
    Serial.printf("[Webhook] POST %s → %d → %s\n", actionType.c_str(), code, responseBody.c_str());

    http.end();
    return code;
}

void handleCardTap() {
    String hwUID = lastCardID;
    Serial.print("[DEBUG] Card detected. HW UID: ");
    Serial.println(hwUID);
    unsigned long now = millis();
    if (hwUID == lastCardID && (now - lastWriteTime < writeDebounceDelay)) {
        if (isTagBlank()) {
            Serial.println("[DEBOUNCE] Ignoring duplicate card tap for blank tag.");
            return;
        } else {
            Serial.println("[INFO] Tag already has UID. Proceeding with confirm return.");
        }
    }

    lastCardID = hwUID;
    lastWriteTime = now;
    if (isTagBlank()) {
        Serial.println("[NFC] Blank tag detected. Writing UID...");

        if (pendingQueue.empty()) {
            Serial.println("[ERROR] No pending UIDs in queue.");
            feedbackError();
            return;
        }

        String newUID = pendingQueue.front();
        //pendingQueue.pop();
        //savePendingQueue(pendingQueue);

        Serial.print("[DEBUG] Writing to NFC UID: ");
        Serial.println(newUID);
        if (!nfc.mifareclassic_IsFirstBlock(BLOCK_TO_USE)) {
            Serial.println("[WARN] Target block may be a sector trailer. Aborting write.");
            feedbackError();
            return;
        }

        if (writeUIDToTag(newUID)) {
            hasWrittenToCurrentCard = true;
            lastWriteTime = now;
            feedbackSuccess();
            pendingQueue.pop();
            savePendingQueue(pendingQueue);
            String response;
            sendWebhook(newUID, "confirm_borrow", response);
        } else {
            feedbackError();
        }
        //hasWrittenToCurrentCard = false;
        return;
    }

    String tagUID = readUIDFromTag();
    Serial.print("[DEBUG] Scanned Tag UID (from memory): ");
    Serial.println(tagUID);

    Transaction tx;
    if (!findTransactionByUID(tagUID, tx)) {
        Serial.println("[WARN] Unknown UID. No matching transaction found.");
        log_event(tagUID, "unrecognized_uid");
        feedbackError();
        return;
    }

    Serial.println("[INFO] Valid UID found. Checking return...");

    String response;
    int code = sendWebhook(tagUID, "confirm_return", response);

    if (code == 200) {
        StaticJsonDocument<128> resDoc;
        DeserializationError err = deserializeJson(resDoc, response);

        if (!err) {
            String status = resDoc["status"] | "";

            if (status == "approved") {
                Serial.println("[INFO] Return approved by staff.");
                if (wipeTag()) {
                    feedbackReturnAccepted();
                } else {
                    Serial.println("[ERROR] Failed to wipe tag.");
                    feedbackError();
                }
            } else if (status == "pending") {
                Serial.println("[INFO] Return pending staff approval.");
                feedbackProcessing();
                delay(1000);
            } else if (status == "rejected") {
                Serial.println("[WARN] Return rejected by staff.");
                feedbackError();
                delay(1000);
            } else {
                Serial.println("[WARN] Unrecognized status: " + status);
                feedbackError();
            }
        } else {
            Serial.println("[ERROR] Failed to parse JSON response.");
            feedbackError();
        }
    } else {
        Serial.println("[ERROR] Server error or timeout.");
        feedbackError();
    }
    Serial.printf("[QUEUE] %d UIDs left in queue.\n", pendingQueue.size());
}


void checkNFC() {
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &currentUidLength)) {
        static bool lastCardPresent = false;
        if (lastCardPresent) {
            hasWrittenToCurrentCard = false;
        }
        lastCardPresent = true;
        digitalWrite(BLUE_LED_PIN, LOW); // Turn on blue LED to indicate card detected
        Serial.print("[DEBUG] Card detected. HW UID: ");
        for (uint8_t i = 0; i < currentUidLength; i++) {
            Serial.print(currentUid[i], HEX);
        }
        Serial.println();
        handleCardTap();
        digitalWrite(BLUE_LED_PIN, HIGH);
        delay(1000); 
        lastCardPresent = false;
    }
}


String manualReadNFC() {
    Serial.println("[NFC] manualReadNFC() called");
    Serial.println("Tag detected!");
    uint8_t uid[] = { 0 };
    uint8_t uidLength;

    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
        return "No tag detected.";
    }

    uint8_t data[16];
    if (nfc.mifareclassic_ReadDataBlock(BLOCK_TO_USE, data)) {
        String result;
        for (int i = 0; i < 16; i++) {
            result += (char)data[i];
        }
        return "Read: " + result;
    } else {
        return "Read failed.";
    }
}

bool manualWriteNFC(const String& content) {
    Serial.println("[NFC] manualWriteNFC() called");
    uint8_t uid[] = { 0 };
    uint8_t uidLength;

    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
        return false;
    }

    uint8_t buffer[16];
    memset(buffer, ' ', 16);
    int len = min(16U, content.length());
    memcpy(buffer, content.c_str(), len);

    return nfc.mifareclassic_WriteDataBlock(BLOCK_TO_USE, buffer);
}

bool manualWipeNFC() {
    Serial.println("[NFC] manualWipeNFC() called");
    return wipeTag();
}

