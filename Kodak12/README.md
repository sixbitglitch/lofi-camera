# **Kodak12 Camera**

### **12 Pixel Arduino Camera**

4×3 pixel sensor array with multiplexer control, two buttons, knob, and beeper. Built with Arduino Nano and ST7735 display.

---

## **Hardware Requirements**

- **Arduino Nano** (or compatible)
- **ST7735 TFT Display** (128×128)
- **12 Light Sensors** (LDRs or photodiodes)
- **2× CD4051 Multiplexers** (8-channel analog multiplexers)
- **2 Push Buttons**
- **1 Potentiometer** (10kΩ)
- **1 Beeper/Buzzer**
- **1 LED** (status indicator)
- **Resistors** (10kΩ for pull-ups, 1kΩ for LED)
- **Breadboard/PCB** for assembly

---

## **Pin Configuration**

### **Arduino Nano Pins**
- **A0**: Multiplexer 1 output (right side sensors)
- **A1**: Multiplexer 2 output (left side sensors)  
- **A2**: Potentiometer (exposure control)
- **A3**: TFT DC pin
- **A4**: TFT RST pin
- **A5**: TFT CS pin
- **D2**: Button 1 (capture/gallery)
- **D3**: Multiplexer 1 control pins (S0, S1, S2)
- **D4**: Multiplexer 1 control pins (S0, S1, S2)
- **D5**: Multiplexer 1 control pins (S0, S1, S2)
- **D6**: Multiplexer 2 control pins (S0, S1, S2)
- **D7**: Multiplexer 2 control pins (S0, S1, S2)
- **D8**: Button 2 (secondary function)
- **D9**: Multiplexer 2 control pins (S0, S1, S2)
- **D10**: Beeper
- **D11**: TFT MOSI
- **D12**: TFT MISO
- **D13**: TFT SCK
- **D14**: LED (status)

---

## **Sensor Array Layout**

```
4×3 Pixel Array:
[1] [2] [3] [4]  ← Multiplexer 1 (A0)
[5] [6] [7] [8]  ← Multiplexer 1 (A0)  
[9][10][11][12]  ← Multiplexer 2 (A1)
```

**Multiplexer 1 (Right Side)**: Pixels 1-8
**Multiplexer 2 (Left Side)**: Pixels 9-12

---

## **Build Instructions**

### **1. Assemble Sensor Array**
- Connect 12 light sensors in 4×3 grid
- Wire sensors to multiplexer inputs (0-7 for each mux)
- Connect multiplexer outputs to Arduino A0 and A1
- Add pull-up resistors to sensor inputs

### **2. Connect Multiplexers**
- Wire control pins (S0, S1, S2) to Arduino D3-D5 (Mux1) and D6, D7, D9 (Mux2)
- Connect VCC to 5V, GND to ground
- Connect enable pins to ground (active low)

### **3. Connect Display**
- ST7735 to Arduino SPI pins (D11, D12, D13)
- CS to A5, RST to A4, DC to A3

### **4. Connect Controls**
- Button 1 to D2 (with pull-up resistor)
- Button 2 to D8 (with pull-up resistor)
- Potentiometer to A2 (with 5V and GND)
- Beeper to D10
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
- Display should show "Kodak12 V2 - Enhanced UI"
- Check serial monitor for "Kodak12 Camera V2 Ready"
- Test buttons and knob response

---

## **Usage**

### **Controls**
- **Button 1 (Short Press)**: Capture photo
- **Button 1 (Long Press)**: Toggle gallery mode
- **Button 2**: Secondary functions
- **Both Buttons**: Format EEPROM
- **Knob**: Adjust exposure mode (Normal/Enhanced/Ultra)

### **Display Modes**
- **Live View**: Real-time sensor readings
- **Gallery**: Browse captured photos
- **Capture**: Shows captured image

### **Photo Storage**
- **Capacity**: 20 photos maximum
- **Format**: 8-bit grayscale (0-255)
- **Size**: 4×3 pixels (12 bytes per image)
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
- **exported_images/**: PNG images (4×3 pixels)
- **scaled_images/**: 100x scaled PNG images (400×300)
- **metadata/**: JSON files with capture info

---

## **Troubleshooting**

### **Display Issues**
- **Upside down**: Check display rotation in firmware
- **No display**: Verify SPI connections and power
- **Garbled text**: Check library installation

### **Sensor Issues**
- **All zeros**: Check multiplexer wiring and power
- **Inconsistent readings**: Verify sensor connections
- **Wrong pixels**: Check multiplexer control pin wiring

### **Button Issues**
- **No response**: Check pull-up resistors
- **Multiple triggers**: Add debouncing (already in firmware)

### **Serial Issues**
- **No communication**: Check USB cable and port
- **Wrong port**: Use `arduino-cli board list` to find correct port

---

## **Technical Specifications**

- **Resolution**: 4×3 pixels (12 total)
- **Color Depth**: 8-bit grayscale
- **Storage**: 20 images maximum
- **Display**: 128×128 ST7735 TFT
- **Microcontroller**: Arduino Nano (ATmega328P)
- **Power**: 5V via USB or external supply
- **Communication**: Serial (USB)

---

## **File Structure**

```
Kodak12/
├── CameraFirmware/
│   └── CameraFirmware.ino    # Main firmware
├── QuickDownload/
│   ├── GetImages.py          # Image download script
│   ├── run.sh               # Run script
│   ├── verify_100x.py       # Image verification
│   └── requirements.txt     # Python dependencies
└── README.md                # This file
```

---

**Charles Gershom** - [charlesgershom.com](https://charlesgershom.com/)
