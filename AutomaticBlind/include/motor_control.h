#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include <EEPROM.h>

// Motor pin definitions
const int PUL = 25;
const int DIR = 26;
const int ENA = 27;

// Motor settings
const int steps_per_rev = 6400;
const double rotations = 31;   
const int step_delay = 50;

// EEPROM settings
#define EEPROM_ADDR 0

// Function declarations
void initializeMotorPins();
void initializeEEPROM();
void moveBlindsDown();
void moveBlindsUp();
int getCurrentBlindsState();

#endif