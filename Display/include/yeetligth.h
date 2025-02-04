#ifndef YEETLIGHT_H
#define YEETLIGHT_H

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Constants
extern const int COLOR_TEMP_WARM;
extern const int FADE_DURATION;
extern const int MIN_BRIGHTNESS;
extern const int MAX_BRIGHTNESS;
extern const int LIGHT_ON_DURATION;
extern const char* LAMP_IP;
extern const uint16_t LAMP_PORT;

// Global variables
extern bool lightManualControl;
extern bool blindManualControl;
extern bool lightIsOn;
extern time_t lightOnTime;
extern WiFiClient lampClient;

// Function declarations
bool setYeelightBrightness(int brightness);
bool setYeelightColor(int r, int g, int b);
bool sendYeelightCommand(const char* method, const char* param);

#endif