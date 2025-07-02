#pragma once
#include <ESPAsyncWebServer.h>
#include "config.h"

extern AsyncWebServer server;
void setupWebServer(AsyncWebServer& server, Config& config);
