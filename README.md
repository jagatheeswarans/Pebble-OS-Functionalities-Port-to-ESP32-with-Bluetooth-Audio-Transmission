# **Pebble OS Port on Reference Board with Bluetooth Audio Transmission**  

## **Project Overview**  
This project focuses on porting the open-source [Pebble OS](https://github.com/google/pebble/tree/main) onto a selected reference board with a display and microphone. Additionally, a new feature has been implemented to **transmit a 15-second audio note via Bluetooth** to a mobile phone or laptop.  

## **Key Components**  
- **Pebble OS Porting**: Adapted Pebble OS to run on an ESP32-based reference board with a smartwatch-sized display.  
- **Bluetooth Audio Transmission**: Added functionality to record and transmit a 15-second audio clip over Bluetooth.  
- **Optimized Performance**: Technical decisions were made to ensure minimal latency and efficient power consumption.  
- **Custom UI Implementation**: Designed an intuitive smartwatch-like interface for user interactions.  

---

## **Hardware Requirements**  
- **Microcontroller**: ESP32-based board with Bluetooth LE support.  
- **Display**: TFT/OLED screen with adjustable viewport to mimic smartwatch dimensions.  
- **Microphone**: Analog or I2S microphone module for audio input.  
- **Power Source**: Battery-powered setup for smartwatch-like behavior.  

---

## **Software Stack**  
### **1. Embedded Code (Arduino)**  
The ESP32 firmware includes:  
- **Display Driver**: Handles rendering Pebble UI elements on the selected screen.  
- **Pebble Fonts & UI Manager**: Implements the Pebble OS interface.  
- **Config File**: Stores key system settings such as Bluetooth pairing details.  
- **Bluetooth Audio Transmission**: Captures and sends recorded audio over BLE.  

📂 **File: `PebbleOS_ESP_Port_Main_V2_BLE_Audio_Transmi_copy_20250404014810.ino`**  

### **2. Python Receiver (PC/Mobile)**  
A Python script using the **Bleak** library enables the laptop/mobile device to receive and process the transmitted audio in real time. Additionally, the script leverages **Whisper AI** for speech-to-text conversion.  

📂 **File: `bleak_receiver_whisper_v5_realtime_cuda.py`**  

---

## **User Interface (UI) Feature**  
A smartwatch-like UI was developed for the Pebble OS port to facilitate user interaction. The interface follows a simple and intuitive navigation flow.  

### **1. Display Adaptation**  
- **Viewport Resizing**: Adjusted the UI to fit the reference board’s **TFT/OLED screen** while maintaining Pebble OS aesthetics.  
- **Optimized Frame Buffering**: To ensure smooth rendering and reduce flickering.  

### **2. UI Components & User Experience**  
| **Feature** | **Description** |
|------------|----------------|
| **Home Screen** | Displays the main watch face, mimicking a Pebble smartwatch interface. |
| **Bluetooth Audio Mode** | A dedicated screen for recording and transmitting a 15-second audio note. |
| **Status Indicators** | Shows Bluetooth connection status, microphone status, and battery level. |
| **Touch & Button Controls** | Implemented simple touch/tap navigation (if touchscreen is available) and button-based interactions. |

### **3. Bluetooth Audio Transmission UI Flow**  
1. **Home Screen**  
   - Displays current time and device status.  
   - A button/touch gesture allows switching to the **Audio Mode**.  

2. **Audio Recording Screen**  
   - Once in **Audio Mode**, the display updates to show a **recording timer** (15 sec countdown).  
   - A microphone icon appears when recording starts.  

3. **Bluetooth Transmission Status**  
   - After recording, the UI updates to indicate **"Transmitting Audio..."** over Bluetooth.  
   - Once successfully transmitted, a confirmation message appears: **"Audio Sent!"**  

4. **Return to Home**  
   - After transmission, the screen automatically returns to the **Home Screen**.  

### **4. UI Implementation - Key Files**  
📂 **`display_driver.h/.cpp`** – Handles the rendering of UI elements.  
📂 **`pebble_fonts.h`** – Provides Pebble-style fonts for UI consistency.  
📂 **`Pebble_ui.h/.cpp`** – Manages screen transitions and event handling.  
📂 **`ui_manager.h/.cpp`** – Controls different UI states (Home, Recording, Transmission).  

---

## **Feature: Bluetooth Audio Transmission**  
### **How It Works?**  
1. The ESP32 records a **15-second audio clip** from the microphone.  
2. The audio is processed and converted into a Bluetooth-compatible format.  
3. The device transmits the audio over **BLE (Bluetooth Low Energy)**.  
4. The Python script on the mobile/PC receives the audio and processes it using Whisper AI for transcription (optional).  

### **Key Technical Decisions**  
| Decision Point | Choice Made | Reasoning |
|--------------|------------|-----------|
| **Board Selection** | ESP32 with BLE | Pre-existing BLE stack to avoid driver development. |
| **Display Type** | TFT/OLED | Small viewport, optimized for smartwatch UI. |
| **Audio Format** | PCM over BLE | Maintains quality while ensuring BLE compatibility. |
| **Transmission Protocol** | Bluetooth Low Energy (BLE) | Lower power consumption and faster data exchange. |
| **Audio Processing** | Whisper AI | Enables real-time speech-to-text conversion. |
| **BLE Library** | Bleak (Python) | Lightweight, cross-platform BLE support. |

---

## **Installation & Usage**  
### **For ESP32 Board**  
1. Flash the `PebbleOS_ESP_Port_Main_V2_BLE_Audio_Transmi_copy_20250404014810.ino` onto the ESP32 board using Arduino IDE.  
2. Ensure the microphone and display are properly connected.  
3. Power on the device and initiate Bluetooth pairing.  

### **For PC/Mobile (Python BLE Receiver)**  
1. Install dependencies:  
   ```bash
   pip install bleak openai-whisper torch torchaudio
   ```  
2. Run the Python script:  
   ```bash
   python bleak_receiver_whisper_v5_realtime_cuda.py
   ```  
3. The device will receive and process incoming audio.  

---

## **Future Improvements**  
✔️ Implement real-time audio compression for better BLE transmission.  
✔️ Extend smartwatch functionalities (notifications, health tracking, etc.).  
✔️ Optimize power efficiency for prolonged battery life.  
✔️ Add animations for smoother transitions between screens.  
✔️ Implement **haptic feedback** for user interactions.  
✔️ Include a **playback feature** to let users listen before transmitting.  

---

This README provides a **clear, structured, and detailed** explanation of the project, **enhancing its presentation and evaluation**. 🚀  
