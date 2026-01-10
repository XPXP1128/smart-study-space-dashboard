# USM Library Monitor (Smart Study Space Monitoring System)

This project is a pilot Smart Study Space Monitoring System for the USM Library.
An ESP32 node reads seat occupancy and lighting conditions, uploads data to Firebase Realtime Database (RTDB), and a React dashboard displays real-time status and historical analytics.

## Features
- Seat states: **AVAILABLE / RESERVED / OCCUPIED**
- Lighting states: **BRIGHT / LOW LIGHT**
- RTDB Live updates every ~2 seconds
- Historical logs pushed every ~30 seconds
- Dashboard includes:
  - Live sensor readings + last update time
  - History charts + summary statistics
  - Online/Offline indicator using `updatedAt`

---

## Repository Structure
- `smart-study-space-dashboard/` → React dashboard (Firebase RTDB)
- `study_space_monitor/` → ESP32 firmware (Arduino IDE)

---

## Hardware Used
**Controller**
- ESP32 (NodeMCU ESP32)

**Sensors**
- Ultrasonic sensor (distance)
- FSR pressure sensor (occupied detection)
- LDR module (AO/DO for brightness)

**Outputs**
- OLED display (I2C)
- LEDs (Red / Green / Yellow)

---

## Firebase RTDB Data Paths
- Live node: `/USMLibrary/Desk01/`
- Logs node: `/USMLibrary/Desk01Logs/`

Live node fields include:
- `seatStatus`, `distance_cm`, `fsrValue`, `lightStatus`, `ldrAO`, `ldrDO`, `updatedAt`

---

## Setup Instructions

### 1) Firebase Setup (RTDB)
1. Create a Firebase project
2. Enable **Realtime Database (RTDB)**
3. Enable **Email/Password Authentication**
4. Create a user for ESP32 login (example: `esp32@studyspace.com`)
5. Prepare:
   - RTDB URL
   - API Key
   - Web config

> Note: Some institutional Wi-Fi networks may block TLS/SSL traffic.
> If ESP32 shows SSL timeout or token errors, use a stable hotspot network.

---

### 2) Run the React Dashboard
**Requirements**
- Node.js + npm (Node 18+ recommended)

Steps:
```bash
cd smart-study-space-dashboard
npm install
npm start
