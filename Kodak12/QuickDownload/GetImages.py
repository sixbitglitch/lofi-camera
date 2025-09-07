#!/usr/bin/env python3

import serial
import time
import os
import sys
from PIL import Image
import argparse
from datetime import datetime
import json

class Kodak12Downloader:
    def __init__(self, port=None, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_connection = None
        self.image_width = 4
        self.image_height = 3
        self.pixels_per_image = 12
        self.max_images = 20
        
        self.export_dir = "exported_images"
        self.source_dir = "source_images"
        self.scaled_dir = "scaled_images"
        self.metadata_dir = "metadata"
        os.makedirs(self.export_dir, exist_ok=True)
        os.makedirs(self.source_dir, exist_ok=True)
        os.makedirs(self.scaled_dir, exist_ok=True)
        os.makedirs(self.metadata_dir, exist_ok=True)
    
    def find_arduino_port(self):
        import serial.tools.list_ports
        
        ports = serial.tools.list_ports.comports()
        for port in ports:
            if any(identifier in port.description.lower() for identifier in 
                   ['arduino', 'usb', 'ch340', 'cp210', 'ftdi']):
                return port.device
        
        if ports:
            return ports[0].device
        return None
    
    def connect(self):
        if not self.port:
            self.port = self.find_arduino_port()
        
        if not self.port:
            print("No Arduino port found.")
            return False
        
        try:
            self.serial_connection = serial.Serial(self.port, self.baudrate, timeout=10)
            print(f"Connected to {self.port}")
            time.sleep(2)
            return True
        except serial.SerialException as e:
            print(f"Connection failed: {e}")
            return False
    
    def disconnect(self):
        if self.serial_connection and self.serial_connection.is_open:
            self.serial_connection.close()
    
    def wait_for_begin(self):
        print("Waiting for camera...")
        timeout = 30
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            if self.serial_connection.in_waiting > 0:
                line = self.serial_connection.readline().decode('utf-8', errors='ignore').strip()
                if "BEGIN" in line:
                    return True
                elif "Kodak12 Camera V2 Ready" in line:
                    pass
            time.sleep(0.1)
        
        return False
    
    def parse_image_data(self, data_line):
        try:
            if data_line.startswith("IMAGE,") and data_line.endswith(",IMAGE"):
                pixel_data = data_line[6:-6]
                pixel_values = pixel_data.split(',')
                
                pixels = []
                for pixel in pixel_values:
                    if pixel.strip():
                        try:
                            value = int(pixel.strip())
                            pixels.append(max(0, min(255, value)))
                        except ValueError:
                            return None
                
                if len(pixels) == self.pixels_per_image:
                    return pixels
                else:
                    print(f"Expected {self.pixels_per_image} pixels, got {len(pixels)}")
                    return None
            else:
                print(f"Invalid format: {data_line[:50]}...")
                return None
                
        except Exception as e:
            print(f"Parse error: {e}")
            return None
    
    def pixels_to_image(self, pixels):
        if len(pixels) != self.pixels_per_image:
            print(f"Invalid pixel count: {len(pixels)}")
            return None
        
        img = Image.new('L', (self.image_width, self.image_height))
        img.putdata(pixels)
        return img
    
    def save_image(self, pixels, image_number):
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        img = self.pixels_to_image(pixels)
        if img:
            filename = f"image_{image_number:03d}_{timestamp}.png"
            filepath = os.path.join(self.export_dir, filename)
            img.save(filepath)
            print(f"Saved: {filename}")
            
            scaled_img = img.resize((self.image_width * 100, self.image_height * 100), Image.NEAREST)
            scaled_filename = f"image_{image_number:03d}_{timestamp}_100x.png"
            scaled_filepath = os.path.join(self.scaled_dir, scaled_filename)
            scaled_img.save(scaled_filepath)
        
        raw_filename = f"image_{image_number:03d}_{timestamp}.raw"
        raw_path = os.path.join(self.source_dir, raw_filename)
        
        with open(raw_path, 'w') as f:
            f.write(f"# Kodak12 Camera V2 Raw Data - Image {image_number}\n")
            f.write(f"# Timestamp: {timestamp}\n")
            f.write(f"# Dimensions: {self.image_width}x{self.image_height}\n")
            f.write(f"# EEPROM Address: {(image_number * self.pixels_per_image) + 24}\n")
            f.write("# Pixel values (8-bit, 0-255):\n")
            
            for i, pixel in enumerate(pixels):
                if i % self.image_width == 0:
                    f.write("\n")
                f.write(f"{pixel:3d} ")
            f.write("\n")
        
        metadata = {
            "image_number": image_number,
            "timestamp": timestamp,
            "dimensions": f"{self.image_width}x{self.image_height}",
            "pixel_count": len(pixels),
            "eeprom_address": (image_number * self.pixels_per_image) + 24,
            "min_value": min(pixels),
            "max_value": max(pixels),
            "avg_value": sum(pixels) / len(pixels)
        }
        
        metadata_filename = f"image_{image_number:03d}_{timestamp}_metadata.json"
        metadata_path = os.path.join(self.metadata_dir, metadata_filename)
        with open(metadata_path, 'w') as f:
            json.dump(metadata, f, indent=2)
    
    def download_images(self):
        if not self.connect():
            return False
        
        try:
            if not self.wait_for_begin():
                print("Camera not ready")
                return False
            
            print("Downloading images...")
            image_count = 0
            download_metadata = {
                "download_timestamp": datetime.now().isoformat(),
                "camera_version": "Kodak12_V2",
                "images_downloaded": 0,
                "total_images_found": 0
            }
            
            while image_count < self.max_images:
                if self.serial_connection.in_waiting > 0:
                    line = self.serial_connection.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line.startswith("IMAGE,") and line.endswith(",IMAGE"):
                        pixels = self.parse_image_data(line)
                        if pixels:
                            self.save_image(pixels, image_count)
                            image_count += 1
                            download_metadata["images_downloaded"] = image_count
                            print(f"Downloaded {image_count}/{self.max_images} images")
                    
                    elif "END" in line:
                        print("Download complete")
                        break
                
                time.sleep(0.01)
            
            download_metadata["total_images_found"] = image_count
            download_metadata["download_summary"] = f"Downloaded {image_count} images from Kodak12 Camera V2"
            
            summary_filename = f"download_summary_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            summary_path = os.path.join(self.metadata_dir, summary_filename)
            with open(summary_path, 'w') as f:
                json.dump(download_metadata, f, indent=2)
            
            print(f"Download complete: {image_count} images")
            return True
            
        except Exception as e:
            print(f"Download error: {e}")
            return False
        finally:
            self.disconnect()
    
    def list_ports(self):
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        print("Available ports:")
        for port in ports:
            print(f"  {port.device}: {port.description}")
    
    def create_readme(self):
        readme_content = """# Kodak12 Camera V2 Image Downloader

This script downloads images from the Kodak12 Camera V2 Arduino and converts them to various formats.

## Features

- Downloads up to 20 images from the EEPROM system
- Converts to PNG format (original and 100x scaled)
- Saves raw pixel data and metadata
- Auto-detects Arduino port

## Usage

```bash
python3 GetImages.py [options]
```

Options:
- `--port PORT`: Serial port (auto-detect if not specified)
- `--baudrate BAUD`: Baud rate (default: 115200)
- `--list-ports`: List available serial ports
- `--max-images N`: Maximum images to download (default: 20)

## Output Files

- `exported_images/`: Original 4x3 PNG images
- `scaled_images/`: 100x scaled PNG images (400x300)
- `source_images/`: Raw pixel data files
- `metadata/`: JSON metadata files

## Requirements

- Python 3.6+
- pyserial
- Pillow (PIL)
"""
        
        with open("README.md", "w") as f:
            f.write(readme_content)
        print("Created README.md")

def main():
    parser = argparse.ArgumentParser(description='Download images from Kodak12 Camera V2 Arduino')
    parser.add_argument('--port', '-p', help='Serial port (auto-detect if not specified)')
    parser.add_argument('--baudrate', '-b', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--list-ports', '-l', action='store_true', help='List available serial ports')
    parser.add_argument('--verbose', '-v', action='store_true', help='Enable verbose output')
    parser.add_argument('--max-images', '-m', type=int, default=20, help='Maximum images to download (default: 20)')
    
    args = parser.parse_args()
    
    downloader = Kodak12Downloader(port=args.port, baudrate=args.baudrate)
    downloader.max_images = args.max_images
    
    if args.list_ports:
        downloader.list_ports()
        return
    
    if not os.path.exists("README.md"):
        downloader.create_readme()
    
    print("Kodak12 Camera V2 Image Downloader")
    print("=" * 40)
    print(f"Max images: {downloader.max_images}")
    print(f"Port: {downloader.port or 'Auto-detect'}")
    print(f"Baud rate: {downloader.baudrate}")
    print("=" * 40)
    
    success = downloader.download_images()
    if success:
        print("Download completed successfully!")
    else:
        print("Download failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()