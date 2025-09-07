# Kodak12 Camera Image Downloader

Downloads and converts images from the Kodak12 Camera Arduino.

## Usage

```bash
python3 GetImages.py
```

Or use the run script:
```bash
./run.sh
```

## Options

- `--port PORT`: Serial port (auto-detect if not specified)
- `--baudrate BAUD`: Baud rate (default: 115200)
- `--max-images N`: Maximum images to download (default: 20)
- `--list-ports`: List available serial ports

## Output

- `exported_images/`: PNG images (4x3 pixels)
- `scaled_images/`: 100x scaled PNG images (400x300 pixels)
- `source_images/`: Raw pixel data files
- `metadata/`: JSON metadata files

## Requirements

- Python 3.6+
- pyserial
- Pillow (PIL)