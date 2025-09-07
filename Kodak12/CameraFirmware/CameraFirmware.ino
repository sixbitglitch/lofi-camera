/*
 * Kodak12 Lofi Camera Firmware V2
 * Enhanced version for 12-pixel multiplexer camera
 * 
 * Features:
 * - 12-pixel sensor array with multiplexer control
 * - Enhanced EEPROM management with validation
 * - Improved exposure control and UI
 * - Beeper feedback system
 * - Two-button interface with knob control
 * - Gallery mode and streaming support
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <EEPROM.h>
#include <math.h>

// Hardware Configuration
#define LED_PIN 13
#define BUTTON_PIN 9
#define BUTTON2_PIN 8  // Second button for additional functions
#define BEEPER_PIN 10

// Display Configuration
#define TFT_CS     A5
#define TFT_RST    A4
#define TFT_DC     A3

// Camera Configuration
#define PHOTO_COUNT_MAX 20  // Reduced for 12-pixel images (12 bytes each)
#define IMAGE_WIDTH 4
#define IMAGE_HEIGHT 3
#define TOTAL_PIXELS (IMAGE_WIDTH * IMAGE_HEIGHT)

// EEPROM Layout (Arduino Nano has 1024 bytes)
// 0-1:     Photo count (2 bytes)
// 2-3:     Camera config flags (2 bytes)
// 4-7:     Reserved for future use (4 bytes)
// 8-11:    Magic signature "K12" (3 bytes) + version (1 byte)
// 12-15:   EEPROM version (4 bytes)
// 16-19:   Total photos taken (4 bytes, for statistics)
// 20-23:   Reserved (4 bytes)
// 24-1023: Image data (1000 bytes = 20 photos max, 12 bytes each)

// EEPROM Addresses
#define PHOTO_COUNT_ADDR 0
#define CAMERA_CONFIG_ADDR 2
#define MAGIC_SIGNATURE_ADDR 8
#define EEPROM_VERSION_ADDR 12
#define TOTAL_PHOTOS_ADDR 16
#define IMAGE_DATA_START_ADDR 24
#define BYTES_PER_IMAGE TOTAL_PIXELS  // 12 bytes per image (8-bit)

// Camera Config Flags (bit flags)
#define CONFIG_CALIBRATED 0x01
#define CONFIG_AUTO_EXPOSURE 0x02
#define CONFIG_DEBUG_MODE 0x04

// EEPROM Version
#define EEPROM_VERSION 1

// Multiplexer Configuration
// Left Side
const int selectPinsLeft[3] = {5, 6, 7}; // S0~2, S1~3, S2~4
const int aInputLeft = A7; // Connect common (Z) to A7

// Right Side
const int selectPinsRight[3] = {2, 3, 4}; // S0~2, S1~3, S2~4
const int aInputRight = A1; // Connect common (Z) to A1

// Display Settings
#define PIXEL_SIZE 18
#define READ_DELAY 20
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

// Beeper tones
#define  c     3830    // 261 Hz 
#define  d     3400    // 294 Hz 
#define  e     3038    // 329 Hz 
#define  f     2864    // 349 Hz 
#define  g     2550    // 392 Hz 
#define  a     2272    // 440 Hz 
#define  b     2028    // 493 Hz 
#define  C     1912    // 523 Hz



// Global Variables
Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

uint16_t photoCount = 0;
uint16_t sensorData[IMAGE_HEIGHT][IMAGE_WIDTH];
uint8_t imageData[TOTAL_PIXELS];
uint8_t pixelReadCount = 0;
float exposureTime = 0.1;
float enhanceMultiplier = 1.0;
uint8_t enhanceMode = 0; // 0=normal, 1=enhanced, 2=ultra
uint16_t minValue = 1023;
uint16_t maxValue = 0;

// Button state tracking
bool lastButtonState = HIGH;
bool lastButton2State = HIGH;
bool buttonPressed = false;
bool button2Pressed = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// UI Update timing
unsigned long lastUIUpdate = 0;

// Gallery mode
bool galleryMode = false;
uint8_t currentGalleryImage = 0;
unsigned long lastButtonPress = 0;
const unsigned long LONG_PRESS_TIME = 1000;

// Streaming mode
bool streamingMode = false;
uint32_t lastStreamUpdate = 0;
const uint32_t STREAM_INTERVAL = 50;

void playTone(int pin, int tone_, int duration) {
  long elapsed_time = 0;
  while (elapsed_time < duration) {
    digitalWrite(pin, HIGH);
    delayMicroseconds(tone_ / 2);
    digitalWrite(pin, LOW);
    delayMicroseconds(tone_ / 2);
    elapsed_time += (tone_);
  } 
}

void photoBeep() {
  playTone(BEEPER_PIN, c, 70);
  delay(50);
  playTone(BEEPER_PIN, d, 60);
  delay(50);
  playTone(BEEPER_PIN, d, 70);
}

void errorBeep() {
  playTone(BEEPER_PIN, f, 100);
  delay(100);
  playTone(BEEPER_PIN, f, 100);
}

void successBeep() {
  playTone(BEEPER_PIN, g, 50);
  delay(30);
  playTone(BEEPER_PIN, a, 50);
  delay(30);
  playTone(BEEPER_PIN, C, 100);
}

void readSensorData() {
  // Read all 12 pixels using multiplexer
  // Row 1 (top row)
  selectMuxPinRight(5);
  delay(READ_DELAY);
  int val1 = analogRead(aInputRight);
  sensorData[0][0] += val1;
  
  selectMuxPinRight(4);
  delay(READ_DELAY);
  int val2 = analogRead(aInputRight);
  sensorData[0][1] += val2;
  
  selectMuxPinLeft(5);
  delay(READ_DELAY);
  int val3 = analogRead(aInputLeft);
  sensorData[0][2] += val3;
  
  selectMuxPinLeft(4);
  delay(READ_DELAY);
  int val4 = analogRead(aInputLeft);
  sensorData[0][3] += val4;

  // Row 2 (middle row)
  selectMuxPinRight(3);
  delay(READ_DELAY);
  int val5 = analogRead(aInputRight);
  sensorData[1][0] += val5;
  
  selectMuxPinRight(2);
  delay(READ_DELAY);
  int val6 = analogRead(aInputRight);
  sensorData[1][1] += val6;
  
  selectMuxPinLeft(3);
  delay(READ_DELAY);
  int val7 = analogRead(aInputLeft);
  sensorData[1][2] += val7;
  
  selectMuxPinLeft(2);
  delay(READ_DELAY);
  int val8 = analogRead(aInputLeft);
  sensorData[1][3] += val8;

  // Row 3 (bottom row)
  selectMuxPinRight(1);
  delay(READ_DELAY);
  int val9 = analogRead(aInputRight);
  sensorData[2][0] += val9;
  
  selectMuxPinRight(0);
  delay(READ_DELAY);
  int val10 = analogRead(aInputRight);
  sensorData[2][1] += val10;
  
  selectMuxPinLeft(1);
  delay(READ_DELAY);
  int val11 = analogRead(aInputLeft);
  sensorData[2][2] += val11;
  
  selectMuxPinLeft(0);
  delay(READ_DELAY);
  int val12 = analogRead(aInputLeft);
  sensorData[2][3] += val12;
  
  pixelReadCount++;
  
  // Debug output every 10 readings
  if (pixelReadCount % 10 == 0) {
    Serial.print("Raw sensor readings (sample ");
    Serial.print(pixelReadCount);
    Serial.println("):");
    Serial.print("Right A1: ");
    Serial.print(val1);
    Serial.print(" ");
    Serial.print(val2);
    Serial.print(" ");
    Serial.print(val5);
    Serial.print(" ");
    Serial.print(val6);
    Serial.print(" ");
    Serial.print(val9);
    Serial.print(" ");
    Serial.print(val10);
    Serial.println();
    Serial.print("Left A7:  ");
    Serial.print(val3);
    Serial.print(" ");
    Serial.print(val4);
    Serial.print(" ");
    Serial.print(val7);
    Serial.print(" ");
    Serial.print(val8);
    Serial.print(" ");
    Serial.print(val11);
    Serial.print(" ");
    Serial.print(val12);
    Serial.println();
    Serial.println("---");
  }
}

void selectMuxPinLeft(byte pin)
{
  if (pin > 7) return; // Exit if pin is out of scope
  for (int i=0; i<3; i++)
  {
    if (pin & (1<<i))
      digitalWrite(selectPinsLeft[i], HIGH);
    else
      digitalWrite(selectPinsLeft[i], LOW);
  }
}


void selectMuxPinRight(byte pin)
{
  if (pin > 7) return; // Exit if pin is out of scope
  for (int i=0; i<3; i++)
  {
    if (pin & (1<<i))
      digitalWrite(selectPinsRight[i], HIGH);
    else
      digitalWrite(selectPinsRight[i], LOW);
  }
}

void setupSensorMultiplex() {
  for (int i=0; i<3; i++) {
    pinMode(selectPinsLeft[i], OUTPUT);
    digitalWrite(selectPinsLeft[i], HIGH);
  }

  for (int i=0; i<3; i++) {
    pinMode(selectPinsRight[i], OUTPUT);
    digitalWrite(selectPinsRight[i], HIGH);
  }
}

// Enhanced EEPROM Management Functions
bool validateEEPROM() {
  // Check magic signature
  char sig[3];
  for (uint8_t i = 0; i < 3; i++) {
    sig[i] = EEPROM.read(MAGIC_SIGNATURE_ADDR + i);
  }
  
  Serial.print("EEPROM signature: ");
  for (uint8_t i = 0; i < 3; i++) {
    Serial.print(sig[i]);
  }
  Serial.println();
  
  if (sig[0] != 'K' || sig[1] != '1' || sig[2] != '2') {
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
  EEPROM.write(MAGIC_SIGNATURE_ADDR, 'K');
  EEPROM.write(MAGIC_SIGNATURE_ADDR + 1, '1');
  EEPROM.write(MAGIC_SIGNATURE_ADDR + 2, '2');
  
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting Kodak12 Lofi Camera V2...");
  
  // Initialize hardware
  initializePins();
  initializeDisplay();
  
  // EEPROM check and initialization
  if (!validateEEPROM()) {
    Serial.println("Initializing EEPROM...");
    initializeEEPROM();
  }
  
  // Load photo count
  readPhotoCount();
  Serial.print("Photo count: ");
  Serial.println(photoCount);
  
  // Display startup message
  display.fillScreen(UI_BG_COLOR);
  display.setTextColor(UI_SUCCESS_COLOR);
  display.setTextSize(2);
  display.setCursor(10, 30);
  display.println("Kodak12");
  display.setTextSize(1);
  display.setCursor(10, 60);
  display.println("V2 Ready");
  display.setCursor(10, 80);
  display.print("Photos: ");
  display.print(photoCount);
  
  delay(2000);
  
  Serial.println("Kodak12 Camera V2 Ready");
  
  // Test analog pins
  Serial.println("Testing analog pins:");
  Serial.print("A1 (Right): ");
  Serial.println(analogRead(A1));
  Serial.print("A7 (Left): ");
  Serial.println(analogRead(A7));
  Serial.print("A2 (Knob): ");
  Serial.println(analogRead(A2));
  Serial.println("---");
  
  // Automatically send stored images after startup
  if (photoCount > 0) {
    displayStoredImages();
  }
}

void initializePins() {
  // LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Buttons with pullup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  
  // Beeper
  pinMode(BEEPER_PIN, OUTPUT);
  
  // Setup multiplexer
  setupSensorMultiplex();
}

void initializeDisplay() {
  display.initR(INITR_144GREENTAB);
  display.setRotation(2); // Set rotation to fix upside-down display (180 degrees)
  display.fillScreen(UI_BG_COLOR);
  display.setTextColor(UI_ACCENT_COLOR);
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.println("Kodak12");
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.println("V2 - Enhanced UI");
  display.setCursor(0, 50);
  display.println("12-Pixel Ready");
  delay(1500);
  display.fillScreen(UI_BG_COLOR);
}

void displayStoredImages() {
  Serial.println("BEGIN");
  
  for (int i = 0; i < photoCount; i++) {
    dumpImageToSerial(i);
  }
  Serial.println("END");   
}

void dumpImageToSerial(uint8_t imageNumber) {
  int address = (imageNumber * BYTES_PER_IMAGE) + IMAGE_DATA_START_ADDR;
  
  Serial.print("IMAGE,");
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    uint8_t pixelValue = EEPROM.read(address + i);
    Serial.print(pixelValue);
    if (i < TOTAL_PIXELS - 1) {
      Serial.print(",");
    }
  }
  Serial.println(",IMAGE");
}

void saveImageToEEPROM() {
  int address = (photoCount * BYTES_PER_IMAGE) + IMAGE_DATA_START_ADDR;
  
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    EEPROM.write(address + i, imageData[i]);
  }
}

void processImageData() {
  int pixelIndex = 0;
  
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      // Calculate average sensor reading
      uint16_t avgValue = sensorData[row][col] / max(pixelReadCount, 1);
      
      // Apply enhance multiplier
      uint16_t enhancedValue = avgValue * enhanceMultiplier;
      if (enhancedValue > 1023) enhancedValue = 1023;
      
      // Convert to 8-bit for storage
      imageData[pixelIndex] = map(enhancedValue, 0, 1023, 0, 255);
      pixelIndex++;
    }
  }
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


void updateDisplay() {
  // Track previous values to only update when changed
  static float lastExposureTime = 0;
  static float lastEnhanceMultiplier = 0;
  static int8_t lastEnhanceMode = -1;
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
    
    int enhanceBarWidth = map(exposureTime, 0.05, 10.0, 0, 30);
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
  
  // Update enhance values
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
    
    // Line 2: Time and Multiplier values
    display.setCursor(0, y + 30);
    display.print(exposureTime, 1);
    display.print("s  ");
    display.print(enhanceMultiplier, 1);
    display.print("x");
  }
  
  // Always update sensor pixels for smooth real-time display
  drawSensorPixels();
}

void drawMainUI() {
  // Draw border around sensor area (4x3 = 72x54 pixels)
  display.drawRect(0, 0, IMAGE_WIDTH * PIXEL_SIZE + 2, IMAGE_HEIGHT * PIXEL_SIZE + 2, UI_ACCENT_COLOR);
}

void drawSensorPixels() {
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      uint16_t avgValue = sensorData[row][col] / max(pixelReadCount, 1);
      
      // Apply enhance multiplier for preview
      uint16_t multipliedValue = avgValue * enhanceMultiplier * 1.5;
      if (multipliedValue > 1023) multipliedValue = 1023;
      
      // Convert to 8-bit for display
      uint8_t gray = map(multipliedValue, 0, 1023, 0, 255);
      
      uint16_t color = display.color565(gray, gray, gray);
      
      display.fillRect(col * PIXEL_SIZE + 1, row * PIXEL_SIZE + 1, 
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
  display.print("Photos: ");
  display.print(photoCount);
  display.print("/");
  display.print(PHOTO_COUNT_MAX);
  
  // Progress bar
  int progressWidth = map(photoCount, 0, PHOTO_COUNT_MAX, 0, 30);
  display.fillRect(0, y + 10, progressWidth, 3, UI_SUCCESS_COLOR);
  display.drawRect(0, y + 10, 30, 3, UI_TEXT_COLOR);
  
  // Enhance knob info
  display.setCursor(0, y + 20);
  display.print("Enhance: ");
  const char* modeNames[] = {"NORMAL", "ENHANCED", "ULTRA"};
  display.print(modeNames[enhanceMode]);
  
  // Instructions removed
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
    int knobValue = analogRead(A2);
    
    // Map knob to enhance modes and multipliers
    if (knobValue < 341) {
      // Mode 1: Normal (0-1/3 of knob)
      enhanceMode = 0;
      exposureTime = mapFloat(knobValue, 0, 340, 0.05, 1.0);
      enhanceMultiplier = 1.0;
    } else if (knobValue < 682) {
      // Mode 2: Enhanced (1/3 to 2/3 of knob)
      enhanceMode = 1;
      exposureTime = mapFloat(knobValue, 341, 681, 1.0, 4.0);
      enhanceMultiplier = mapFloat(knobValue, 341, 681, 1.5, 3.0);
    } else {
      // Mode 3: Ultra (2/3 to full knob)
      enhanceMode = 2;
      exposureTime = mapFloat(knobValue, 682, 1023, 4.0, 10.0);
      enhanceMultiplier = mapFloat(knobValue, 682, 1023, 3.0, 8.0);
    }
    
    // Read sensor data
    readSensorData();
    
    // Check for photo capture
    checkButtonPress();
    
    // Update display (throttled for better performance)
    if (millis() - lastUIUpdate > 20) {
      updateDisplay();
      lastUIUpdate = millis();
    }
    
    // Reset for next cycle
    resetSensorData();
    pixelReadCount = 0;
  }
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

void sendStreamFrame() {
  // Send binary frame header (2 bytes: 0xAA, 0x55)
  Serial.write(0xAA);
  Serial.write(0x55);
  
  // Send 12 pixels as 8-bit values (12 bytes total)
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    // Convert 2D sensor data to 1D array index
    uint8_t row = i / IMAGE_WIDTH;
    uint8_t col = i % IMAGE_WIDTH;
    uint16_t pixelValue = sensorData[row][col];
    
    // Convert to 8-bit range for streaming
    uint8_t streamValue = map(pixelValue, 0, 1023, 0, 255);
    
    // Send as single byte
    Serial.write(streamValue);
  }
  
  // Send frame footer (2 bytes: 0x55, 0xAA)
  Serial.write(0x55);
  Serial.write(0xAA);
}

void checkButtonPress() {
  int buttonState = digitalRead(BUTTON_PIN);
  int button2State = digitalRead(BUTTON2_PIN);
  
  // Debounce the buttons
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
  
  // Handle second button (format EEPROM if both buttons pressed)
  if (button2State == LOW && buttonState == LOW) {
    if (!button2Pressed) {
      button2Pressed = true;
      formatEEPROM();
    }
  } else {
    button2Pressed = false;
  }
  
  lastButtonState = buttonState;
  lastButton2State = button2State;
}

void capturePhoto() {
  if (photoCount >= PHOTO_COUNT_MAX) {
    displayMessage("Memory Full!", UI_ERROR_COLOR);
    errorBeep();
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
  
  // Display the captured photo
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
  
  // Return to normal preview mode
  display.fillScreen(UI_BG_COLOR);
  resetSensorData();
  pixelReadCount = 0;
  lastUIUpdate = 0; // Force UI reset
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
    readSensorData();
  }
}

void displayCapturedPhoto() {
  // Clear screen
  display.fillScreen(UI_BG_COLOR);
  
  // Display the captured photo
  for (uint8_t row = 0; row < IMAGE_HEIGHT; row++) {
    for (uint8_t col = 0; col < IMAGE_WIDTH; col++) {
      int pixelIndex = row * IMAGE_WIDTH + col;
      
      // Safety check
      if (pixelIndex >= TOTAL_PIXELS) continue;
      
      // Get pixel value
      uint8_t pixelValue = imageData[pixelIndex];
      
      // Create color
      uint16_t color = display.color565(pixelValue, pixelValue, pixelValue);
      
      // Draw larger pixels for visibility
      int pixelSize = 20;
      display.fillRect(col * pixelSize, row * pixelSize, pixelSize, pixelSize, color);
    }
  }
  
  // Show photo info
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, IMAGE_HEIGHT * 20 + 5);
  display.print("Photo ");
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
  display.print("Press button to continue");
  
  photoBeep();
}

void displayMessage(const char* message, uint16_t color) {
  display.fillScreen(UI_BG_COLOR);
  display.setCursor(0, 30);
  display.setTextColor(color);
  display.setTextSize(2);
  display.println(message);
}

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
  
  delay(3000);
  display.fillScreen(UI_BG_COLOR);
  
  errorBeep();
  errorBeep();
  errorBeep();
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
    lastUIUpdate = 0; // Force UI reset
  }
}

void handleGalleryNavigation() {
  static int16_t lastKnobValue = 0;
  static unsigned long lastNavigationTime = 0;
  
  // Check for button press (long press to exit)
  checkButtonPress();
  
  // Handle knob navigation
  int knobValue = analogRead(A2);
  
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
    display.println("No Photos");
    display.setTextSize(1);
    display.setCursor(10, 50);
    display.println("Long press to exit");
    return;
  }
  
  // Load and display current image
  loadImageFromEEPROM(currentGalleryImage);
  
  // Display image
  for (int y = 0; y < IMAGE_HEIGHT; y++) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      int pixelIndex = y * IMAGE_WIDTH + x;
      
      // Safety check
      if (pixelIndex >= TOTAL_PIXELS) continue;
      
      // Get pixel value
      uint8_t pixelValue = imageData[pixelIndex];
      
      // Create color
      uint16_t color = display.color565(pixelValue, pixelValue, pixelValue);
      display.fillRect(x * PIXEL_SIZE, y * PIXEL_SIZE, PIXEL_SIZE, PIXEL_SIZE, color);
    }
  }
  
  // Display gallery info
  display.setTextColor(UI_TEXT_COLOR);
  display.setTextSize(1);
  display.setCursor(0, IMAGE_HEIGHT * PIXEL_SIZE + 5);
  display.print("Gallery: ");
  display.print(currentGalleryImage + 1);
  display.print("/");
  display.print(photoCount);
  
  display.setCursor(0, IMAGE_HEIGHT * PIXEL_SIZE + 15);
  display.print("Knob: Navigate");
  
  display.setCursor(0, IMAGE_HEIGHT * PIXEL_SIZE + 25);
  display.print("Long press: Exit");
}

void loadImageFromEEPROM(uint8_t imageIndex) {
  if (imageIndex >= photoCount || imageIndex < 0) return;
  
  int address = (imageIndex * BYTES_PER_IMAGE) + IMAGE_DATA_START_ADDR;
  
  // Safety check for EEPROM bounds
  if (address + TOTAL_PIXELS > 1024) {
    return;
  }
  
  for (uint8_t i = 0; i < TOTAL_PIXELS; i++) {
    imageData[i] = EEPROM.read(address + i);
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

