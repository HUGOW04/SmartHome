# SmartHome
<img src="https://github.com/user-attachments/assets/2e47d334-a87e-4aaa-b03e-e7bd6665ba10" width="350" height="350">
<img src="https://github.com/user-attachments/assets/865ded09-db8c-4d5d-b11c-25256a82f9b3" width="350" height="350">
<img src="https://github.com/user-attachments/assets/adb93e70-38a5-4d0a-8472-489fb85679d5" width="350" height="350">
<img src="https://github.com/user-attachments/assets/a2984522-11ed-4d66-81f8-9d0860f8cc4d" width="350" height="350">


# Smart Home Automation

## Project Overview
This project revolves around creating a **Smart Home** system that automates both blinds and lighting using a **NEMA 23 stepper motor** and a **Yeelight smart lamp**, all controlled by an **ESP32 microcontroller**. The system integrates WiFi-based automation, providing a smart environment where you can control the blinds and the lighting seamlessly. The blinds are powered by a motor enhanced with a **planetary gearbox**, and the Yeelight lamp is set to follow a specific schedule for a more convenient waking routine.

## How the System Works
The **ESP32 microcontroller** is connected to a local WiFi network and handles automation for both the blinds and the Yeelight lamp. The system is synchronized with **NTP (Network Time Protocol)** to ensure time-based actions are precise.

- **Blinds Automation**: The blinds open automatically at **6 AM on weekdays** and at **9 AM on weekends**, and they close at **10 PM** every day to provide privacy and security.
  
- **Lamp Automation**: The Yeelight smart lamp automatically turns on at **6 AM on weekdays** and **9 AM on weekends**, simulating a natural sunrise to help you wake up gradually. The lamp will **automatically turn off after 1 hour** to ensure you aren't disturbed by the light for too long.

## Manual Control and "Sovmorgon" Feature
The system includes **manual control** via physical buttons on the **ESP32**, allowing you to:

- Move the blinds up or down manually at any time.
- Turn the Yeelight lamp on or off with a button press.

The **"Sovmorgon" (lie-in)** feature allows you to skip the scheduled automation for a day. By pressing a button, you can disable the automatic operation, so the blinds and lamp won't operate at their usual times, perfect for weekends or lazy mornings.

## Key Features
- **Planetary Gearbox for Blinds**: The blinds are operated with a stepper motor enhanced by a **planetary gearbox**, providing up to 4-5 times more torque, making the motor strong enough to move the blinds smoothly even under load.
  
- **Time Synchronization**: The system is synchronized with **NTP** to ensure that all automation happens at the correct time each day.
  
- **Manual Control**: Manual buttons let you control the blinds and Yeelight lamp, providing flexibility in scheduling and overriding automation.

- **Power Failure Recovery**: The system saves the last known position of the blinds using **EEPROM memory**, so after a power failure, the blinds will return to their correct position.

- **Smart Wake-Up**: The Yeelight lamp turns on automatically at scheduled times (6 AM on weekdays and 9 AM on weekends) to simulate sunrise and help you wake up. The light will automatically turn off after **1 hour**, ensuring you're not disturbed by it for too long.

## Benefits
- **Convenience**: Automation makes it easy to control your blinds and lighting based on time or preference.
- **Customizable Wake-Up Routine**: The Yeelight lamp simulates sunrise, providing a gentle wake-up experience, and will automatically turn off after 1 hour for a comfortable morning.
- **Manual Override**: Manual control via buttons allows you to bypass automation whenever you want.
