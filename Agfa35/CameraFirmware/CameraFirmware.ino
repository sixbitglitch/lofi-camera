/*
 * Lofi Camera Firmware V2
 * Clean, optimized version of the DIY LDR camera
 * 
 * Features:
 * - Clean code structure with proper organization
 * - Better variable naming and documentation
 * - Improved exposure control
 * - Fixed bugs in autoExposure function
 * - Better error handling
 * - Optimized sensor reading
 * - Cleaner display graphics
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <EEPROM.h>
#include <math.h>

// Hardware Configuration
#define LED_PIN 13
#define BUTTON_PIN 7

// Display Configuration
#define TFT_CS    10
#define TFT_RST   8
#define TFT_DC    9

// Camera Configuration
#define PHOTO_COUNT_MAX MAX_IMAGES_IN_EEPROM  // 28 photos max
#define IMAGE_WIDTH 7
#define IMAGE_HEIGHT 5
#define TOTAL_PIXELS (IMAGE_WIDTH * IMAGE_HEIGHT)

// EEPROM Layout (Arduino Nano has 1024 bytes)
// 0-1:     Photo count (2 bytes)
// 2-3:     Camera config flags (2 bytes)
// 4-7:     Reserved for future use (4 bytes)
// 8-11:    Magic signature "CAM1" (4 bytes) for data validation
// 12-15:   EEPROM version (4 bytes)
// 16-19:   Total photos taken (4 bytes, for statistics)
// 20-23:   Reserved (4 bytes)
// 24-1023: Image data (1000 bytes = 28 photos max, 35 bytes each)
// 1024+:  Calibration data (stored in separate area if needed)

// EEPROM Addresses
#define PHOTO_COUNT_ADDR 0
#define CAMERA_CONFIG_ADDR 2
#define MAGIC_SIGNATURE_ADDR 8
#define EEPROM_VERSION_ADDR 12
#define TOTAL_PHOTOS_ADDR 16
#define IMAGE_DATA_START_ADDR 24
#define MAX_IMAGES_IN_EEPROM 14  // (1024 - 24) / 70 = 14.28
#define BYTES_PER_IMAGE (TOTAL_PIXELS * 2)  // 70 bytes per image (16-bit)
#define CALIBRATION_OFFSET_ADDR 1024  // After EEPROM, stored in separate area

// Camera Config Flags (bit flags)
#define CONFIG_CALIBRATED 0x01
#define CONFIG_AUTO_EXPOSURE 0x02
#define CONFIG_DEBUG_MODE 0x04

// EEPROM Version
#define EEPROM_VERSION 1

// Sensor Row Pins
#define ROW1_PIN 6
#define ROW2_PIN 5
#define ROW3_PIN 4
#define ROW4_PIN 3
#define ROW5_PIN 2

// Sensor Column Pins (A0-A6)
#define COL_START_PIN A0
#define COL_COUNT 7

// Display Settings
#define PIXEL_SIZE 8
#define READ_DELAY 10
#define UI_UPDATE_INTERVAL 50

// Sensor Settings
#define SENSOR_MAX_VALUE 1023

// UI Colors
#define UI_BG_COLOR ST77XX_BLACK
#define UI_TEXT_COLOR ST77XX_WHITE
#define UI_ACCENT_COLOR ST77XX_CYAN
#define UI_WARNING_COLOR ST77XX_YELLOW
#define UI_ERROR_COLOR ST77XX_RED
#define UI_SUCCESS_COLOR ST77XX_GREEN

// Strings stored in PROGMEM to save RAM
const char PROGMEM str_lofi_camera[] = "Lofi Camera";
const char PROGMEM str_v2_ready[] = "V2 Ready";
const char PROGMEM str_photos[] = "Photos: ";
const char PROGMEM str_enhance[] = "Enhance: ";
const char PROGMEM str_normal[] = "NORMAL";
const char PROGMEM str_enhanced[] = "ENHANCED";
const char PROGMEM str_ultra[] = "ULTRA";
const char PROGMEM str_photo[] = "Photo ";
const char PROGMEM str_captured[] = "Captured!";
const char PROGMEM str_press_button[] = "Press button to continue";
const char PROGMEM str_gallery[] = "Gallery: ";
const char PROGMEM str_no_photos[] = "No Photos";
const char PROGMEM str_long_press[] = "Long press to exit";
// Calibration strings removed
const char PROGMEM str_knob_navigate[] = "Knob: Navigate";

// Global Variables
Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Helper function to print PROGMEM strings
void printProgMem(const char* str) {
  char c;
  while ((c = pgm_read_byte(str++)) != 0) {
    display.print(c);
  }
}

// Calibration functions removed

uint16_t photoCount = 0;
uint16_t sensorData[IMAGE_HEIGHT][IMAGE_WIDTH];
uint16_t imageData[TOTAL_PIXELS];
uint8_t pixelReadCount = 0; // Reduced from uint16_t to uint8_t (max 255 is plenty)
float exposureTime = 0.1; // Base exposure time
float enhanceMultiplier = 1.0; // Magic enhance multiplier
uint8_t enhanceMode = 0; // 0=normal, 1=enhanced, 2=ultra (reduced from int)
uint16_t minValue = 1023;
uint16_t maxValue = 0;

// Calibration removed - using raw sensor values

// Button state tracking
bool lastButtonState = HIGH;
bool buttonPressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// UI Update timing
unsigned long lastUIUpdate = 0;

// Gallery mode
bool galleryMode = false;
uint8_t currentGalleryImage = 0; // Reduced from int to uint8_t (max 14 images)
unsigned long lastButtonPress = 0;
const unsigned long LONG_PRESS_TIME = 1000; // 1 second for long press

// Streaming mode
bool streamingMode = false;
uint32_t lastStreamUpdate = 0;
const uint32_t STREAM_INTERVAL = 50; // Stream every 50ms (20 FPS) - maximum speed

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting Lofi Camera V2...");
  
  // Initialize hardware
  initializePins();
  initializeDisplay();
  
  // Simple EEPROM check
  if (!validateEEPROM()) {
    Serial.println("Initializing EEPROM...");
    initializeEEPROM();
  }
  
  // Load photo count
  readPhotoCount();
  Serial.print("Photo count: ");
  Serial.println(photoCount);
  
  // Calibration removed - using raw sensor values
  
  // Display startup message
  display.fillScreen(UI_BG_COLOR);
  display.setTextColor(UI_SUCCESS_COLOR);
  display.setTextSize(2);
  display.setCursor(10, 30);
  printProgMem(str_lofi_camera);
  display.setTextSize(1);
  display.setCursor(10, 60);
  printProgMem(str_v2_ready);
  display.setCursor(10, 80);
  printProgMem(str_photos);
  display.print(photoCount);
  
  delay(2000);
  
  Serial.println("Agfa35 Camera V2 Ready");
  
  // Automatically send stored images after startup
  if (photoCount > 0) {
    displayStoredImages();
  }
}

void initializePins() {
  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Button with pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Sensor row pins
  pinMode(ROW1_PIN, OUTPUT);
  pinMode(ROW2_PIN, OUTPUT);
  pinMode(ROW3_PIN, OUTPUT);
  pinMode(ROW4_PIN, OUTPUT);
  pinMode(ROW5_PIN, OUTPUT);
  
  // Initialize all rows to LOW
  digitalWrite(ROW1_PIN, LOW);
  digitalWrite(ROW2_PIN, LOW);
  digitalWrite(ROW3_PIN, LOW);
  digitalWrite(ROW4_PIN, LOW);
  digitalWrite(ROW5_PIN, LOW);
}

void initializeDisplay() {
  display.initR(INITR_144GREENTAB);
  display.setRotation(0); // Set rotation to portrait mode (128x160)
  display.fillScreen(UI_BG_COLOR);
  display.setTextColor(UI_ACCENT_COLOR);
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Lofi Camera");
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.println("Agfa35 V2");
  display.setCursor(0, 50);
  display.println("HDR Ready");
  delay(1500);
  display.fillScreen(UI_BG_COLOR);
}

// Calibration function removed

// Calibration function removed

// Calibration functions removed

void formatEEPROM() {
  Serial.println("FORMATTING EEPROM - This will reset photo count!");
  
  display.fillScreen(UI_BG_COLOR);
  display.setCursor(0, 20);
  display.setTextColor(UI_ERROR_COLOR);
  display.setTextSize(2);
  display.println("FORMATTING");
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.setTextColor(UI_WARNING_COLOR);
  display.println("EEPROM...");
  display.setCursor(0, 60);
  display.println("All photos will be");
  display.println("deleted!");
  
  // Reset photo count
  photoCount = 0;
  writePhotoCount(0);
  
  // Calibration removed
  
  delay(3000);
  display.fillScreen(UI_BG_COLOR);
}

// EEPROM Management Functions
bool validateEEPROM() {
  // Check magic signature
  char sig[4];
  for (uint8_t i = 0; i < 4; i++) {
    sig[i] = EEPROM.read(MAGIC_SIGNATURE_ADDR + i);
  }
  
  Serial.print("EEPROM signature: ");
  for (uint8_t i = 0; i < 4; i++) {
    Serial.print(sig[i]);
  }
  Serial.println();
  
  if (sig[0] != 'C' || sig[1] != 'A' || sig[2] != 'M' || sig[3] != '1') {
    Serial.println("EEPROM signature invalid - will initialize");
    return false;
  }
  
  // Check version
  uint32_t version = 0;
  for (uint8_t i = 0; i < 4; i++) {
    version |= ((uint32_t)EEPROM.read(EEPROM_VERSION_ADDR + i)) << (i * 8);
  }
  
  Serial.print("EEPROM version: ");
  Serial.println(version);
  
  bool valid = (version == EEPROM_VERSION);
  if (!valid) {
    Serial.println("EEPROM version mismatch - will initialize");
  } else {
    Serial.println("EEPROM validation successful");
  }
  
  return valid;
}

void initializeEEPROM() {
  Serial.println("Initializing EEPROM - this will reset photo count to 0!");
  
  // Write magic signature
  EEPROM.write(MAGIC_SIGNATURE_ADDR, 'C');
  EEPROM.write(MAGIC_SIGNATURE_ADDR + 1, 'A');
  EEPROM.write(MAGIC_SIGNATURE_ADDR + 2, 'M');
  EEPROM.write(MAGIC_SIGNATURE_ADDR + 3, '1');
  
  // Write version
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.write(EEPROM_VERSION_ADDR + i, (EEPROM_VERSION >> (i * 8)) & 0xFF);
  }
  
  // Initialize photo count
  writePhotoCount(0);
  
  // Initialize total photos count
  writeTotalPhotos(0);
  
  // Clear camera config
  writeCameraConfig(0);
  
  Serial.println("EEPROM initialization complete");
}

void writePhotoCount(uint16_t count) {
  EEPROM.write(PHOTO_COUNT_ADDR, count & 0xFF);
  EEPROM.write(PHOTO_COUNT_ADDR + 1, (count >> 8) & 0xFF);
  Serial.print("Wrote photo count to EEPROM: ");
  Serial.println(count);
}

void readPhotoCount() {
  uint8_t lowByte = EEPROM.read(PHOTO_COUNT_ADDR);
  uint8_t highByte = EEPROM.read(PHOTO_COUNT_ADDR + 1);
  photoCount = (highByte << 8) | lowByte;
  
  Serial.print("Read photo count from EEPROM: ");
  Serial.println(photoCount);
  
  if (photoCount > PHOTO_COUNT_MAX) {
    Serial.println("Photo count out of range, resetting to 0");
    photoCount = 0;
  }
}

void writeTotalPhotos(uint32_t total) {
  for (uint8_t i = 0; i < 4; i++) {
    EEPROM.write(TOTAL_PHOTOS_ADDR + i, (total >> (i * 8)) & 0xFF);
  }
}

uint32_t readTotalPhotos() {
  uint32_t total = 0;
  for (uint8_t i = 0; i < 4; i++) {
    total |= ((uint32_t)EEPROM.read(TOTAL_PHOTOS_ADDR + i)) << (i * 8);
  }
  return total;
}

void writeCameraConfig(uint16_t config) {
  EEPROM.write(CAMERA_CONFIG_ADDR, config & 0xFF);
  EEPROM.write(CAMERA_CONFIG_ADDR + 1, (config >> 8) & 0xFF);
}

uint16_t readCameraConfig() {
  uint8_t lowByte = EEPROM.read(CAMERA_CONFIG_ADDR);
  uint8_t highByte = EEPROM.read(CAMERA_CONFIG_ADDR + 1);
  return (highByte << 8) | lowByte;
}

void setCameraConfigFlag(uint16_t flag) {
  uint16_t config = readCameraConfig();
  config |= flag;
  writeCameraConfig(config);
}

void clearCameraConfigFlag(uint16_t flag) {
  uint16_t config = readCameraConfig();
  config &= ~flag;
  writeCameraConfig(config);
}

bool isCameraConfigFlagSet(uint16_t flag) {
  return (readCameraConfig() & flag) != 0;
}

void displayStoredImages() {
  Serial.println("BEGIN");
  
  for (int i = 0; i < photoCount; i++) {
    dumpImageToSerial(i);
  }
  Serial.println("END");   
}

// Calibration metadata function removed

void dumpImageToSerial(uint8_t imageNumber) {
  int address = (imageNumber * BYTES_PER_IMAGE) + IMAGE_DATA_START_ADDR;
  
  Serial.print("IMAGE,");
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    // Read 16-bit value from EEPROM
    uint8_t lowByte = EEPROM.read(address + (i * 2));
    uint8_t highByte = EEPROM.read(address + (i * 2) + 1);
    uint16_t pixelValue = (highByte << 8) | lowByte;
    
    Serial.print(pixelValue);
    if (i < TOTAL_PIXELS - 1) {
      Serial.print(",");
    }
  }
  Serial.println(",IMAGE");
}

void sendStreamFrame() {
  // Send binary frame header (2 bytes: 0xAA, 0x55)
  Serial.write(0xAA);
  Serial.write(0x55);
  
  // Send 35 pixels as 16-bit values (70 bytes total)
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    // Convert 2D sensor data to 1D array index
    uint8_t row = i / IMAGE_WIDTH;
    uint8_t col = i % IMAGE_WIDTH;
    uint16_t pixelValue = sensorData[row][col];
    
    // Convert to 16-bit range for streaming
    uint16_t streamValue = map(pixelValue, 0, 1023, 0, 65535);
    
    // Send as two bytes (high byte first)
    Serial.write((streamValue >> 8) & 0xFF);
    Serial.write(streamValue & 0xFF);
  }
  
  // Send frame footer (2 bytes: 0x55, 0xAA)
  Serial.write(0x55);
  Serial.write(0xAA);
}

void handleSerialCommands() {
  if (Serial.available()) {
    char command = Serial.read();
    
    switch (command) {
      case 'S':
      case 's':
        // Start streaming
        streamingMode = true;
        galleryMode = false;
        Serial.println("STREAM_START");
        break;
        
      case 'Q':
      case 'q':
        // Stop streaming
        streamingMode = false;
        Serial.println("STREAM_STOP");
        break;
        
      case 'D':
      case 'd':
        // Download stored images
        if (photoCount > 0) {
          displayStoredImages();
        } else {
          Serial.println("NO_IMAGES");
        }
        break;
        
      case 'R':
      case 'r':
        // Reset to camera mode
        streamingMode = false;
        galleryMode = false;
        Serial.println("RESET");
        break;
    }
  }
}

void loop() {
  // Handle serial commands first
  handleSerialCommands();
  
  if (streamingMode) {
    // Streaming mode - send live sensor data
    readSensorData();
    
    // Send stream frame at regular intervals
    if (millis() - lastStreamUpdate > STREAM_INTERVAL) {
      sendStreamFrame();
      lastStreamUpdate = millis();
    }
    
    // Reset for next cycle
    resetSensorData();
    pixelReadCount = 0;
    
  } else if (galleryMode) {
    // Gallery mode - handle knob navigation
    handleGalleryNavigation();
  } else {
    // Camera mode - normal operation
    // Magic enhance knob - combines exposure time and smart compensation
    int knobValue = analogRead(A7);
    
    // Map knob to enhance modes and multipliers with longer exposure times
    if (knobValue < 341) {
      // Mode 1: Normal (0-1/3 of knob)
      enhanceMode = 0; // Normal
      exposureTime = mapFloat(knobValue, 0, 340, 0.05, 1.0); // 50ms to 1 second
      enhanceMultiplier = 1.0;
    } else if (knobValue < 682) {
      // Mode 2: Enhanced (1/3 to 2/3 of knob)
      enhanceMode = 1; // Enhanced
      exposureTime = mapFloat(knobValue, 341, 681, 1.0, 4.0); // 1s to 4s
      enhanceMultiplier = mapFloat(knobValue, 341, 681, 1.5, 3.0); // 1.5x to 3x
    } else {
      // Mode 3: Ultra (2/3 to full knob)
      enhanceMode = 2; // Ultra
      exposureTime = mapFloat(knobValue, 682, 1023, 4.0, 10.0); // 4s to 10s
      enhanceMultiplier = mapFloat(knobValue, 682, 1023, 3.0, 8.0); // 3x to 8x
    }
    
    // Read sensor data
    readSensorData();
    
    // Check for photo capture
    checkButtonPress();
    
    // Update display (throttled for better performance)
    if (millis() - lastUIUpdate > 20) { // Faster UI updates
      updateDisplay();
      lastUIUpdate = millis();
    }
    
    // Reset for next cycle
    resetSensorData();
    pixelReadCount = 0;
  }
}

void readSensorData() {
  // Simple preview: take one sample per cycle for fast UI
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    activateRow(row);
    delay(READ_DELAY);
    
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      int sensorValue = analogRead(COL_START_PIN + col);
      sensorData[row][col] += sensorValue;
      
      // Track min/max for histogram
      if (sensorValue < minValue) minValue = sensorValue;
      if (sensorValue > maxValue) maxValue = sensorValue;
    }
  }
  
  pixelReadCount++;
}

void activateRow(int row) {
  // Deactivate all rows first
  digitalWrite(ROW1_PIN, LOW);
  digitalWrite(ROW2_PIN, LOW);
  digitalWrite(ROW3_PIN, LOW);
  digitalWrite(ROW4_PIN, LOW);
  digitalWrite(ROW5_PIN, LOW);
  
  // Activate the specified row
  switch (row) {
    case 0: digitalWrite(ROW1_PIN, HIGH); break;
    case 1: digitalWrite(ROW2_PIN, HIGH); break;
    case 2: digitalWrite(ROW3_PIN, HIGH); break;
    case 3: digitalWrite(ROW4_PIN, HIGH); break;
    case 4: digitalWrite(ROW5_PIN, HIGH); break;
  }
}

void checkButtonPress() {
  int buttonState = digitalRead(BUTTON_PIN);
  
  // Debounce the button
  if (buttonState != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (buttonState == LOW && !buttonPressed) {
      buttonPressed = true;
      lastButtonPress = millis();
    } else if (buttonState == HIGH && buttonPressed) {
      buttonPressed = false;
      unsigned long pressDuration = millis() - lastButtonPress;
      
      if (pressDuration >= LONG_PRESS_TIME) {
        // Long press - toggle gallery mode
        toggleGalleryMode();
      } else if (pressDuration >= debounceDelay) {
        // Short press - capture photo (only in camera mode)
        if (!galleryMode) {
          capturePhoto();
        }
      }
    }
  }
  
  lastButtonState = buttonState;
}

void capturePhoto() {
  if (photoCount >= PHOTO_COUNT_MAX) {
    displayMessage("Memory Full!", UI_ERROR_COLOR);
    delay(1500);
    return;
  }
  
  displayMessage("CAPTURING...", UI_WARNING_COLOR);

  
  // Take high-quality exposure with multiple samples
  captureHighQualityExposure();
  
  // Process and save image data
  processImageData();
  saveImageToEEPROM();
  
  photoCount++;
  writePhotoCount(photoCount);
  
  // Update total photos count
  uint32_t totalPhotos = readTotalPhotos();
  writeTotalPhotos(totalPhotos + 1);
  
  // Display the captured photo with interactive button
  displayCapturedPhoto();
  
  // Wait for button press or timeout
  unsigned long photoStartTime = millis();
  const unsigned long PHOTO_DISPLAY_TIMEOUT = 5000; // 5 second timeout
  
  while (millis() - photoStartTime < PHOTO_DISPLAY_TIMEOUT) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      // Button pressed - exit photo display
      break;
    }
    delay(50);
  }
  
  // Return to normal preview mode - complete UI reset
  display.fillScreen(UI_BG_COLOR);
  resetSensorData();
  pixelReadCount = 0;
  // Force complete UI reinitialization
  forceUIReset();
}



void captureHighQualityExposure() {
  // Calculate exposure time in milliseconds
  unsigned long exposureMs = (unsigned long)(exposureTime * 1000);
  unsigned long startTime = millis();
  unsigned long endTime = startTime + exposureMs;
  
  pixelReadCount = 0;
  
  // Clear sensor data for fresh capture
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      sensorData[row][col] = 0;
    }
  }
  
  // Take samples continuously for the specified exposure time
  while (millis() < endTime) {
    // Read each row of sensors
    for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
      activateRow(row);
      delay(READ_DELAY);
      
      for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
        int sensorValue = analogRead(COL_START_PIN + col);
        sensorData[row][col] += sensorValue;
      }
    }
    pixelReadCount++;
  }
}

void processImageData() {
  int pixelIndex = 0;
  
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      // Calculate average sensor reading (includes noise reduction from multiple samples)
      uint16_t avgValue = sensorData[row][col] / max(pixelReadCount, 1);
      
      // Apply magic enhance multiplier
      uint16_t enhancedValue = avgValue * enhanceMultiplier;
      if (enhancedValue > 1023) enhancedValue = 1023;
      
      // Store raw enhanced value as 16-bit (no calibration applied)
      uint16_t processedValue = map(enhancedValue, 0, 1023, 0, 65535);
      
      imageData[pixelIndex] = processedValue;
      pixelIndex++;
    }
  }
}

// Calibration preview functions removed

void displayCapturedPhoto() {
  // Clear screen
  display.fillScreen(UI_BG_COLOR);
  
  // Display the captured photo with proper 16-bit to 8-bit conversion
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      int pixelIndex = row * IMAGE_WIDTH + col;
      
      // Safety check
      if (pixelIndex >= TOTAL_PIXELS) continue;
      
      // Convert 16-bit to 8-bit properly
      uint16_t pixelValue = imageData[pixelIndex];
      uint8_t gray = (pixelValue >> 8) & 0xFF; // Take high byte for 8-bit conversion
      
      // Create color
      uint16_t color = display.color565(gray, gray, gray);
      
      // Draw larger pixels for visibility
      int pixelSize = 20;
      display.fillRect(col * pixelSize, row * pixelSize, pixelSize, pixelSize, color);
    }
  }
  
  // Show photo info
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, IMAGE_HEIGHT * 20 + 5);
  printProgMem(str_photo);
  display.print(photoCount);
  display.print(" - ");
  const char* modeNames[] = {"NORM", "ENH", "ULTRA"};
  display.print(modeNames[enhanceMode]);
  display.print(" ");
  display.print(exposureTime, 2);
  display.print("s ");
  display.print(enhanceMultiplier, 1);
  display.print("x");
  
  // Show button instruction
  display.setCursor(0, IMAGE_HEIGHT * 20 + 20);
  display.setTextColor(UI_ACCENT_COLOR);
  printProgMem(str_press_button);
}


void saveImageToEEPROM() {
  int address = (photoCount * BYTES_PER_IMAGE) + IMAGE_DATA_START_ADDR;
  
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    // Write 16-bit value as two bytes (little-endian)
    EEPROM.write(address + (i * 2), imageData[i] & 0xFF);         // Low byte
    EEPROM.write(address + (i * 2) + 1, (imageData[i] >> 8) & 0xFF); // High byte
  }
}

void updateDisplay() {
  // Track previous values to only update when changed
  static float lastExposureTime = 0;
  static float lastEnhanceMultiplier = 0;
  static int8_t lastEnhanceMode = -1; // Reduced from int to int8_t
  static uint16_t lastPhotoCount = 0;
  static bool uiInitialized = false;
  
  // Force reinitialization if lastUIUpdate is 0 (after photo capture)
  if (lastUIUpdate == 0) {
    uiInitialized = false;
  }
  
  // Initialize UI once or when screen is cleared
  if (!uiInitialized) {
    display.fillScreen(UI_BG_COLOR);
    drawMainUI();
    drawStatusInfo();
    uiInitialized = true;
    // Reset all tracking variables
    lastExposureTime = 0;
    lastEnhanceMultiplier = 0;
    lastEnhanceMode = -1;
    lastPhotoCount = 0;
  }
  
  // Update enhance bar only if values changed
  if (abs(exposureTime - lastExposureTime) > 0.001 || abs(enhanceMultiplier - lastEnhanceMultiplier) > 0.01 || enhanceMode != lastEnhanceMode) {
    // Clear only the enhance bar area
    display.fillRect(0, IMAGE_HEIGHT * PIXEL_SIZE + 5, 32, 6, UI_BG_COLOR);
    // Redraw enhance bar with mode-based color
    uint16_t barColor = UI_ACCENT_COLOR;
    if (enhanceMode == 1) barColor = UI_WARNING_COLOR;
    if (enhanceMode == 2) barColor = UI_SUCCESS_COLOR;
    
    int enhanceBarWidth = map(exposureTime, 0.05, 10.0, 0, 30); // Map based on exposure time
    display.fillRect(0, IMAGE_HEIGHT * PIXEL_SIZE + 5, enhanceBarWidth, 4, barColor);
    display.drawRect(0, IMAGE_HEIGHT * PIXEL_SIZE + 5, 30, 4, UI_TEXT_COLOR);
    
    lastExposureTime = exposureTime;
    lastEnhanceMultiplier = enhanceMultiplier;
    lastEnhanceMode = enhanceMode;
  }
  
  // Update only specific text values that changed
  int y = IMAGE_HEIGHT * PIXEL_SIZE + 15;
  
  // Update photo count only if changed
  if (photoCount != lastPhotoCount) {
    // Clear only the photo count area
    display.fillRect(50, y, 40, 8, UI_BG_COLOR);
    display.setTextColor(UI_TEXT_COLOR);
    display.setTextSize(1);
    display.setCursor(50, y);
    display.print(photoCount);
    display.print("/");
    display.print(PHOTO_COUNT_MAX);
    lastPhotoCount = photoCount;
  }
  
  // Update progress bar only if photo count changed
  if (photoCount != lastPhotoCount) {
    // Clear progress bar area
    display.fillRect(0, y + 10, 32, 5, UI_BG_COLOR);
    // Redraw progress bar
    int progressWidth = map(photoCount, 0, PHOTO_COUNT_MAX, 0, 30);
    display.fillRect(0, y + 10, progressWidth, 3, UI_SUCCESS_COLOR);
    display.drawRect(0, y + 10, 30, 3, UI_TEXT_COLOR);
  }
  
  // Update enhance values - make it more responsive
  if (millis() - lastUIUpdate >= UI_UPDATE_INTERVAL) {
    // Clear the enhance value areas
    display.fillRect(0, y + 20, 128, 16, UI_BG_COLOR);
    display.setTextColor(UI_TEXT_COLOR);
    display.setTextSize(1);
    
    // Line 1: Mode
    display.setCursor(0, y + 20);
    display.print("Enhance: ");
    const char* modeNames[] = {"NORMAL", "ENHANCED", "ULTRA"};
    display.print(modeNames[enhanceMode]);
    
    // Line 2: Time and Multiplier values only
    display.setCursor(0, y + 30);
    display.print(exposureTime, 1);
    display.print("s  ");
    display.print(enhanceMultiplier, 1);
    display.print("x");
  }
  
  // Range line removed as requested
  
  // Always update sensor pixels for smooth real-time display
  drawSensorPixels();
}

void drawMainUI() {
  // Draw border around sensor area (5x7 = 40x56 pixels)
  display.drawRect(0, 0, IMAGE_WIDTH * PIXEL_SIZE + 2, IMAGE_HEIGHT * PIXEL_SIZE + 2, UI_ACCENT_COLOR);
}

void drawSensorPixels() {
  // Removed lastPixelValues array to save RAM - update all pixels every time
  
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      uint16_t avgValue = sensorData[row][col] / max(pixelReadCount, 1);
      
      // Apply magic enhance multiplier for preview with additional boost
      uint16_t multipliedValue = avgValue * enhanceMultiplier * 1.5; // Extra boost for preview visibility
      if (multipliedValue > 1023) multipliedValue = 1023;
      
      // Convert to 8-bit for display (no calibration applied)
      uint8_t gray = map(multipliedValue, 0, 1023, 0, 255);
      
      uint16_t color = display.color565(gray, gray, gray);
      
      display.fillRect((IMAGE_WIDTH - 1 - col) * PIXEL_SIZE + 1, row * PIXEL_SIZE + 1, 
                      PIXEL_SIZE, PIXEL_SIZE, color);
    }
  }
}

void drawStatusInfo() {
  int y = IMAGE_HEIGHT * PIXEL_SIZE + 15;
  
  // Photo count with progress bar
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, y);
  printProgMem(str_photos);
  display.print(photoCount);
  display.print("/");
  display.print(PHOTO_COUNT_MAX);
  
  // Progress bar
  int progressWidth = map(photoCount, 0, PHOTO_COUNT_MAX, 0, 30);
  display.fillRect(0, y + 10, progressWidth, 3, UI_SUCCESS_COLOR);
  display.drawRect(0, y + 10, 30, 3, UI_TEXT_COLOR);
  
  // Magic enhance knob - split into two lines
  display.setCursor(0, y + 20);
  printProgMem(str_enhance);
  const char* modeNames[] = {str_normal, str_enhanced, str_ultra};
  printProgMem(modeNames[enhanceMode]);
  
  // Second line for time and multiplier values only
  display.setCursor(0, y + 30);
  display.print(exposureTime, 1);
  display.print("s  ");
  display.print(enhanceMultiplier, 1);
  display.print("x");
  
  // Range line removed as requested
  
  // Calibration status removed
  
  // Instructions (compact)
  display.setTextColor(UI_ACCENT_COLOR);
  display.setCursor(0, y + 55);
  display.print("Press: Capture");
}



void displayMessage(const char* message, uint16_t color) {
  display.fillScreen(UI_BG_COLOR);
  display.setCursor(0, 30);
  display.setTextColor(color);
  display.setTextSize(2);
  display.println(message);
}

void resetSensorData() {
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      sensorData[row][col] = 0;
    }
  }
  minValue = 1023;
  maxValue = 0;
}

void resetUIState() {
  // Force UI reinitialization by clearing the screen and redrawing
  display.fillScreen(UI_BG_COLOR);
  drawMainUI();
  drawStatusInfo();
}

void forceUIReset() {
  // Complete UI reset - clear screen and force reinitialization
  display.fillScreen(UI_BG_COLOR);
  // Reset all static variables by forcing UI reinit
  lastUIUpdate = 0;
}

void toggleGalleryMode() {
  galleryMode = !galleryMode;
  if (galleryMode) {
    // Entering gallery mode
    currentGalleryImage = 0;
    display.fillScreen(UI_BG_COLOR);
    displayGallery();
  } else {
    // Exiting gallery mode - return to camera
    display.fillScreen(UI_BG_COLOR);
    forceUIReset();
  }
}

void loadImageFromEEPROM(uint8_t imageIndex) {
  if (imageIndex >= photoCount || imageIndex < 0) return;
  
  int address = (imageIndex * BYTES_PER_IMAGE) + IMAGE_DATA_START_ADDR;
  
  // Safety check for EEPROM bounds
  if (address + (TOTAL_PIXELS * 2) > 1024) {
    return;
  }
  
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    // Read 16-bit value from two bytes (little-endian)
    uint8_t lowByte = EEPROM.read(address + (i * 2));
    uint8_t highByte = EEPROM.read(address + (i * 2) + 1);
    imageData[i] = (highByte << 8) | lowByte;
  }
}

void handleGalleryNavigation() {
  static int16_t lastKnobValue = 0; // Reduced from int to int16_t
  static unsigned long lastNavigationTime = 0;
  
  // Check for button press (long press to exit)
  checkButtonPress();
  
  // Handle knob navigation
  int knobValue = analogRead(A7);
  
  // Only navigate if knob has changed significantly and enough time has passed
  if (abs(knobValue - lastKnobValue) > 50 && millis() - lastNavigationTime > 200) {
    if (photoCount > 0) {
      if (knobValue < 512) {
        // Turn left - previous image
        currentGalleryImage--;
        if (currentGalleryImage < 0) currentGalleryImage = photoCount - 1;
      } else {
        // Turn right - next image
        currentGalleryImage++;
        if (currentGalleryImage >= photoCount) currentGalleryImage = 0;
      }
      
      // Update display
      display.fillScreen(UI_BG_COLOR);
      displayGallery();
      lastNavigationTime = millis();
    }
    lastKnobValue = knobValue;
  }
}

void displayGallery() {
  if (photoCount == 0) {
    // No photos to display
    display.setTextColor(UI_TEXT_COLOR);
    display.setTextSize(2);
    display.setCursor(10, 30);
    printProgMem(str_no_photos);
    display.setTextSize(1);
    display.setCursor(10, 50);
    printProgMem(str_long_press);
    return;
  }
  
  // Load and display current image
  Serial.print("Loading gallery image: ");
  Serial.println(currentGalleryImage);
  loadImageFromEEPROM(currentGalleryImage);
  
  // Display image with proper 16-bit to 8-bit conversion
  for (int y = 0; y < IMAGE_HEIGHT; y++) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      int pixelIndex = y * IMAGE_WIDTH + x;
      
      // Safety check
      if (pixelIndex >= TOTAL_PIXELS) continue;
      
      // Convert 16-bit to 8-bit properly
      uint16_t pixelValue = imageData[pixelIndex];
      uint8_t gray = (pixelValue >> 8) & 0xFF; // Take high byte for 8-bit conversion
      
      // Create color
      uint16_t color = display.color565(gray, gray, gray);
      display.fillRect(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, color);
    }
  }
  
  // Display gallery info
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, IMAGE_HEIGHT * PIXEL_SIZE + 5);
  printProgMem(str_gallery);
  display.print(currentGalleryImage + 1);
  display.print("/");
  display.print(photoCount);
  
  display.setCursor(0, IMAGE_HEIGHT * PIXEL_SIZE + 15);
  printProgMem(str_knob_navigate);
  
  display.setCursor(0, IMAGE_HEIGHT * PIXEL_SIZE + 25);
  display.print("Long press: Exit");
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
 return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
