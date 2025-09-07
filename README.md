# **Lofi Cameras**

### **Simple Digital Cameras**

Two Arduino-based cameras with very low pixel counts. Built with basic components and open source code.

---

## **The Cameras**

### **Agfa35** - 35 Pixel Camera
7×5 pixel sensor array with Arduino Nano, ST7735 display, and EEPROM storage.

**Features:**
- 35 pixels (7×5 array)
- Enhanced EEPROM management
- Real-time preview display
- Gallery mode with knob navigation
- Streaming support
- Enhanced exposure control

### **Kodak12** - 12 Pixel Camera  
4×3 pixel sensor array with Arduino Nano, ST7735 display, multiplexer control, two buttons, knob, and beeper.

**Features:**
- 12 pixels (4×3 array)
- Multiplexer sensor control
- Two-button interface with beeper feedback
- Knob-controlled exposure modes
- Gallery mode and streaming
- Enhanced UI with real-time sensor preview

---

## **Getting Started**

Each camera comes with:
- **Arduino firmware** - Complete source code for the camera operation
- **QuickDownload system** - Python-based image download and conversion tools
- **Documentation** - Detailed setup and usage instructions
- **Example images** - Sample outputs from the cameras

### **Requirements**
- Arduino Nano or compatible microcontroller
- ST7735 TFT display
- Light sensors (LDRs or photodiodes)
- Basic electronic components (resistors, multiplexers, etc.)
- Python 3.6+ for image processing

---

## **The Process**

1. **Build the Hardware** - Assemble the sensor array and control circuits
2. **Upload Firmware** - Flash the Arduino with the camera code  
3. **Take Photos** - Use the camera to capture images
4. **Download Images** - Extract images via serial connection and convert to viewable formats

---

## **License & Usage**

These cameras and their software are open source and available for educational and artistic use. Feel free to build, modify, and experiment with the designs.

**Charles Gershom** - [charlesgershom.com](https://charlesgershom.com/)