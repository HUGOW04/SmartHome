# Automated Blinds System

## Project Overview
This project is an automated blinds system powered by WiFi and a **NEMA 23 stepper motor** controlled by an **ESP32 microcontroller**.  
Since the motor alone was too weak to lift the blinds, I integrated a **planetary gearbox**. The gearbox increases torque output by a factor of 4–5 using its design where one central *sun gear* is surrounded by four *planet gears*. This evenly distributes the load and provides the extra strength needed without requiring a larger, more power-hungry motor.  
With this setup, the blinds move smoothly and reliably, even under load.

---

## Planetary Gearbox System
- **Gear design:** Central sun gear with four planetary gears.  
- **Purpose:** Torque amplification (4–5×).  
- **Result:** Stable lifting of blinds with a relatively compact motor.  

---

## How the System Works
- **ESP32 control:** Connected to the local WiFi network.  
- **Time synchronization:** Uses **NTP (Network Time Protocol)** for accurate scheduling.  
- **Automatic schedule:**
  - Opens blinds at **6 AM on weekdays**.  
  - Opens blinds at **9 AM on weekends** (for a more relaxed start).  
  - Closes blinds at **10 PM every night** for privacy and security.  

---

## Manual Control and Sleep-In Mode
Originally, the system relied on a second ESP32 with a display to send control signals.  
This was replaced by a simpler, more flexible solution:  

- The main ESP32 now hosts its own **web server**.  
- Features of the web interface:
  - Manually open/close blinds.  
  - Override scheduled operations (e.g., sleep in).  
  - Force blinds to move at any chosen time.  
  - Temporarily disable automation for specific days.  

This gives full flexibility and makes the system easy to manage remotely.

---

## Experimental Setup
- Tested multiple stepper motors to find the right balance of power and efficiency.  
- Adjusted the gear tracks to perfectly fit the curtain setup.  
- Iterative trial-and-error ensured smooth operation without motor overload.  
- The final system achieves **reliable movement with minimal mechanical issues**.  

---

## Demo
<p align="center">
  <img src="https://github.com/user-attachments/assets/cc9a33d3-ace7-4c69-aa76-523b4659701e" alt="Planetary Gearbox" width="30%"/>
  <img src="https://github.com/user-attachments/assets/af963ba9-655c-4fcf-a348-8d5cc893c9a2" alt="Motor" width="52%"/>
  <img src="https://github.com/user-attachments/assets/5b6ac11b-9e64-4f91-8573-2422f959ebd0" alt="Webserver Interface" width="50%"/>
</p>




---

## Future Improvements
- Integration with smart home assistants (Google Home, Alexa).  
- Light sensor input to adjust blinds dynamically.  
- Mobile-friendly control dashboard.  
