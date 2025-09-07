# **Agfa35 Camera**

### **35 Pixel Arduino Camera**

7×5 pixel sensor array with Arduino Nano, ST7735 display, and EEPROM storage. Built with LDR matrix and enhanced UI controls.

---

## **Hardware Requirements**

- **Arduino Nano** (or compatible)
- **ST7735 TFT Display** (128×160)
- **35 Light Sensors** (LDRs)
- **1 Push Button**
- **1 LED** (status indicator)
- **1 Potentiometer** (knob for exposure control)
- **Resistors** (10kΩ for pull-ups, 1kΩ for LED)
- **Breadboard/PCB** for assembly

---

## **Pin Configuration**

### **Arduino Nano Pins**
- **A0**: LDR matrix output (analog read)
- **A1**: Potentiometer (exposure control)
- **D2**: LDR row select (bit 0)
- **D3**: LDR row select (bit 1)
- **D4**: LDR row select (bit 2)
- **D5**: LDR row select (bit 3)
- **D6**: LDR row select (bit 4)
- **D7**: Button (capture/gallery)
- **D8**: TFT DC pin
- **D9**: TFT RST pin
- **D10**: TFT CS pin
- **D11**: TFT MOSI
- **D12**: TFT MISO
- **D13**: TFT SCK
- **D14**: LED (status)

---

## **Sensor Array Layout**

```
7×5 Pixel Array:
[1] [2] [3] [4] [5] [6] [7]  ← Row 0
[8] [9][10][11][12][13][14]  ← Row 1
[15][16][17][18][19][20][21] ← Row 2
[22][23][24][25][26][27][28] ← Row 3
[29][30][31][32][33][34][35] ← Row 4
```

**Row Selection**: Digital pins D2-D6 (5-bit binary)
**Column Read**: Analog pin A0

---

## **Build Instructions**

### **1. Assemble Sensor Array**
- Connect 35 LDRs in 7×5 grid
- Wire row select pins to Arduino D2-D6
- Connect matrix output to Arduino A0
- Add pull-up resistors to sensor inputs

### **2. Connect Display**
- ST7735 to Arduino SPI pins (D11, D12, D13)
- CS to D10, RST to D9, DC to D8

### **3. Connect Controls**
- Button to D7 (with pull-up resistor)
- Potentiometer to A1 (with 5V and GND)
- LED to D14 (with current limiting resistor)

---

## **Software Setup**

### **1. Install Arduino IDE**
- Download Arduino IDE 1.8.x or 2.x
- Install required libraries:
  - `Adafruit ST7735` (display)
  - `Adafruit GFX` (graphics)

### **2. Upload Firmware**
```bash
# Navigate to firmware directory
cd CameraFirmware/

# Upload to Arduino (replace port as needed)
arduino-cli compile --fqbn arduino:avr:nano CameraFirmware.ino
arduino-cli upload -p /dev/cu.usbserial-10 --fqbn arduino:avr:nano CameraFirmware.ino
```

### **3. Test Camera**
- Power on Arduino
- Display should show "Agfa35 V2 - Enhanced UI"
- Check serial monitor for "Agfa35 Camera V2 Ready"
- Test button and knob response

---

## **Usage**

### **Controls**
- **Button (Short Press)**: Capture photo
- **Button (Long Press)**: Toggle gallery mode
- **Knob**: Adjust exposure mode (Normal/Enhanced/Ultra)

### **Display Modes**
- **Live View**: Real-time sensor readings
- **Gallery**: Browse captured photos
- **Capture**: Shows captured image

### **Photo Storage**
- **Capacity**: 28 photos maximum
- **Format**: 8-bit grayscale (0-255)
- **Size**: 7×5 pixels (35 bytes per image)
- **Storage**: Arduino EEPROM

---

## **Download Images**

### **1. Install Python Dependencies**
```bash
cd QuickDownload/
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```

### **2. Download Images**
```bash
# Run download script
./run.sh

# Or run directly
python GetImages.py
```

### **3. Output Files**
- **source_images/**: Raw 8-bit pixel data (.raw files)
- **exported_images/**: PNG images (7×5 pixels)
- **scaled_images/**: 100x scaled PNG images (700×500)
- **metadata/**: JSON files with capture info

---

## **Troubleshooting**

### **Display Issues**
- **No display**: Check SPI connections and power
- **Garbled text**: Check library installation
- **Wrong colors**: Verify display initialization

### **Sensor Issues**
- **All zeros**: Check LDR matrix wiring
- **Inconsistent readings**: Verify row select pins
- **Wrong pixels**: Check matrix connections

### **Button Issues**
- **No response**: Check pull-up resistors
- **Multiple triggers**: Add debouncing (already in firmware)

### **Serial Issues**
- **No communication**: Check USB cable and port
- **Wrong port**: Use `arduino-cli board list` to find correct port

---

## **Technical Specifications**

- **Resolution**: 7×5 pixels (35 total)
- **Color Depth**: 8-bit grayscale
- **Storage**: 28 images maximum
- **Display**: 128×160 ST7735 TFT
- **Microcontroller**: Arduino Nano (ATmega328P)
- **Power**: 5V via USB or external supply
- **Communication**: Serial (USB)

---

## **File Structure**

```
Agfa35/
├── CameraFirmware/
│   └── CameraFirmware.ino      # Main firmware
├── QuickDownload/
│   ├── GetImages.py           # Image download script
│   ├── run.sh                # Run script
│   ├── verify_100x.py        # Image verification
│   └── requirements.txt      # Python dependencies
└── README.md                 # This file
```

---

**Charles Gershom** - [charlesgershom.com](https://charlesgershom.com/)
