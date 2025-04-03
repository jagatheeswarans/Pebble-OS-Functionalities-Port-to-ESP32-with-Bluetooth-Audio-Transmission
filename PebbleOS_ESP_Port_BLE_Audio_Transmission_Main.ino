#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/i2s.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
// Add UI-related includes
#include "display_driver.h"
#include "ui_manager.h"
#include "pebble_fonts.h"

// BLE UUIDs
#define SERVICE_UUID        "5F8A9156-41D8-4211-9988-CFF98FE5D4A1"
#define AUDIO_CHAR_UUID     "5F8A9157-41D8-4211-9988-CFF98FE5D4A1"
#define CONTROL_CHAR_UUID   "5F8A9158-41D8-4211-9988-CFF98FE5D4A1"

// I2S Configuration
#define I2S_SAMPLE_RATE     16000
#define I2S_BITS_PER_SAMPLE I2S_BITS_PER_SAMPLE_16BIT
#define I2S_CHANNEL_FORMAT  I2S_CHANNEL_FMT_ONLY_LEFT  // Mono
#define I2S_COMM_FORMAT     I2S_COMM_FORMAT_STAND_I2S

// I2S Pins
#define I2S_MIC_SD          32  // Data pin
#define I2S_MIC_SCK         25   // Clock pin
#define I2S_MIC_WS          33  // Word Select pin

// Optimized buffer settings
#define AUDIO_BUFFER_SIZE   8192      // Raw audio buffer size (8KB)
#define PCM_PACKET_SIZE     512       // Optimized BLE packet size
#define RECORDING_SECONDS   15        // 15 seconds recording
#define QUEUE_SIZE          500       // Queue capacity

// Task configurations
#define STACK_SIZE_AUDIO    8192
#define STACK_SIZE_BLE      8192
#define TASK_PRIORITY_AUDIO 3
#define TASK_PRIORITY_BLE   2

// Button pin definitions
#define UP_BUTTON_PIN 12
#define SELECT_BUTTON_PIN 14
#define DOWN_BUTTON_PIN 27

// App state definitions
#define STATE_WATCHFACE 0
#define STATE_MENU 1
#define STATE_RECORDING 2
#define STATE_ICON_MENU 3
#define STATE_NOTIFICATION 4
#define STATE_SETTINGS 5

// Audio buffer structure for the queue
typedef struct {
  uint8_t *data;  // Pointer to data in PSRAM
  size_t size;
} audio_packet_t;

// PSRAM-based Queue implementation
struct PSRAMQueue {
  audio_packet_t* items;
  int capacity;
  int head;
  int tail;
  int count;
  SemaphoreHandle_t mutex;
  SemaphoreHandle_t notEmpty;
  SemaphoreHandle_t notFull;
};

PSRAMQueue* audioQueue = NULL;

// BLE characteristics
BLECharacteristic *pAudioCharacteristic;
BLECharacteristic *pControlCharacteristic;
bool deviceConnected = false;
bool recordingActive = false;

// UI Manager
UIManager uiManager;
int currentState = STATE_WATCHFACE;
int previousState = STATE_WATCHFACE;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 200; // Debounce time in milliseconds

// Control commands
enum ControlCommands {
  CMD_START_RECORDING = 1,
  CMD_STOP_RECORDING = 2,
  CMD_STATUS_REQUEST = 3
};

// Function prototypes
void checkButtons();
void handleUpButton();
void handleSelectButton();
void handleDownButton();
void showNotification(const char* title, const char* message);
void drawSettingsScreen();
void updateDisplayState();

// Server callbacks
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
      showNotification("Bluetooth", "Device connected");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      recordingActive = false;  // Stop recording if client disconnects
      Serial.println("Device disconnected");
      showNotification("Bluetooth", "Device disconnected");
      // Restart advertising when disconnected
      BLEDevice::startAdvertising();
      // Return to watchface if recording was active
      updateDisplayState();
    }
};

// Control characteristic callbacks
class ControlCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        uint8_t command = value[0];
        
        switch (command) {
          case CMD_START_RECORDING:
            if (!recordingActive) {
              recordingActive = true;
              Serial.println("Starting audio recording");
              // Update display to show recording screen
              currentState = STATE_RECORDING;
              uiManager.drawRecordingPage();
            }
            break;
            
          case CMD_STOP_RECORDING:
            if (recordingActive) {
              recordingActive = false;
              Serial.println("Stopping audio recording");
              // Return to watchface after recording stops
              updateDisplayState();
            }
            break;
            
          case CMD_STATUS_REQUEST:
            {
              uint8_t status = recordingActive ? 1 : 0;
              pControlCharacteristic->setValue(&status, 1);
              pControlCharacteristic->notify();
            }
            break;
        }
      }
    }
};

// Create a queue in PSRAM
PSRAMQueue* createPSRAMQueue(int size) {
  // Allocate queue structure in PSRAM
  PSRAMQueue* queue = (PSRAMQueue*)heap_caps_malloc(sizeof(PSRAMQueue), MALLOC_CAP_SPIRAM);
  if (!queue) {
    Serial.println("Failed to allocate queue structure in PSRAM");
    return NULL;
  }
  
  // Allocate items array in PSRAM
  queue->items = (audio_packet_t*)heap_caps_malloc(size * sizeof(audio_packet_t), MALLOC_CAP_SPIRAM);
  if (!queue->items) {
    Serial.println("Failed to allocate queue items in PSRAM");
    heap_caps_free(queue);
    return NULL;
  }
  
  // Initialize queue properties
  queue->capacity = size;
  queue->head = 0;
  queue->tail = 0;
  queue->count = 0;
  
  // Create synchronization primitives
  queue->mutex = xSemaphoreCreateMutex();
  queue->notEmpty = xSemaphoreCreateBinary();
  queue->notFull = xSemaphoreCreateBinary();
  
  // Initially, the queue is empty but not full
  xSemaphoreGive(queue->notFull);
  
  Serial.printf("PSRAM Queue created with capacity %d\n", size);
  
  return queue;
}

// Send a packet to the PSRAM queue
bool psramQueueSend(PSRAMQueue* queue, audio_packet_t* packet, TickType_t timeout) {
  if (!queue) return false;
  
  // Wait until the queue is not full
  if (xSemaphoreTake(queue->notFull, timeout) != pdTRUE) {
    return false;  // Timeout
  }
  
  // Take mutex to protect queue access
  xSemaphoreTake(queue->mutex, portMAX_DELAY);
  
  // Add item to queue
  queue->items[queue->tail] = *packet;
  queue->tail = (queue->tail + 1) % queue->capacity;
  queue->count++;
  
  // Signal that the queue is not empty
  xSemaphoreGive(queue->notEmpty);
  
  // If queue is full, don't signal notFull
  if (queue->count < queue->capacity) {
    xSemaphoreGive(queue->notFull);
  }
  
  // Release mutex
  xSemaphoreGive(queue->mutex);
  
  return true;
}

// Receive a packet from the PSRAM queue
bool psramQueueReceive(PSRAMQueue* queue, audio_packet_t* packet, TickType_t timeout) {
  if (!queue) return false;
  
  // Wait until the queue is not empty
  if (xSemaphoreTake(queue->notEmpty, timeout) != pdTRUE) {
    return false;  // Timeout
  }
  
  // Take mutex to protect queue access
  xSemaphoreTake(queue->mutex, portMAX_DELAY);
  
  // Get item from queue
  *packet = queue->items[queue->head];
  queue->head = (queue->head + 1) % queue->capacity;
  queue->count--;
  
  // Signal that the queue is not full
  xSemaphoreGive(queue->notFull);
  
  // If queue is not empty, signal notEmpty
  if (queue->count > 0) {
    xSemaphoreGive(queue->notEmpty);
  }
  
  // Release mutex
  xSemaphoreGive(queue->mutex);
  
  return true;
}

// Get available space in the queue
int psramQueueSpacesAvailable(PSRAMQueue* queue) {
  if (!queue) return 0;
  
  xSemaphoreTake(queue->mutex, portMAX_DELAY);
  int spaces = queue->capacity - queue->count;
  xSemaphoreGive(queue->mutex);
  
  return spaces;
}

// Check if PSRAM is available and report memory
void check_psram() {
  if (ESP.getPsramSize() > 0) {
    Serial.println("PSRAM available: " + String(ESP.getPsramSize() / 1024) + " KB");
  } else {
    Serial.println("WARNING: PSRAM not detected! This code requires PSRAM.");
  }
}

// Initialize I2S for microphone input
void setup_i2s() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FORMAT,
    .communication_format = I2S_COMM_FORMAT,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 16,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };
  
  esp_err_t result = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (result != ESP_OK) {
    Serial.printf("Error installing I2S driver: %d\n", result);
    return;
  }
  
  result = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (result != ESP_OK) {
    Serial.printf("Error setting I2S pins: %d\n", result);
    return;
  }
  
  Serial.println("I2S microphone initialized");
}

// Setup BLE service for audio streaming
void setup_ble() {
  // Initialize BLE
  BLEDevice::init("PebbleAudio");
  
  // Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  
  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Create Audio Characteristic with larger MTU
  pAudioCharacteristic = pService->createCharacteristic(
                            AUDIO_CHAR_UUID,
                            BLECharacteristic::PROPERTY_NOTIFY |
                            BLECharacteristic::PROPERTY_READ
                          );
  pAudioCharacteristic->addDescriptor(new BLE2902());
  
  // Create Control Characteristic
  pControlCharacteristic = pService->createCharacteristic(
                            CONTROL_CHAR_UUID,
                            BLECharacteristic::PROPERTY_WRITE |
                            BLECharacteristic::PROPERTY_READ |
                            BLECharacteristic::PROPERTY_NOTIFY
                          );
  pControlCharacteristic->setCallbacks(new ControlCallbacks());
  pControlCharacteristic->addDescriptor(new BLE2902());
  
  // Start the service
  pService->start();
  
  // Set the maximum MTU size to improve throughput
  BLEDevice::setMTU(512);
  
  // Start advertising
  BLEAdvertisementData advData;
  advData.setName("PebbleAudio");
  advData.setCompleteServices(BLEUUID(SERVICE_UUID));
  
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();
  
  Serial.println("BLE initialized with 512 MTU, advertising started");
}

// Audio recording task - reads from I2S and sends to queue
void audioTask(void *parameter) {
  // Allocate I2S buffer in PSRAM
  uint8_t* i2s_buffer = (uint8_t*)heap_caps_malloc(AUDIO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  if (i2s_buffer == NULL) {
    Serial.println("Failed to allocate I2S buffer in PSRAM, halting");
    vTaskDelete(NULL);
    return;
  }
  
  unsigned long recordingStartTime = 0;
  
  while (true) {
    if (recordingActive) {
      if (recordingStartTime == 0) {
        recordingStartTime = millis();
        Serial.println("Recording started");
        
        // Update UI to show recording screen if not already showing
        if (currentState != STATE_RECORDING) {
          currentState = STATE_RECORDING;
          uiManager.drawRecordingPage();
        }
      }
      
      // Check if we've reached the time limit
      unsigned long currentTime = millis();
      if (currentTime - recordingStartTime >= RECORDING_SECONDS * 1000) {
        recordingActive = false;
        recordingStartTime = 0;
        Serial.println("Recording completed (time limit reached)");
        
        // Notify completion via control characteristic
        uint8_t status = 0; // Stopped
        pControlCharacteristic->setValue(&status, 1);
        pControlCharacteristic->notify();
        
        // Return to watchface or previous state
        updateDisplayState();
        
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      
      // Read audio data from I2S
      size_t bytes_read = 0;
      esp_err_t result = i2s_read(I2S_NUM_0, i2s_buffer, AUDIO_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
      
      if (result == ESP_OK && bytes_read > 0) {
        // Break audio data into smaller chunks and send to queue
        for (int offset = 0; offset < bytes_read; offset += PCM_PACKET_SIZE) {
          int chunk_size = min((int)PCM_PACKET_SIZE, (int)(bytes_read - offset));
          
          // Allocate packet data in PSRAM
          uint8_t* packet_data = (uint8_t*)heap_caps_malloc(chunk_size, MALLOC_CAP_SPIRAM);
          if (packet_data == NULL) {
            continue; // Skip this chunk if allocation fails
          }
          
          // Copy data to the PSRAM buffer
          memcpy(packet_data, i2s_buffer + offset, chunk_size);
          
          // Create packet structure
          audio_packet_t packet;
          packet.data = packet_data;
          packet.size = chunk_size;
          
          // Send to queue with timeout using our PSRAM queue
          if (!psramQueueSend(audioQueue, &packet, pdMS_TO_TICKS(10))) {
            // Free memory if queue is full
            heap_caps_free(packet_data);
          }
        }
      }
    } else {
      recordingStartTime = 0;
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  
  // This should never be reached
  heap_caps_free(i2s_buffer);
  vTaskDelete(NULL);
}

// BLE transmission task - reads from queue and sends via BLE
void bleTask(void *parameter) {
  audio_packet_t packet;
  unsigned long lastStatsTime = 0;
  uint32_t packetsProcessed = 0;
  
  while (true) {
    // Wait for data from the queue using our PSRAM queue
    if (psramQueueReceive(audioQueue, &packet, pdMS_TO_TICKS(10))) {
      if (deviceConnected) {
        // Optimize BLE chunk size - use 200 bytes for better throughput
        const size_t ble_chunk_size = 200;
        
        for (size_t i = 0; i < packet.size; i += ble_chunk_size) {
          if (!deviceConnected || !recordingActive) {
            break; // Exit the loop if conditions change
          }
          
          size_t chunk_size = min(ble_chunk_size, packet.size - i);
          
          // Send the audio packet over BLE
          pAudioCharacteristic->setValue(packet.data + i, chunk_size);
          pAudioCharacteristic->notify();
          
          // Reduced delay between chunks - just enough to not overwhelm
          vTaskDelay(pdMS_TO_TICKS(2));
        }
        
        packetsProcessed++;
        
        // Only log every 50 packets to reduce overhead
        if (packetsProcessed % 50 == 0) {
          unsigned long currentTime = millis();
          if (currentTime - lastStatsTime >= 5000) {
            lastStatsTime = currentTime;
            Serial.printf("BLE: %u packets sent\n", packetsProcessed);
          }
        }
      }
      
      // Free the PSRAM after we've sent the packet
      heap_caps_free(packet.data);
    } else {
      // Short yield if no data
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

// UI related functions
void checkButtons() {
  // Don't process buttons during recording to avoid accidental stops
  if (recordingActive) {
    return;
  }
  
  // Read button states
  bool upPressed = digitalRead(UP_BUTTON_PIN) == LOW;
  bool selectPressed = digitalRead(SELECT_BUTTON_PIN) == LOW;
  bool downPressed = digitalRead(DOWN_BUTTON_PIN) == LOW;
  
  // Check if debounce time has passed
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress < debounceTime) {
    return;
  }
  
  // Handle button presses
  if (upPressed) {
    handleUpButton();
    lastButtonPress = currentTime;
  } else if (selectPressed) {
    handleSelectButton();
    lastButtonPress = currentTime;
  } else if (downPressed) {
    handleDownButton();
    lastButtonPress = currentTime;
  }
}

void handleUpButton() {
  Serial.println("UP button pressed");
  
  switch(currentState) {
    case STATE_WATCHFACE:
      // Quick access to recording
      recordingActive = true;
      currentState = STATE_RECORDING;
      uiManager.drawRecordingPage();
      break;
      
    case STATE_MENU:
      uiManager.menuPrevious();
      uiManager.drawMenu();
      break;
      
    case STATE_ICON_MENU:
      uiManager.menuPrevious();
      uiManager.drawIconMenu();
      break;
      
    case STATE_SETTINGS:
      // Handle settings navigation
      drawSettingsScreen();
      break;
  }
}

void handleSelectButton() {
  Serial.println("SELECT button pressed");
  
  switch(currentState) {
    case STATE_MENU:
      // Check if "Record Audio" menu item is selected
      if (uiManager.getCurrentMenuItem() == 0) {
        recordingActive = true;
        currentState = STATE_RECORDING;
        uiManager.drawRecordingPage();
      } else {
        // Handle other menu selections
        currentState = STATE_WATCHFACE;
        uiManager.drawWatchFace();
      }
      break;
      
    default:
      // Reset to watchface for other states
      recordingActive = false; // Stop recording if active
      currentState = STATE_WATCHFACE;
      uiManager.drawWatchFace();
      break;
  }
}

void handleDownButton() {
  Serial.println("DOWN button pressed");
  
  switch(currentState) {
    case STATE_WATCHFACE:
      // Show menu
      currentState = STATE_MENU;
      uiManager.drawMenu();
      break;
      
    case STATE_MENU:
      uiManager.menuNext();
      uiManager.drawMenu();
      break;
      
    case STATE_ICON_MENU:
      uiManager.menuNext();
      uiManager.drawIconMenu();
      break;
      
    case STATE_SETTINGS:
      // Handle settings navigation
      drawSettingsScreen();
      break;
  }
}

void showNotification(const char* title, const char* message) {
  Serial.println("Showing notification");
  // Save previous state to return to
  previousState = currentState;
  currentState = STATE_NOTIFICATION;
  
  // Display notification
  uiManager.drawNotificationCard(title, message);
  
  // After 3 seconds, return to previous state
  delay(3000);
  currentState = previousState;
  updateDisplayState();
}

void drawSettingsScreen() {
  Serial.println("Drawing settings screen");
  display_clear();
  
  // Draw status bar at the top
  uiManager.drawStatusBar(true, deviceConnected, recordingActive);
  
  // Draw settings title using Pebble font
  uiManager.drawTextWithFont(10, 30, "Settings", FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
  
  // Draw brightness setting
  uiManager.drawTextWithFont(10, 50, "Brightness:", FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
  uiManager.drawProgressBar(10, 65, SCREEN_WIDTH - 40, 15, 75, COLOR_BLUE);
  
  // Draw volume setting
  uiManager.drawTextWithFont(10, 90, "Volume:", FONT_KEY_GOTHIC_14, ALIGN_LEFT, COLOR_WHITE);
  uiManager.drawProgressBar(10, 105, SCREEN_WIDTH - 40, 15, 60, COLOR_GREEN);
  
  // Draw action bar on the right side
  uiManager.drawActionBar(true, true, true);
}

// Helper function to update display based on current state
void updateDisplayState() {
  switch(currentState) {
    case STATE_WATCHFACE:
      uiManager.drawWatchFace();
      break;
    case STATE_MENU:
      uiManager.drawMenu();
      break;
    case STATE_RECORDING:
      if (recordingActive) {
        uiManager.drawRecordingPage();
      } else {
        currentState = STATE_WATCHFACE;
        uiManager.drawWatchFace();
      }
      break;
    case STATE_ICON_MENU:
      uiManager.drawIconMenu();
      break;
    case STATE_NOTIFICATION:
      // Should only happen temporarily
      currentState = STATE_WATCHFACE;
      uiManager.drawWatchFace();
      break;
    case STATE_SETTINGS:
      drawSettingsScreen();
      break;
  }
}

// Main setup function
void setup() {
  Serial.begin(115200);
  Serial.println("Starting PebbleAudio with Display");
  
  heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);

  // Check PSRAM status
  check_psram();
  
  // Create PSRAM-based queue for audio packets
  audioQueue = createPSRAMQueue(QUEUE_SIZE);
  if (audioQueue == NULL) {
    Serial.println("Error creating PSRAM audio queue, halting");
    while(1);
  }
  
  // Set up button pins
  pinMode(UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SELECT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize display
  display_init();
  
  // Initialize UI manager
  uiManager.begin();
  
  // Set time (replace with RTC code if available)
  uiManager.setTime(10, 30, 0);
  
  // Set up regular menu items
  const char* menuItems[] = {
    "Record Audio",
    "Settings",
    "Apps",
    "Notifications"
  };
  uiManager.setMenuItems(menuItems, 4);
  
  // Setup components
  setup_i2s();
  setup_ble();
  
  // Create tasks
  xTaskCreatePinnedToCore(
    audioTask,
    "AudioTask",
    STACK_SIZE_AUDIO,
    NULL,
    TASK_PRIORITY_AUDIO,
    NULL,
    0
  );
  
  xTaskCreatePinnedToCore(
    bleTask,
    "BLETask",
    STACK_SIZE_BLE,
    NULL,
    TASK_PRIORITY_BLE,
    NULL,
    1
  );
  
  // Draw initial watch face
  currentState = STATE_WATCHFACE;
  uiManager.drawWatchFace();
  
  Serial.println("System initialized");
}

// Main loop function 
void loop() {
  
  // Check for button presses - ONLY when not recording
  if (!recordingActive) {
    checkButtons();
  }
  
  // Update display based on current state - ONLY when not recording
  if (!recordingActive) {
    switch(currentState) {
      case STATE_WATCHFACE:
        uiManager.updateTime();
        break;
      // Other cases...
    }
  } else {
    // When recording, just make sure we're showing the recording screen
    if (currentState != STATE_RECORDING) {
      currentState = STATE_RECORDING;
      uiManager.drawRecordingPage();
    }
  }
  // Update display based on current state
  switch(currentState) {
    case STATE_WATCHFACE:
      uiManager.updateTime();
      
      // Show recording status on watchface if active
      if (recordingActive && currentState != STATE_RECORDING) {
        currentState = STATE_RECORDING;
        uiManager.drawRecordingPage();
      }
      break;
      
    case STATE_RECORDING:
      // If recording stopped, return to watchface
      if (!recordingActive) {
        currentState = STATE_WATCHFACE;
        uiManager.drawWatchFace();
      }
      break;
      
    // Other states don't need continuous updates
  }
  
  // Minimized status reports
  static unsigned long lastStatusTime = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastStatusTime >= 10000) { // Every 10 seconds
    lastStatusTime = currentTime;
    
    Serial.printf("Status: BLE %s, Recording %s, Free PSRAM: %d KB\n", 
                 deviceConnected ? "connected" : "disconnected",
                 recordingActive ? "active" : "inactive",
                 ESP.getFreePsram() / 1024);
  }
  
  delay(100);
}