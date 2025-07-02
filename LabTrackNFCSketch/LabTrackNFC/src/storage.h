#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <queue>

struct Transaction {
  String timestamp;
  String uid;
  String student_email;
  String prof_email;
  String group_members;
  String items;
  String status;
  bool returned;
};

bool saveTransaction(const Transaction& tx);
std::vector<Transaction> loadTransactions();
bool updateTransactionStatus(String uid, String newStatus, bool markReturned);
bool acceptReturn(const String& uid);
bool rejectReturn(const String& uid);
bool findTransactionByUID(const String& uid, Transaction& foundTx);
bool savePendingQueue(const std::queue<String>& queue);
bool loadPendingQueue(std::queue<String>& queue);

