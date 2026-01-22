#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "config.h"
#include "storage.h"
#include "utils.h"
#include <LittleFS.h>
#include "nfc.h"
#include <queue>
#include "labstaff_css.h"
#include "borrowform_css.h"

extern Config config;
extern String currentPendingUID;

void sendBorrowWebhook(const Transaction &tx) {
    WiFiClient client;
    HTTPClient http;

    String url = "http://" + config.flask_ip + ":8000/receive";
    http.begin(client, url);

    ArduinoJson::JsonDocument doc;
    doc["type"] = "borrow";
    doc["timestamp"] = tx.timestamp;
    doc["uid"] = tx.uid;
    doc["student_email"] = tx.student_email;
    doc["prof_email"] = tx.prof_email;
    doc["group_members"] = tx.group_members;
    doc["items"] = tx.items;

    String payload;
    serializeJson(doc, payload);

    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(payload);
    Serial.printf("[Webhook] POST borrow → %d\n", httpResponseCode);
    http.end();
}

void setupWebServer(AsyncWebServer &server, Config &config) {
    // HTML routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/borrowform.html", "text/html");
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/config.html", "text/html");
    });

    server.on("/labstaff", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/labstaff.html", "text/html");
    });

    server.on("/utility", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/utility.html", "text/html");
    });

    // Utility routes
    server.on("/utility/test-led", HTTP_POST, [](AsyncWebServerRequest *request) {
        runLEDTest();
        request->send(200, "application/json", "{\"message\":\"LED test triggered\"}");
    });

    server.on("/utility/test-buzzer", HTTP_POST, [](AsyncWebServerRequest *request) {
        runBuzzerTest();
        request->send(200, "application/json", "{\"message\":\"Buzzer test triggered\"}");
    });

    server.on("/utility/read", HTTP_POST, [](AsyncWebServerRequest *request) {
        String result = manualReadNFC();
        StaticJsonDocument<128> doc;
        doc["content"] = result;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/utility/write", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data);
            if (error || !doc.containsKey("content")) {
                request->send(400, "application/json", "{\"status\":\"Invalid JSON or missing content\"}");
                return;
            }

            bool success = manualWriteNFC(doc["content"]);
            request->send(200, "application/json", success ?
                "{\"status\":\"Write successful\"}" : "{\"status\":\"Write failed\"}");
        });

    server.on("/utility/wipe", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = manualWipeNFC();
        request->send(200, "application/json", success ?
            "{\"status\":\"Wipe successful\"}" : "{\"status\":\"Wipe failed\"}");
    });

    // Transaction generator
    server.on("/generate", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<1024> doc;
            DeserializationError error = deserializeJson(doc, data);
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            String studentEmail = doc["student_email"];
            String profEmail = doc["prof_email"];
            String members = doc["group_members"];
            String items = doc["items"];

            String generatedUID = generateUID();
            Transaction tx{getTimeStamp(), generatedUID, studentEmail, profEmail, members, items, "borrowed", false};
            saveTransaction(tx);

            extern std::queue<String> pendingQueue;
            pendingQueue.push(tx.uid);
            savePendingQueue(pendingQueue);

            sendBorrowWebhook(tx);

            StaticJsonDocument<256> res;
            res["success"] = true;
            res["uid"] = tx.uid;
            String response;
            serializeJson(res, response);
            request->send(200, "application/json", response);
        });

    // Load records
    server.on("/records", HTTP_GET, [](AsyncWebServerRequest *request) {
        auto transactions = loadTransactions();
        StaticJsonDocument<2048> doc;
        JsonArray arr = doc.to<JsonArray>();

        for (const auto &tx : transactions) {
            JsonObject obj = arr.createNestedObject();
            obj["timestamp"] = tx.timestamp;
            obj["student_email"] = tx.student_email;
            obj["prof_email"] = tx.prof_email;
            obj["group_members"] = tx.group_members;
            obj["items"] = tx.items;
            obj["uid"] = tx.uid;
            obj["status"] = tx.status;
        }

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Save config
    server.on("/save-config", HTTP_POST, [&config](AsyncWebServerRequest *request) {}, nullptr,
        [&config](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, data);

            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            config.wifi_ssid = doc["ssid"] | "";
            config.wifi_password = doc["password"] | "";
            config.flask_ip = doc["flask_ip"] | "";

            if (saveConfig(config)) {
                request->send(200, "text/plain", "Config saved. Rebooting...");
                delay(2000);
                ESP.restart();
            } else {
                request->send(500, "text/plain", "Failed to save config.");
            }
        });

    // Manual erase route for compatibility
    server.on("/erase", HTTP_POST, [&](AsyncWebServerRequest *request) {}, nullptr,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data);
            if (error) {
                request->send(400, "application/json", "{\"status\":\"Invalid JSON\"}");
                return;
            }

            String uid = doc["uid"] | "";
            if (uid == "") {
                request->send(400, "application/json", "{\"status\":\"Missing UID\"}");
                return;
            }

            bool erased = wipeTag(); // full wipe
            request->send(erased ? 200 : 500, "application/json",
                          erased ? "{\"status\":\"Tag erased\"}" : "{\"status\":\"Erase failed\"}");
        });

    // Static files & JSON config
    server.serveStatic("/", LittleFS, "/").setDefaultFile("borrowform.html");

    server.on("/config.json", HTTP_GET, [&config](AsyncWebServerRequest *request) {
        StaticJsonDocument<128> doc;
        doc["flask_ip"] = config.flask_ip;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/return/accept", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("[WEB] Received /return/accept");

    if (currentPendingUID != "") {
        if (wipeTag()) {
            feedbackReturnAccepted();  
            currentPendingUID = "";
            request->send(200, "text/plain", "ACCEPTED");
        } else {
            feedbackError();  // if wipe fails
            request->send(500, "text/plain", "TAG_WIPE_FAILED");
        }
    } else {
        request->send(400, "text/plain", "NO_PENDING_UID");
    }
});

    server.on("/return/reject", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.println("[WEB] Received /return/reject");

        if (currentPendingUID != "") {
            feedbackError();  // 
            currentPendingUID = "";
            request->send(200, "text/plain", "REJECTED");
        } else {
            request->send(400, "text/plain", "NO_PENDING_UID");
        }
    });

    server.on("/labstaff.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", labstaff_css);
    });

    server.on("/borrowform.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/css", borrowform_css);
    });

}
