# Agfa35 Camera Live Stream Viewer

Real-time GUI for viewing live sensor data from the Agfa35 Camera.

## Usage

```bash
python3 livestream.py
```

Or use the run script:
```bash
./run_livestream.sh
```

## Features

- Real-time 400x scale display
- Serial communication with Arduino
- Connect/start/stop controls
- FPS counter and live logging

## Requirements

- Python 3.7+
- pyserial
- tkinter
- Pillow (PIL)
- numpy

## Controls

1. Select serial port (default: auto-detect)
2. Click "Connect" to establish connection
3. Click "Start Stream" to begin live preview
4. Adjust exposure frames for better image quality

## Display

- 7x5 sensor array scaled to 400px
- Grayscale display
- Real-time FPS counter
- Exposure frame accumulation