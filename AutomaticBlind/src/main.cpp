#include <WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "config.h"

// UDP settings
WiFiUDP udp;
const int UDP_PORT = 8888;
char packetBuffer[255];

// Motor Pins 
const int PUL = 25;
const int DIR = 26;
const int ENA = 27;

// Motor settings
const int steps_per_rev = 6400;
const double rotations = 31;   
const int step_delay = 50;     

// EEPROM address for storing blinds state
#define EEPROM_ADDR 0

// Connection monitoring
unsigned long lastWifiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
bool isMoving = false;

void moveBlindsDown();
void moveBlindsUp();

void sendState(const String& state) {
    // Get the IP address and port of the device that sent the last command
    IPAddress remoteIP = udp.remoteIP();
    uint16_t remotePort = udp.remotePort();
    
    if (remoteIP != IPAddress(0,0,0,0)) {
        udp.beginPacket(remoteIP, remotePort);
        udp.print(state);
        udp.endPacket();
        Serial.printf("Sent state '%s' to %s:%d\n", 
                     state.c_str(), 
                     remoteIP.toString().c_str(), 
                     remotePort);
    }
}

void setupWiFi() {
    WiFi.disconnect();
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected");
        Serial.println("IP address: " + WiFi.localIP().toString());
        
        udp.stop();
        if(udp.begin(UDP_PORT)) {
            Serial.printf("UDP Listening on port %d\n", UDP_PORT);
        } else {
            Serial.println("Failed to start UDP");
        }
    } else {
        Serial.println("\nWiFi connection failed");
    }
}

void setup() {
    Serial.begin(115200);
    
    EEPROM.begin(sizeof(int));
    
    int storedState;
    EEPROM.get(EEPROM_ADDR, storedState);
    if (storedState != 0 && storedState != 1) {
        EEPROM.put(EEPROM_ADDR, -1);
        EEPROM.commit();
    }
    
    pinMode(PUL, OUTPUT);
    pinMode(DIR, OUTPUT);
    pinMode(ENA, OUTPUT);
    digitalWrite(ENA, HIGH);
    
    setupWiFi();
}

void checkWiFiConnection() {
    if (millis() - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
        lastWifiCheck = millis();
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost! Reconnecting...");
            setupWiFi();
        }
    }
}

void loop() {
    checkWiFiConnection();
    
    if (WiFi.status() == WL_CONNECTED && !isMoving) {
        int packetSize = udp.parsePacket();
        if (packetSize) {
            int len = udp.read(packetBuffer, 255);
            if (len > 0) {
                packetBuffer[len] = 0;
                
                String command = String(packetBuffer);
                command.trim();
                
                Serial.printf("Processing command: %s from %s\n", 
                            command.c_str(), 
                            udp.remoteIP().toString().c_str());
                
                if (command == "up") {
                    isMoving = true;
                    moveBlindsUp();
                    isMoving = false;
                }
                else if (command == "down") {
                    isMoving = true;
                    moveBlindsDown();
                    isMoving = false;
                }
                else if (command == "state") {
                    // Send current state without moving
                    int currentState;
                    EEPROM.get(EEPROM_ADDR, currentState);
                    sendState(currentState == 1 ? "down" : 
                             currentState == 0 ? "up" : "unknown");
                }
            }
        }
    }
}

void moveBlindsDown() {
    int lastBlindsState;
    EEPROM.get(EEPROM_ADDR, lastBlindsState);
    
    if (lastBlindsState == 0 || lastBlindsState == -1) {
        digitalWrite(ENA, LOW);
        Serial.println("Moving blinds down");
        digitalWrite(DIR, HIGH);
        int steps = steps_per_rev * rotations;
        for (int i = 0; i < steps; i++) {
            digitalWrite(PUL, HIGH);
            delayMicroseconds(step_delay);
            digitalWrite(PUL, LOW);
            delayMicroseconds(step_delay);
        }
        delay(1000);
        digitalWrite(ENA, HIGH);
        EEPROM.put(EEPROM_ADDR, 1);
        EEPROM.commit();
        
        sendState("down");  // Send final position
    } else {
        Serial.println("Blinds are already down");
        sendState("already_down");
    }
}

void moveBlindsUp() {
    int lastBlindsState;
    EEPROM.get(EEPROM_ADDR, lastBlindsState);
    
    if (lastBlindsState == 1 || lastBlindsState == -1) {
        digitalWrite(ENA, LOW);
        Serial.println("Moving blinds up");
        
        digitalWrite(DIR, LOW);
        int steps = steps_per_rev * rotations;
        for (int i = 0; i < steps; i++) {
            digitalWrite(PUL, HIGH);
            delayMicroseconds(step_delay);
            digitalWrite(PUL, LOW);
            delayMicroseconds(step_delay);
        }
        delay(1000);
        digitalWrite(ENA, HIGH);
        EEPROM.put(EEPROM_ADDR, 0);
        EEPROM.commit();
        
        sendState("up");  // Send final position
    } else {
        Serial.println("Blinds are already up");
        sendState("already_up");
    }
}