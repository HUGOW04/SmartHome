#include "lgfx.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <crypto.h>
#include <sys/time.h>

#include "config.h"
#include "yeetligth.h"

// UDP settings
WiFiUDP udp;
WiFiUDP ntpUDP;
const int UDP_PORT = 8888;
const IPAddress BINDS_IP(192, 168, 1, 227);

// NTP settings
NTPClient timeClient(ntpUDP);

// Array of NTP servers
const char* ntpServers[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com",
    "europe.pool.ntp.org"
};
const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);

// Sleep mode settings
const unsigned long SLEEP_TIMEOUT = 30000;
unsigned long lastActivityTime = 0;
bool isScreenSleeping = false;
const uint8_t AWAKE_BRIGHTNESS = 128;
const uint8_t SLEEP_BRIGHTNESS = 0;

// Time backup variables
String lastTimeStr = "00:00:00";
String lastDayStr = "Unknown";
bool hasValidTime = false;

// Display constants
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;
const int TIME_SECTION_HEIGHT = 120;
const int BUTTON_MARGIN = 5;

// Flag for not spamming command to other esp
bool isMorning = true;

// Debounce settings
unsigned long wakeUpTime = 0;
const unsigned long WAKE_DEBOUNCE_TIME = 1000;

// Add these at the top with other variables
bool waitingForResponse = false;
unsigned long lastCommandTime = 0;
const unsigned long COMMAND_RETRY_INTERVAL = 5000; // Retry every 5 seconds
String lastCommand = "";
char responseBuffer[255];

// TIME SHIT
unsigned long lastMillis = 0;
time_t internalTime = 0;
bool initialTimeSynced = false;

// Add this constant at the top with other time-related constants
const int TIME_ZONE_OFFSET = 3600; // UTC+1

// Button definitions
struct Button {
    int x, y, w, h;
    const char* label;
    uint16_t color;
    uint16_t textColor;
    bool pressed;
};

const int BUTTON_WIDTH = (SCREEN_WIDTH - (BUTTON_MARGIN * 3)) / 2;
const int BUTTON_HEIGHT = (SCREEN_HEIGHT - TIME_SECTION_HEIGHT - (BUTTON_MARGIN * 3)) / 2;

// Add to global variables
bool skipDays[7] = {false, false, false, false, false, false, false};
const char* dayLabels[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

Button buttons[11] = {
    // Day buttons - top row
    {40, 10, 50, 40, "Sun", TFT_DARKGREY, TFT_WHITE, false},
    {100, 10, 50, 40, "Mon", TFT_DARKGREY, TFT_WHITE, false},
    {160, 10, 50, 40, "Tue", TFT_DARKGREY, TFT_WHITE, false},
    {220, 10, 50, 40, "Wed", TFT_DARKGREY, TFT_WHITE, false},
    
    // Day buttons - bottom row
    {70, 55, 50, 40, "Thu", TFT_DARKGREY, TFT_WHITE, false},
    {130, 55, 50, 40, "Fri", TFT_DARKGREY, TFT_WHITE, false},
    {190, 55, 50, 40, "Sat", TFT_DARKGREY, TFT_WHITE, false},
    
    // Control buttons
    {BUTTON_MARGIN, 120, BUTTON_WIDTH, BUTTON_HEIGHT, 
     "BindUp", TFT_DARKGREY, TFT_WHITE, false},
    {SCREEN_WIDTH - BUTTON_WIDTH - BUTTON_MARGIN, 120, BUTTON_WIDTH, BUTTON_HEIGHT, 
     "BindDown", TFT_DARKGREY, TFT_WHITE, false},
    {BUTTON_MARGIN, 120 + BUTTON_HEIGHT + BUTTON_MARGIN, BUTTON_WIDTH, BUTTON_HEIGHT, 
     "LampOn", TFT_DARKGREY, TFT_WHITE, false},
    {SCREEN_WIDTH - BUTTON_WIDTH - BUTTON_MARGIN, 120 + BUTTON_HEIGHT + BUTTON_MARGIN, 
     BUTTON_WIDTH, BUTTON_HEIGHT, "LampOff", TFT_DARKGREY, TFT_WHITE, false}
};

// Add touch gesture tracking variables
int16_t touchStartX = 0;
int16_t touchStartY = 0;
bool isDragging = false;
const int SWIPE_THRESHOLD = 50;
int currentPage = 0; // 0 = time page, 1 = buttons page
time_t adjustedTime;

void drawButton(Button& btn);
bool isButtonPressed(Button& btn, uint16_t x, uint16_t y);
String dayOfWeekString(int d);
bool isWeekday(int d);
bool isWeekend(int d);
bool isDST(int year, int month, int day, int dayOfWeek);
void updateTimeDisplay();
void sendCommand(const String& command);
void wakeScreen();
void updateInternalTime();

String dayOfWeekString(int d) {
    switch (d) {
        case 0: return "Sunday";
        case 1: return "Monday";
        case 2: return "Tuesday";
        case 3: return "Wednesday";
        case 4: return "Thursday";
        case 5: return "Friday";
        case 6: return "Saturday";
        default: return "Invalid";
    }
}

bool isWeekday(int d) {
    return (d >= 1 && d <= 5);
}

bool isWeekend(int d) {
    return (d == 0 || d == 6);
}

bool isDST(int year, int month, int day, int dayOfWeek) {
    if (month == 3 || month == 10) {
        int lastSundayInMarch = 31 - static_cast<int>(round(((5.0 * year / 4) + 4))) % 7;
        int lastSundayInOctober = 31 - static_cast<int>(round(((5.0 * year / 4) + 1))) % 7;
        
        if ((month == 3 && day >= lastSundayInMarch) || 
            (month == 10 && day < lastSundayInOctober)) {
            return true;
        }
    }
    if (month > 3 && month < 10) {
        return true;
    }
    return false;
}

void drawButton(Button& btn) {
    tft.fillRoundRect(btn.x - 2, btn.y - 2, btn.w + 4, btn.h + 4, 10, TFT_BLACK);
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 8, btn.color);
    
    tft.setTextColor(btn.textColor);
    tft.setTextSize(2);
    
    int16_t padding = 10;
    int16_t textX = (btn.x < SCREEN_WIDTH / 2) ? 
                    btn.x + padding : 
                    btn.x + btn.w - tft.textWidth(btn.label) - padding;
    
    int16_t textY = (btn.y < (TIME_SECTION_HEIGHT + BUTTON_HEIGHT)) ?
                    btn.y + padding :
                    btn.y + btn.h - 20 - padding;
    
    tft.drawString(btn.label, textX, textY);
}

bool isButtonPressed(Button& btn, uint16_t x, uint16_t y) {
    return (x >= btn.x && x <= (btn.x + btn.w) &&
            y >= btn.y && y <= (btn.y + btn.h));
}



// Modify the existing sendCommand function
void sendCommand(const String& command) {
    // Don't send empty commands
    if (command.isEmpty()) {
        Serial.println("Warning: Attempted to send empty command");
        return;
    }
    
    // Handle lamp commands
    if (command == "on" || command == "off") {
        if (sendYeelightCommand("set_power", command.c_str())) {
            Serial.println("Yeelight command sent successfully");
            return;
        }
        Serial.println("Failed to send Yeelight command");
        return;
    }
    
    // Handle blind commands (existing code)
    Serial.printf("Sending command: %s to %s\n", command.c_str(), BINDS_IP.toString().c_str());
    udp.beginPacket(BINDS_IP, UDP_PORT);
    udp.write((const uint8_t*)command.c_str(), command.length());
    udp.endPacket();
    lastCommandTime = millis();
    lastCommand = command;
    waitingForResponse = true;
}

void checkResponse() {
    if (!waitingForResponse) return;
    
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(responseBuffer, 255);
        if (len > 0) {
            responseBuffer[len] = 0;
            String response = String(responseBuffer);
            Serial.printf("Received response: %s\n", response.c_str());
            
            // Check if the response matches any valid state
            if ((lastCommand == "up" && (response == "up" || response == "already_up")) ||
                (lastCommand == "down" && (response == "down" || response == "already_down"))) {
                waitingForResponse = false;
                if (lastCommand == "up") {
                    isMorning = false;
                } else if (lastCommand == "down") {
                    isMorning = true;
                }
                lastCommand = "";
            }
        }
    }
    
    // Only retry blinds commands
    if (waitingForResponse && (lastCommand == "up" || lastCommand == "down") && 
        (millis() - lastCommandTime > COMMAND_RETRY_INTERVAL)) {
        sendCommand(lastCommand);
    }
}

void wakeScreen() {
    if (isScreenSleeping) {
        wakeUpTime = millis();
        lastActivityTime = millis();
        isScreenSleeping = false;
        
        // First set brightness without any drawing
        tft.setBrightness(AWAKE_BRIGHTNESS);
        delay(50);  // Short delay to let brightness settle

        currentPage = 0; // reset to time page
    }
}

void handleSleepMode() {
    if (!isScreenSleeping && (millis() - lastActivityTime > SLEEP_TIMEOUT) && (millis() - wakeUpTime > WAKE_DEBOUNCE_TIME)) {
        tft.setBrightness(SLEEP_BRIGHTNESS);
        isScreenSleeping = true;
    }
}



// Modify updateTimeDisplay to separate time updating from display updating
void updateInternalTime() {
    if (!initialTimeSynced) {
        // Try to get initial time from NTP only once at startup
        bool updateSuccess = false;
        for (int i = 0; i < numNtpServers && !updateSuccess; i++) {
            timeClient.setPoolServerName(ntpServers[i]);
            updateSuccess = timeClient.update();
            if (updateSuccess) {
                internalTime = timeClient.getEpochTime();
                lastMillis = millis();
                initialTimeSynced = true;
                Serial.println("Initial time sync successful");
                break;
            }
            delay(100);
        }
        if (!initialTimeSynced) return;
    }
    
    // Update internal time based on millis()
    unsigned long currentMillis = millis();
    unsigned long deltaMillis = currentMillis - lastMillis;
    
    // Handle millis() overflow
    if (currentMillis < lastMillis) {
        deltaMillis = currentMillis + (0xFFFFFFFF - lastMillis);
    }
    
    // Update internal time and lastMillis if at least a second has passed
    if (deltaMillis >= 1000) {
        internalTime += deltaMillis / 1000;
        lastMillis = currentMillis - (deltaMillis % 1000); // Keep remainder for accuracy
    }
}

void drawTimePage() {
    tft.fillScreen(TFT_BLACK);
    
    tft.setFont(&fonts::Font0);
    tft.setTextSize(2);
    
    const int labelX = SCREEN_WIDTH - 200;
    const int valueX = SCREEN_WIDTH - 120;
    const int startY = 80;
    const int spacing = 40;
    
    // Time
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Time:", labelX, startY);
    
    // Day
    tft.drawString("Day:", labelX, startY + spacing);
    
    // DST
    tft.drawString("DST:", labelX, startY + spacing * 2);
}

void drawButtonsPage() {
    tft.fillScreen(TFT_BLACK);
    for (int i = 0; i < 11; i++) {
        drawButton(buttons[i]);
    }
}

void handleButtonPress(uint16_t x, uint16_t y) {
    for (int i = 0; i < 11; i++) {
        if (isButtonPressed(buttons[i], x, y)) {
            if (i < 7) {
                // Day buttons
                skipDays[i] = !skipDays[i];
                buttons[i].color = skipDays[i] ? TFT_RED : TFT_DARKGREY;
                drawButton(buttons[i]);
            } else {
                // Control buttons (index 7-10)
                buttons[i].color = TFT_WHITE;
                drawButton(buttons[i]);
                
                if (!waitingForResponse) {
                    switch(i - 7) {
                        case 0: 
                            sendCommand("up");
                            blindManualControl = true;
                            break;
                        case 1: 
                            sendCommand("down");
                            blindManualControl = true;
                            break;
                        case 2: 
                            sendCommand("on");
                            lightManualControl = true;
                            lightIsOn = true;
                            lightOnTime = adjustedTime;
                            break;
                        case 3: 
                            sendCommand("off");
                            lightManualControl = true;
                            lightIsOn = false;
                            break;
                    }
                }
                
                buttons[i].color = TFT_DARKGREY;
                drawButton(buttons[i]);
            }
        }
    }
}

void handleTouchGesture() {
    uint16_t x, y;
    static uint32_t touchStartTime = 0;
    static uint16_t maxDistance = 0;
    
    if (tft.getTouch(&x, &y)) {
        if (isScreenSleeping) {
            wakeScreen();
            if (currentPage == 0) {
                drawTimePage();
            } else {
                drawButtonsPage();
            }
            isDragging = false;
            maxDistance = 0;
            return;
        }
        
        if (!isDragging) {
            touchStartX = x;
            touchStartY = y;
            touchStartTime = millis();
            isDragging = true;
            maxDistance = 0;
        } else if (millis() - wakeUpTime > WAKE_DEBOUNCE_TIME) {
            int16_t swipeDistance = x - touchStartX;
            maxDistance = max(maxDistance, (uint16_t)abs(swipeDistance));
            
            if (maxDistance > SWIPE_THRESHOLD && (millis() - touchStartTime < 300)) {
                if (swipeDistance > 0 && currentPage == 0) {
                    currentPage = 1;
                    drawButtonsPage();
                } else if (swipeDistance < 0 && currentPage == 1) {
                    currentPage = 0;
                    drawTimePage();
                }
                isDragging = false;
                return;
            }
        }
        lastActivityTime = millis();
    } else {
        if (isDragging && currentPage == 1 && maxDistance < SWIPE_THRESHOLD/2) {
            handleButtonPress(touchStartX, touchStartY);
        }
        isDragging = false;
        maxDistance = 0;
    }
}



void updateTimeDisplay() {
    if (!initialTimeSynced || currentPage != 0) return;
    
    struct tm *baseTime = localtime(&internalTime);
    bool isDstActive = isDST(baseTime->tm_year + 1900, 
                           baseTime->tm_mon + 1, 
                           baseTime->tm_mday, 
                           baseTime->tm_wday);
    
    time_t adjustedTime = internalTime + TIME_ZONE_OFFSET + (isDstActive ? 3600 : 0);
    struct tm *currentTime = localtime(&adjustedTime);
    
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", 
            currentTime->tm_hour,
            currentTime->tm_min);
    
    const int valueX = SCREEN_WIDTH - 120;
    const int startY = 80;
    const int spacing = 40;
    
    // Update time
    tft.fillRect(valueX, startY, 100, 30, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString(timeStr, valueX, startY);
    
    // Update day
    tft.fillRect(valueX, startY + spacing, 100, 30, TFT_BLACK);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(dayOfWeekString(currentTime->tm_wday).c_str(), valueX, startY + spacing);
    
    // Update DST
    tft.fillRect(valueX, startY + spacing * 2, 100, 30, TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.drawString(isDstActive ? "Active" : "Inactive", valueX, startY + spacing * 2);
}

void setup() {
    Serial.begin(115200);
    
    tft.init();
    tft.setRotation(1);
    tft.setBrightness(AWAKE_BRIGHTNESS);
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Connecting to WiFi...", 10, 10);
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");
    
    // Test initial connection to Yeelight
    if (lampClient.connect(LAMP_IP, LAMP_PORT)) {
        Serial.println("Connected to Yeelight");
        lampClient.stop();
    } else {
        Serial.println("Failed to connect to Yeelight");
    }

    // Initialize UDP
    udp.begin(UDP_PORT + 1);  // Use different port than receiver
    Serial.println("UDP Initialized on port " + String(UDP_PORT + 1));
    
    tft.fillScreen(TFT_BLACK);
    lastActivityTime = millis();
    
    drawTimePage();
}

void loop() {
    updateInternalTime();
    handleSleepMode();
    checkResponse();
    handleTouchGesture();
    
    if (!isScreenSleeping) {
        updateTimeDisplay();
    }

    // Get the current time with timezone and DST adjustment
    struct tm *baseTime = localtime(&internalTime);
    bool isDstActive = isDST(baseTime->tm_year + 1900, 
                           baseTime->tm_mon + 1, 
                           baseTime->tm_mday, 
                           baseTime->tm_wday);
    adjustedTime = internalTime + TIME_ZONE_OFFSET + (isDstActive ? 3600 : 0);
    struct tm *currentTime = localtime(&adjustedTime);
   
    // Reset clicked flags at specific hours - now separate for blinds and lights
    if ((currentTime->tm_hour == 6 && currentTime->tm_min == 0 && currentTime->tm_sec == 0) || 
        (currentTime->tm_hour == 9 && currentTime->tm_min == 0 && currentTime->tm_sec == 0) || 
        (currentTime->tm_hour == 22 && currentTime->tm_min == 0 && currentTime->tm_sec == 0)) {
        blindManualControl = false;
        lightManualControl = false;
        Serial.println("Reset manual controls");
    }

    // Automatic light control - only if not manually controlled
    if (!lightManualControl && !skipDays[currentTime->tm_wday]) {
        // Turn on light in the morning
        if (isWeekday(currentTime->tm_wday)) {
            if (currentTime->tm_hour == 6 && currentTime->tm_min == 0 && !lightIsOn) {
                sendCommand("on");
                lightIsOn = true;
                lightOnTime = adjustedTime;
            }
        } else if (isWeekend(currentTime->tm_wday)) {
            if (currentTime->tm_hour == 9 && currentTime->tm_min == 0 && !lightIsOn) {
                sendCommand("on");
                lightIsOn = true;
                lightOnTime = adjustedTime;
            }
        }

        // Check if light should be turned off (after 1 hour)
        if (lightIsOn && (adjustedTime - lightOnTime >= LIGHT_ON_DURATION)) {
            sendCommand("off");
            lightIsOn = false;
        }
    }

   // Blind control logic - separate checks for up and down
    if (!blindManualControl) {
        // Up motion - check both manual control and skipDays
        bool shouldBeUp = !skipDays[currentTime->tm_wday] && (
                        (isWeekday(currentTime->tm_wday) && 
                        ((currentTime->tm_hour == 6 && currentTime->tm_min >= 0) || 
                        (currentTime->tm_hour > 6 && currentTime->tm_hour < 22))) || 
                        (isWeekend(currentTime->tm_wday) && 
                        ((currentTime->tm_hour == 9 && currentTime->tm_min >= 0) || 
                        (currentTime->tm_hour > 9 && currentTime->tm_hour < 22))));
        
        // Down motion - only check manual control
        bool shouldBeDown = currentTime->tm_hour >= 22 || 
                        (isWeekday(currentTime->tm_wday) && currentTime->tm_hour < 6) ||
                        (isWeekend(currentTime->tm_wday) && currentTime->tm_hour < 9);
        
        if (shouldBeUp && isMorning && !waitingForResponse) {
            sendCommand("up");
        } 
        else if (shouldBeDown && !isMorning && !waitingForResponse) {
            sendCommand("down");
        }
    }
}