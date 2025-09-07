# Agfa35 Camera Image Downloader

Downloads and converts images from the Agfa35 Camera Arduino.

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
- `--max-images N`: Maximum images to download (default: 14)
- `--list-ports`: List available serial ports

## Output

- `exported_images/`: PNG images (7x5 pixels)
- `scaled_images/`: 100x scaled PNG images (700x500 pixels)
- `source_images/`: Raw pixel data files
- `metadata/`: JSON metadata files

## Requirements

- Python 3.6+
- pyserial
- Pillow (PIL)