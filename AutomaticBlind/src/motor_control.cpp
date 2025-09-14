#include "motor_control.h"


void initializeMotorPins() {
    pinMode(PUL, OUTPUT);
    pinMode(DIR, OUTPUT);
    pinMode(ENA, OUTPUT);
    digitalWrite(ENA, HIGH);
}

void initializeEEPROM() {
    EEPROM.begin(sizeof(int));
    
    int storedState;
    EEPROM.get(EEPROM_ADDR, storedState);
    if (storedState != 0 && storedState != 1) {
        EEPROM.put(EEPROM_ADDR, -1);
        EEPROM.commit();
    }
}

int getCurrentBlindsState() {
    int currentState;
    EEPROM.get(EEPROM_ADDR, currentState);
    return currentState;
}

void moveBlindsDown() {
    int lastBlindsState = getCurrentBlindsState();
    
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
        
        // send stuff to webserver
    } else {
        Serial.println("Blinds are already down");
        // send stuff to the webserver
    }
}

void moveBlindsUp() {
    int lastBlindsState = getCurrentBlindsState();
    
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
        
        // send stuff to webserver
    } else {
        Serial.println("Blinds are already up");
        // send stuff to the webserver
    }
}