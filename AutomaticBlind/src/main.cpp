#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <sys/time.h>

#include "config.h"
#include "motor_control.h"

// webserver on port 8080
WebServer server(8080);

// NTP and Time Management
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Array of NTP servers for redundancy
const char* ntpServers[] = {
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com",
    "europe.pool.ntp.org"
};
const int numNtpServers = sizeof(ntpServers) / sizeof(ntpServers[0]);

// Internal time tracking
time_t internalTime = 0;
unsigned long lastMillis = 0;
bool initialTimeSynced = false;
const int TIME_ZONE_OFFSET = 3600; // UTC+1 (adjust for your timezone)

// Automatic control flags
bool blindManualControl = false;
unsigned long lastResetTime = 0;

// Day skip configuration
bool skipDays[7] = {false, false, false, false, false, false, false}; // Sun, Mon, Tue, Wed, Thu, Fri, Sat

// Stability improvements
unsigned long lastHeapCheck = 0;
unsigned long wifiReconnectTimer = 0;
int reconnectAttempts = 0;
unsigned long lastWatchdog = 0;
const unsigned long WATCHDOG_TIMEOUT = 300000; // 5 minutes
unsigned long lastMemoryReport = 0;

// Connection management
const int MAX_WIFI_RETRIES = 20;
const int WIFI_RECONNECT_INTERVAL = 10000; // 10 seconds
const int MAX_RECONNECT_ATTEMPTS = 5;

void setupWiFi()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    WiFi.mode(WIFI_STA);
    
    // Configure WiFi with static settings for stability
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false); // Don't save WiFi config to flash
    
    WiFi.begin(ssid, password);

    Serial.print("Connecting to WiFi");
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < MAX_WIFI_RETRIES) {
        delay(500);
        Serial.print(".");
        retry++;
        
        // Feed watchdog during long operations
        yield();
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        Serial.println("IP address: " + WiFi.localIP().toString());
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        reconnectAttempts = 0; // Reset on successful connection
    } else {
        Serial.println("WiFi connection failed!");
    }
}

// Initialize NTP and sync time once
void initializeTime() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    Serial.println("Initializing time sync...");
    
    // Try each NTP server until one works
    for (int i = 0; i < numNtpServers && !initialTimeSynced; i++) {
        Serial.printf("Trying NTP server: %s\n", ntpServers[i]);
        timeClient.setPoolServerName(ntpServers[i]);
        timeClient.begin();
        timeClient.setUpdateInterval(86400000); // Update once per day only
        
        bool updateSuccess = timeClient.update();
        if (updateSuccess) {
            internalTime = timeClient.getEpochTime();
            lastMillis = millis();
            initialTimeSynced = true;
            Serial.printf("Time sync successful! Current time: %lu\n", internalTime);
            break;
        }
        delay(500);
        yield(); // Feed watchdog
    }
    
    if (!initialTimeSynced) {
        Serial.println("Failed to sync time with any NTP server");
    }
    
    // Clean up NTP client to free memory
    timeClient.end();
}

// Update internal time based on millis() - no more NTP calls
void updateInternalTime() {
    if (!initialTimeSynced) return;
    
    unsigned long currentMillis = millis();
    unsigned long deltaMillis = currentMillis - lastMillis;
    
    // Handle millis() overflow (happens every ~49 days)
    if (currentMillis < lastMillis) {
        deltaMillis = currentMillis + (0xFFFFFFFF - lastMillis);
        Serial.println("Millis overflow handled");
    }
    
    // Update internal time if at least a second has passed
    if (deltaMillis >= 1000) {
        internalTime += deltaMillis / 1000;
        lastMillis = currentMillis - (deltaMillis % 1000); // Keep remainder for accuracy
    }
}

// Check if current date is in Daylight Saving Time (EU rules)
bool isDST(int year, int month, int day, int dayOfWeek) {
    if (month < 3 || month > 10) return false;
    if (month > 3 && month < 10) return true;
    
    if (month == 3) {
        int lastSundayInMarch = 31 - ((5 * year / 4 + 4) % 7);
        return day >= lastSundayInMarch;
    }
    
    if (month == 10) {
        int lastSundayInOctober = 31 - ((5 * year / 4 + 1) % 7);
        return day < lastSundayInOctober;
    }
    
    return false;
}

// Get current local time with timezone and DST adjustments
time_t getCurrentLocalTime() {
    if (!initialTimeSynced) return 0;
    
    struct tm *baseTime = localtime(&internalTime);
    bool isDstActive = isDST(baseTime->tm_year + 1900, 
                           baseTime->tm_mon + 1, 
                           baseTime->tm_mday, 
                           baseTime->tm_wday);
    
    return internalTime + TIME_ZONE_OFFSET + (isDstActive ? 3600 : 0);
}

// Helper functions for day types
bool isWeekday(int dayOfWeek) {
    return (dayOfWeek >= 1 && dayOfWeek <= 5); // Monday to Friday
}

bool isWeekend(int dayOfWeek) {
    return (dayOfWeek == 0 || dayOfWeek == 6); // Sunday or Saturday
}

// Get day name
String getDayName(int dayOfWeek) {
    const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    if (dayOfWeek >= 0 && dayOfWeek <= 6) {
        return String(days[dayOfWeek]);
    }
    return "Unknown";
}

// Get short day name
String getShortDayName(int dayOfWeek) {
    const char* shortDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (dayOfWeek >= 0 && dayOfWeek <= 6) {
        return String(shortDays[dayOfWeek]);
    }
    return "???";
}

// Memory-efficient HTML generation - split into smaller functions
String generateHTMLHeader() {
    return R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <title>Smart Blinds</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="30">
    <style>
        * {margin: 0; padding: 0; box-sizing: border-box;}
        body {font-family: -apple-system, BlinkMacSystemFont, sans-serif; background: #0f0f0f; color: #e5e5e5; min-height: 100vh; display: flex; justify-content: center; align-items: center; padding: 20px;}
        .container {background: #1a1a1a; border: 1px solid #333; border-radius: 12px; padding: 30px; max-width: 450px; width: 100%; text-align: center;}
        .title {font-size: 1.8rem; font-weight: 600; margin-bottom: 30px; color: #fff;}
        .time-section {margin-bottom: 25px; padding: 20px; background: #262626; border-radius: 8px;}
        .time {font-size: 2.5rem; font-weight: 300; color: #fff; margin-bottom: 5px;}
        .day {font-size: 1.1rem; color: #9ca3af; margin-bottom: 10px;}
        .schedule {font-size: 0.9rem; color: #6b7280;}
        .schedule.disabled {color: #f59e0b; font-weight: 500;}
        .status {margin-bottom: 25px; padding: 15px; border-radius: 8px;}
        .status-text {font-size: 1.2rem; font-weight: 500; color: white;}
        .manual-warning {background: #d97706; color: white; padding: 12px; border-radius: 6px; margin-bottom: 20px; font-size: 0.9rem;}
        .day-skip-section {margin-bottom: 25px; padding: 20px; background: #262626; border-radius: 8px;}
        .day-skip-title {font-size: 1rem; font-weight: 500; color: #fff; margin-bottom: 15px;}
        .day-buttons {display: grid; grid-template-columns: repeat(7, 1fr); gap: 8px;}
        .day-btn {padding: 8px 4px; text-decoration: none; border-radius: 6px; font-size: 0.8rem; font-weight: 500; transition: all 0.2s ease; border: 1px solid #333; background: #1a1a1a; color: #9ca3af;}
        .day-btn:hover {transform: translateY(-1px); border-color: #555;}
        .day-btn.active {background: #f59e0b; color: white; border-color: #d97706;}
        .day-btn.today {border-color: #22c55e; border-width: 2px;}
        .controls {display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 20px;}
        .btn {padding: 15px; text-decoration: none; border-radius: 8px; font-weight: 500; transition: all 0.2s ease; border: 1px solid #333;}
        .btn:hover {transform: translateY(-1px);}
        .up-btn {background: #22c55e; color: white;}
        .up-btn:hover {background: #16a34a;}
        .down-btn {background: #ef4444; color: white;}
        .down-btn:hover {background: #dc2626;}
        .debug-toggle {background: #374151; color: #e5e5e5; padding: 10px 15px; border-radius: 8px; text-decoration: none; display: inline-block; margin-top: 10px; font-size: 0.9rem; border: 1px solid #4b5563;}
        .debug-toggle:hover {background: #4b5563; transform: translateY(-1px);}
        .debug-section {margin-top: 20px; padding: 15px; background: #262626; border-radius: 8px; text-align: left; font-size: 0.85rem; display: none;}
        .debug-section.show {display: block;}
        .debug-title {font-weight: 600; color: #fff; margin-bottom: 10px;}
        .debug-item {margin-bottom: 8px; display: flex; justify-content: space-between;}
        .debug-label {color: #9ca3af;}
        .debug-value {color: #e5e5e5; font-weight: 500;}
        .debug-good {color: #22c55e;}
        .debug-warning {color: #f59e0b;}
        .debug-error {color: #ef4444;}
        @media (max-width: 480px) {
            .container {padding: 20px;}
            .time {font-size: 2rem;}
            .controls {gap: 10px;}
            .day-buttons {gap: 4px;}
            .day-btn {padding: 6px 2px; font-size: 0.7rem;}
        }
    </style>
</head>
<body>)rawliteral";
}

void handleRoot() {
    // Reset watchdog
    lastWatchdog = millis();
    
    time_t localTime = getCurrentLocalTime();
    struct tm *currentTime = localtime(&localTime);
    
    int currentState = getCurrentBlindsState();
    String statusText = "";
    String statusColor = "";
    
    if (currentState == 1) {
        statusText = "DOWN";
        statusColor = "#ef4444";  // Red
    } else if (currentState == 0) {
        statusText = "UP";
        statusColor = "#22c55e";  // Green
    } else {
        statusText = "UNKNOWN";
        statusColor = "#f59e0b";  // Orange
    }
    
    // Time display
    String timeStr = "Time not synced";
    String dayStr = "Unknown";
    String scheduleInfo = "";
    bool todaySkipped = false;
    
    if (initialTimeSynced) {
        char timeBuffer[16];
        sprintf(timeBuffer, "%02d:%02d:%02d", 
                currentTime->tm_hour, 
                currentTime->tm_min, 
                currentTime->tm_sec);
        timeStr = String(timeBuffer);
        dayStr = getDayName(currentTime->tm_wday);
        todaySkipped = skipDays[currentTime->tm_wday];
        
        // Show schedule info - FIXED to match the actual logic (10 PM, not 8 PM)
        if (todaySkipped) {
            scheduleInfo = "Today: AUTO DISABLED - Sleep in mode";
        } else if (isWeekday(currentTime->tm_wday)) {
            scheduleInfo = "Weekday: UP 6:00 AM - DOWN 10:00 PM";
        } else {
            scheduleInfo = "Weekend: UP 9:00 AM - DOWN 10:00 PM";
        }
    }
    
    // Build HTML in parts to save memory
    String html = generateHTMLHeader();
    
    html += "<div class=\"container\">";
    html += "<h1 class=\"title\">Smart Blinds</h1>";
    
    html += "<div class=\"time-section\">";
    html += "<div class=\"time\">" + timeStr + "</div>";
    html += "<div class=\"day\">" + dayStr + "</div>";
    html += "<div class=\"schedule";
    if (todaySkipped) {
        html += " disabled";
    }
    html += "\">" + scheduleInfo + "</div>";
    html += "</div>";
    
    html += "<div class=\"status\" style=\"background-color: " + statusColor + ";\">";
    html += "<div class=\"status-text\">Blinds " + statusText + "</div>";
    html += "</div>";

    if (blindManualControl) {
        html += "<div class=\"manual-warning\">Manual Control Active</div>";
    }

    // Add day skip buttons
    html += "<div class=\"day-skip-section\">";
    html += "<div class=\"day-skip-title\">Sleep In Days (Auto OFF)</div>";
    html += "<div class=\"day-buttons\">";

    // Generate day buttons
    for (int i = 0; i < 7; i++) {
        String dayClass = "day-btn";
        if (skipDays[i]) {
            dayClass += " active";
        }
        if (initialTimeSynced && i == currentTime->tm_wday) {
            dayClass += " today";
        }
        
        html += "<a href=\"/skip/" + String(i) + "\" class=\"" + dayClass + "\">" + getShortDayName(i) + "</a>";
    }

    html += "</div></div>";
    
    html += "<div class=\"controls\">";
    html += "<a href=\"/up\" class=\"btn up-btn\">RAISE</a>";
    html += "<a href=\"/down\" class=\"btn down-btn\">LOWER</a>";
    html += "</div>";
    
    // Debug toggle button
    html += "<a href=\"javascript:void(0)\" class=\"debug-toggle\" onclick=\"toggleDebug()\">Debug Info</a>";
    
    // Debug section
    html += "<div class=\"debug-section\" id=\"debugSection\">";
    html += "<div class=\"debug-title\">System Debug Information</div>";
    
    // WiFi Status
    String wifiStatus = "";
    String wifiClass = "";
    if (WiFi.status() == WL_CONNECTED) {
        wifiStatus = "Connected (" + WiFi.localIP().toString() + ")";
        wifiClass = "debug-good";
    } else {
        wifiStatus = "Disconnected";
        wifiClass = "debug-error";
    }
    html += "<div class=\"debug-item\"><span class=\"debug-label\">WiFi:</span><span class=\"debug-value " + wifiClass + "\">" + wifiStatus + "</span></div>";
    
    // Memory usage
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t minFreeHeap = ESP.getMinFreeHeap();
    String memoryClass = freeHeap > 20000 ? "debug-good" : (freeHeap > 10000 ? "debug-warning" : "debug-error");
    html += "<div class=\"debug-item\"><span class=\"debug-label\">Free Memory:</span><span class=\"debug-value " + memoryClass + "\">" + String(freeHeap) + " bytes</span></div>";
    html += "<div class=\"debug-item\"><span class=\"debug-label\">Min Free Memory:</span><span class=\"debug-value\">" + String(minFreeHeap) + " bytes</span></div>";
    
    // Time sync status
    String timeClass = initialTimeSynced ? "debug-good" : "debug-error";
    String timeStatus = initialTimeSynced ? "Synced" : "Not Synced";
    html += "<div class=\"debug-item\"><span class=\"debug-label\">Time Sync:</span><span class=\"debug-value " + timeClass + "\">" + timeStatus + "</span></div>";
    
    // Uptime
    unsigned long uptime = millis() / 1000;
    unsigned long days = uptime / 86400;
    unsigned long hours = (uptime % 86400) / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    String uptimeStr = String(days) + "d " + String(hours) + "h " + String(minutes) + "m";
    html += "<div class=\"debug-item\"><span class=\"debug-label\">Uptime:</span><span class=\"debug-value\">" + uptimeStr + "</span></div>";
    
    // Reconnect attempts
    String reconnectClass = reconnectAttempts == 0 ? "debug-good" : (reconnectAttempts < 3 ? "debug-warning" : "debug-error");
    html += "<div class=\"debug-item\"><span class=\"debug-label\">Reconnect Attempts:</span><span class=\"debug-value " + reconnectClass + "\">" + String(reconnectAttempts) + "</span></div>";
    
    // Manual control status
    String manualClass = blindManualControl ? "debug-warning" : "debug-good";
    String manualStatus = blindManualControl ? "Active" : "Inactive";
    html += "<div class=\"debug-item\"><span class=\"debug-label\">Manual Control:</span><span class=\"debug-value " + manualClass + "\">" + manualStatus + "</span></div>";
    
    html += "</div>";
    
    html += "<script>";
    html += "function toggleDebug() {";
    html += "  var debug = document.getElementById('debugSection');";
    html += "  debug.classList.toggle('show');";
    html += "}";
    html += "</script>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
    
    // Clear string to free memory immediately
    html = "";
}

void handleUp() {
    lastWatchdog = millis(); // Reset watchdog
    
    Serial.println("Web request: Move blinds UP");
    moveBlindsUp();
    blindManualControl = true; // Disable automatic control
    
    String html = generateHTMLHeader();
    html += R"rawliteral(
    <div class="container">
        <div style="font-size: 3rem; margin-bottom: 15px; color: #22c55e;">↑</div>
        <h1 style="font-size: 1.5rem; margin-bottom: 10px;">Raising Blinds</h1>
        <p style="color: #9ca3af;">Manual control activated</p>
        <p style="color: #9ca3af;">Returning in 3 seconds...</p>
    </div>
    <script>setTimeout(function(){window.location.href='/';}, 3000);</script>
</body></html>)rawliteral";
    
    server.send(200, "text/html", html);
    html = ""; // Free memory
}

void handleDown() {
    lastWatchdog = millis(); // Reset watchdog
    
    Serial.println("Web request: Move blinds DOWN");
    moveBlindsDown();
    blindManualControl = true; // Disable automatic control
    
    String html = generateHTMLHeader();
    html += R"rawliteral(
    <div class="container">
        <div style="font-size: 3rem; margin-bottom: 15px; color: #ef4444;">↓</div>
        <h1 style="font-size: 1.5rem; margin-bottom: 10px;">Lowering Blinds</h1>
        <p style="color: #9ca3af;">Manual control activated</p>
        <p style="color: #9ca3af;">Returning in 3 seconds...</p>
    </div>
    <script>setTimeout(function(){window.location.href='/';}, 3000);</script>
</body></html>)rawliteral";
    
    server.send(200, "text/html", html);
    html = ""; // Free memory
}

void handleSkipDay() {
    lastWatchdog = millis(); // Reset watchdog
    
    String uri = server.uri();
    int dayIndex = uri.substring(6).toInt(); // Extract day index from "/skip/X"
    
    if (dayIndex >= 0 && dayIndex <= 6) {
        skipDays[dayIndex] = !skipDays[dayIndex]; // Toggle the day
        Serial.printf("Day %s skip toggled: %s\n", 
                     getDayName(dayIndex).c_str(), 
                     skipDays[dayIndex] ? "ON" : "OFF");
    }
    
    // Redirect back to main page
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
}

void handleNotFound() {
    server.send(404, "text/plain", "404: Not Found");
}

void setupWebServer() {
    server.on("/", handleRoot);
    server.on("/up", handleUp);
    server.on("/down", handleDown);
    
    // Add handlers for day skip buttons
    for (int i = 0; i < 7; i++) {
        server.on("/skip/" + String(i), handleSkipDay);
    }
    
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("Web server started on port 8080");
    Serial.println("Access at: http://" + WiFi.localIP().toString() + ":8080");
}

// Automatic blinds control logic
void handleAutomaticControl() {
    if (!initialTimeSynced || blindManualControl) return;
    
    time_t localTime = getCurrentLocalTime();
    struct tm *currentTime = localtime(&localTime);
    
    // Skip if this day is disabled
    if (skipDays[currentTime->tm_wday]) {
        return;
    }
    
    int currentState = getCurrentBlindsState();
    
    // Morning schedule: UP
    bool shouldMoveUp = false;
    if (isWeekday(currentTime->tm_wday)) {
        // Weekdays: UP at 6:00
        shouldMoveUp = (currentTime->tm_hour >= 6 && currentTime->tm_hour < 22);
    } else {
        // Weekends: UP at 9:00
        shouldMoveUp = (currentTime->tm_hour >= 9 && currentTime->tm_hour < 22);
    }
    
    // Evening schedule: DOWN at 22:00 (10 PM) for both weekdays and weekends
    bool shouldMoveDown = (currentTime->tm_hour >= 22 || currentTime->tm_hour < 6);
    
    // Execute movements
    if (shouldMoveUp && currentState != 0) {
        Serial.printf("Auto: Moving blinds UP at %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
        moveBlindsUp();
    } else if (shouldMoveDown && currentState != 1) {
        Serial.printf("Auto: Moving blinds DOWN at %02d:%02d\n", currentTime->tm_hour, currentTime->tm_min);
        moveBlindsDown();
    }
}

// Reset manual control at scheduled times
void handleReset() {
    if (!initialTimeSynced) return;
    
    time_t localTime = getCurrentLocalTime();
    struct tm *currentTime = localtime(&localTime);
    
    // Reset at scheduled times
    if ((currentTime->tm_hour == 6 && currentTime->tm_min == 0 && currentTime->tm_sec == 0) || 
        (currentTime->tm_hour == 9 && currentTime->tm_min == 0 && currentTime->tm_sec == 0) || 
        (currentTime->tm_hour == 22 && currentTime->tm_min == 0 && currentTime->tm_sec == 0)) {
        blindManualControl = false;
        Serial.println("Reset manual controls");
    }
}

// System health monitoring
void monitorSystemHealth() {
    unsigned long currentTime = millis();
    
    // Check memory every 5 minutes
    if (currentTime - lastMemoryReport > 300000) {
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t minFreeHeap = ESP.getMinFreeHeap();
        
        Serial.printf("Memory Report - Free: %d bytes, Min Free: %d bytes\n", freeHeap, minFreeHeap);
        
        // If memory is critically low, restart
        if (freeHeap < 10000) {
            Serial.println("Critical memory shortage! Restarting...");
            ESP.restart();
        }
        
        lastMemoryReport = currentTime;
    }
    
    // Watchdog check - restart if no activity for 5 minutes
    if (currentTime - lastWatchdog > WATCHDOG_TIMEOUT) {
        Serial.println("Watchdog timeout! System appears frozen. Restarting...");
        ESP.restart();
    }
}

// Improved WiFi management
void handleWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        reconnectAttempts = 0;
        return;
    }
    
    // WiFi disconnected - try to reconnect
    unsigned long currentTime = millis();
    
    if (currentTime - wifiReconnectTimer > WIFI_RECONNECT_INTERVAL) {
        Serial.printf("WiFi disconnected. Reconnect attempt %d...\n", reconnectAttempts + 1);
        
        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password);
        
        wifiReconnectTimer = currentTime;
        reconnectAttempts++;
        
        // Restart after max attempts
        if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            Serial.println("WiFi reconnection failed after max attempts. Restarting...");
            ESP.restart();
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Smart Blinds Controller...");
    
    // Initialize watchdog
    lastWatchdog = millis();
    
    initializeEEPROM();
    initializeMotorPins();
    setupWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        initializeTime();
        setupWebServer();
    }
    
    Serial.println("Setup complete!");
    Serial.printf("Initial free heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
    // Update watchdog
    lastWatchdog = millis();
    
    // Handle WiFi connection
    handleWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
        updateInternalTime();
        handleReset();
        
        // Run automatic control every 30 seconds to avoid spam
        static unsigned long lastAutoCheck = 0;
        if (millis() - lastAutoCheck > 30000) {
            handleAutomaticControl();
            lastAutoCheck = millis();
        }
    }
    
    // System health monitoring
    monitorSystemHealth();
    
    // Yield to prevent watchdog issues
    yield();
    delay(100);
}