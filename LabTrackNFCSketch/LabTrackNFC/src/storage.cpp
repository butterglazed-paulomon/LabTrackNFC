#include "USB.h"
#define Serial USBSerial
#include "storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <queue>
#define PENDING_QUEUE_FILE "/pending_queue.json"
#define LOG_FILE "/transactions.txt"

bool saveTransaction(const Transaction& tx) {
  File file = LittleFS.open(LOG_FILE, FILE_APPEND);
  if (!file) return false;

  DynamicJsonDocument doc(512);
  doc["timestamp"] = tx.timestamp;
  doc["uid"] = tx.uid;
  doc["student_email"] = tx.student_email;
  doc["prof_email"] = tx.prof_email;
  doc["group_members"] = tx.group_members;
  doc["items"] = tx.items;
  doc["status"] = tx.status;
  doc["returned"] = tx.returned;

  serializeJson(doc, file);
  file.println();
  file.close();
  return true;
}

std::vector<Transaction> loadTransactions() {
  std::vector<Transaction> list;
  File file = LittleFS.open(LOG_FILE, "r");
  if (!file) return list;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, line);
    if (err) continue;

    Transaction tx;
    tx.timestamp = doc["timestamp"].as<String>();
    tx.uid = doc["uid"].as<String>();
    tx.student_email = doc["student_email"].as<String>();
    tx.prof_email = doc["prof_email"].as<String>();
    tx.group_members = doc["group_members"].as<String>();
    tx.items = doc["items"].as<String>();
    tx.status = doc["status"].as<String>();
    tx.returned = doc["returned"];
    list.push_back(tx);
  }
  file.close();
  return list;
}

bool findTransactionByUID(const String& uid, Transaction& foundTx) {
    auto all = loadTransactions();
    for (const auto& tx : all) {
        if (tx.uid == uid) {
            foundTx = tx;
            return true;
        }
    }
    return false;
}


bool updateTransactionStatus(String uid, String newStatus, bool markReturned) {
  auto all = loadTransactions();
  File file = LittleFS.open(LOG_FILE, "w"); // overwrite
  if (!file) return false;

  for (auto& tx : all) {
    if (tx.uid == uid) {
      tx.status = newStatus;
      tx.returned = markReturned;
    }

    DynamicJsonDocument doc(512);
    doc["timestamp"] = tx.timestamp;
    doc["uid"] = tx.uid;
    doc["student_email"] = tx.student_email;
    doc["prof_email"] = tx.prof_email;
    doc["group_members"] = tx.group_members;
    doc["items"] = tx.items;
    doc["status"] = tx.status;
    doc["returned"] = tx.returned;

    serializeJson(doc, file);
    file.println();
  }
  file.close();
  return true;
}
bool acceptReturn(const String& uid) {
    // TODO: Search for transaction by UID and update status to "returned"
    // Optional: erase NFC tag here if hardware logic is involved
    return true;
}

bool rejectReturn(const String& uid) {
    // TODO: Update status to "rejected"
    return true;
}



bool savePendingQueue(const std::queue<String>& queue) {
    File file = LittleFS.open(PENDING_QUEUE_FILE, "w");
    if (!file) return false;

    DynamicJsonDocument doc(1024);
    JsonArray arr = doc.to<JsonArray>();

    std::queue<String> temp = queue;
    while (!temp.empty()) {
        arr.add(temp.front());
        temp.pop();
    }

    serializeJson(doc, file);
    file.close();
    return true;
}

bool loadPendingQueue(std::queue<String>& queue) {
    File file = LittleFS.open(PENDING_QUEUE_FILE, "r");
    if (!file) return false;

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return false;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant v : arr) {
        queue.push(v.as<String>());
    }
    return true;
}