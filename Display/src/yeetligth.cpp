#include "yeetligth.h"

// Define constants
const int COLOR_TEMP_WARM = 3000;
const int FADE_DURATION = 2000;
const int MIN_BRIGHTNESS = 1;
const int MAX_BRIGHTNESS = 20;
const int LIGHT_ON_DURATION = 3600;
const char* LAMP_IP = "192.168.1.183";
const uint16_t LAMP_PORT = 55443;

// Define global variables
bool lightManualControl = false;
bool blindManualControl = false;
bool lightIsOn = false;
time_t lightOnTime = 0;
WiFiClient lampClient;

bool setYeelightBrightness(int brightness) {
    if (!lampClient.connected()) {
        if (!lampClient.connect(LAMP_IP, LAMP_PORT)) {
            Serial.println("Failed to connect to Yeelight");
            return false;
        }
        delay(100);
    }

    JsonDocument doc;
    doc["id"] = 1;
    doc["method"] = "set_bright";
    
    JsonArray params = doc["params"].to<JsonArray>();
    params.add(constrain(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS));
    params.add("smooth");
    params.add(FADE_DURATION);  // Use longer fade duration

    String jsonString;
    serializeJson(doc, jsonString);
    jsonString += "\r\n";

    Serial.printf("Setting brightness to %d over %d ms\n", brightness, FADE_DURATION);
    lampClient.print(jsonString);
    
    return true;
}

bool setYeelightColor(int r, int g, int b) {
    if (!lampClient.connected()) {
        if (!lampClient.connect(LAMP_IP, LAMP_PORT)) {
            Serial.println("Failed to connect to Yeelight");
            return false;
        }
        delay(100);
    }

    JsonDocument doc;
    doc["id"] = 1;
    doc["method"] = "set_rgb";
    
    // Constrain RGB values and calculate combined value
    int rgb = (constrain(r, 0, 255) << 16) | 
              (constrain(g, 0, 255) << 8) | 
              constrain(b, 0, 255);
    
    JsonArray params = doc["params"].to<JsonArray>();
    params.add(rgb);
    params.add("smooth");
    params.add(FADE_DURATION);  // Use longer fade duration

    String jsonString;
    serializeJson(doc, jsonString);
    jsonString += "\r\n";

    Serial.printf("Setting color to RGB(%d,%d,%d) over %d ms\n", r, g, b, FADE_DURATION);
    lampClient.print(jsonString);
    
    return true;
}

bool sendYeelightCommand(const char* method, const char* param) {
    if (!lampClient.connected()) {
        if (!lampClient.connect(LAMP_IP, LAMP_PORT)) {
            Serial.println("Failed to connect to Yeelight");
            return false;
        }
        delay(100);
    }

    if (strcmp(param, "on") == 0) {
        // Step 1: Send the power on command with fade and warm color temp
        JsonDocument doc;
        doc["id"] = 1;
        doc["method"] = "set_power";
        JsonArray params = doc["params"].to<JsonArray>();
        params.add("on");
        params.add("smooth");
        params.add(FADE_DURATION);
        params.add(2); // mode 2 = color temperature mode (important for warm light)

        String jsonString;
        serializeJson(doc, jsonString);
        jsonString += "\r\n";
        
        if (lampClient.print(jsonString) > 0) {
            delay(100);
            
            // Step 2: Set warm color temperature
            doc.clear();
            doc["id"] = 2;
            doc["method"] = "set_ct_abx";
            params = doc["params"].to<JsonArray>();
            params.add(COLOR_TEMP_WARM);
            params.add("smooth");
            params.add(FADE_DURATION);

            jsonString.clear();
            serializeJson(doc, jsonString);
            jsonString += "\r\n";
            lampClient.print(jsonString);
            delay(100);
            
            // Step 3: Set brightness with fade
            doc.clear();
            doc["id"] = 3;
            doc["method"] = "set_bright";
            params = doc["params"].to<JsonArray>();
            params.add(MAX_BRIGHTNESS);
            params.add("smooth");
            params.add(FADE_DURATION);

            jsonString.clear();
            serializeJson(doc, jsonString);
            jsonString += "\r\n";
            lampClient.print(jsonString);
        }
    }
    else if (strcmp(param, "off") == 0) {
        // Send the power off command with fade
        JsonDocument doc;
        doc["id"] = 1;
        doc["method"] = "set_power";
        JsonArray params = doc["params"].to<JsonArray>();
        params.add("off");
        params.add("smooth");
        params.add(FADE_DURATION);

        String jsonString;
        serializeJson(doc, jsonString);
        jsonString += "\r\n";
        lampClient.print(jsonString);
    }
    else {
        // Handle other commands normally
        JsonDocument doc;
        doc["id"] = 1;
        doc["method"] = method;
        
        JsonArray params = doc["params"].to<JsonArray>();
        params.add(param);
        params.add("smooth");
        params.add(FADE_DURATION);

        String jsonString;
        serializeJson(doc, jsonString);
        jsonString += "\r\n";

        Serial.printf("Sending to Yeelight: %s", jsonString.c_str());
        if (lampClient.print(jsonString) > 0) {
            delay(100);
            
            unsigned long timeout = millis() + 1000;
            while (millis() < timeout && !lampClient.available()) {
                delay(10);
            }
            
            if (lampClient.available()) {
                String response = lampClient.readStringUntil('\n');
                Serial.printf("Yeelight response: %s\n", response.c_str());
                return true;
            }
        }
    }
    
    lampClient.stop();
    return false;
}