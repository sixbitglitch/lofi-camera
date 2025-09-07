#!/usr/bin/env python3

import serial
import time
import tkinter as tk
from tkinter import ttk, messagebox
import threading
from PIL import Image, ImageTk
import numpy as np

class Agfa35CameraStreamer:
    def __init__(self):
        self.serial_port = None
        self.streaming = False
        self.root = None
        self.canvas = None
        self.image_label = None
        self.status_label = None
        self.connect_button = None
        self.start_button = None
        self.stop_button = None
        
        self.image_width = 7
        self.image_height = 5
        self.total_pixels = self.image_width * self.image_height
        self.scale_factor = 400 // max(self.image_width, self.image_height)
        self.display_size = (self.image_width * self.scale_factor, self.image_height * self.scale_factor)
        
        self.current_frame = np.zeros((self.image_height, self.image_width), dtype=np.uint8)
        self.frame_count = 0
        self.last_fps_time = time.time()
        
        self.exposure_frames = 1
        self.exposure_buffer = []
        self.max_exposure_frames = 20
        
        self.setup_gui()
        
    def setup_gui(self):
        self.root = tk.Tk()
        self.root.title("Agfa35 Camera Live Stream")
        self.root.geometry("600x500")
        
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(0, weight=1)
        main_frame.columnconfigure(1, weight=1)
        main_frame.rowconfigure(2, weight=1)
        
        # Connection controls
        ttk.Label(main_frame, text="Port:").grid(row=0, column=0, sticky=tk.W, padx=(0, 5))
        self.port_var = tk.StringVar(value="auto")
        port_combo = ttk.Combobox(main_frame, textvariable=self.port_var, width=15)
        port_combo.grid(row=0, column=1, sticky=tk.W, padx=(0, 10))
        port_combo['values'] = ['auto', '/dev/cu.usbserial-10', '/dev/ttyUSB0', '/dev/ttyACM0']
        
        self.connect_button = ttk.Button(main_frame, text="Connect", command=self.connect_camera)
        self.connect_button.grid(row=0, column=2, padx=(0, 5))
        
        # Stream controls
        self.start_button = ttk.Button(main_frame, text="Start Stream", command=self.start_stream, state=tk.DISABLED)
        self.start_button.grid(row=1, column=0, padx=(0, 5), pady=5)
        
        self.stop_button = ttk.Button(main_frame, text="Stop Stream", command=self.stop_stream, state=tk.DISABLED)
        self.stop_button.grid(row=1, column=1, sticky=tk.W, pady=5)
        
        # Status
        self.status_label = ttk.Label(main_frame, text="Not connected")
        self.status_label.grid(row=1, column=2, sticky=tk.W, padx=(10, 0), pady=5)
        
        # Image display
        self.canvas = tk.Canvas(main_frame, width=self.display_size[0], height=self.display_size[1], bg='black')
        self.canvas.grid(row=2, column=0, columnspan=3, pady=10)
        
        # Exposure controls
        exposure_frame = ttk.LabelFrame(main_frame, text="Exposure", padding="5")
        exposure_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=5)
        
        ttk.Label(exposure_frame, text="Frames:").grid(row=0, column=0, sticky=tk.W)
        self.exposure_var = tk.IntVar(value=1)
        exposure_spin = ttk.Spinbox(exposure_frame, from_=1, to=self.max_exposure_frames, 
                                   textvariable=self.exposure_var, width=5)
        exposure_spin.grid(row=0, column=1, padx=(5, 10))
        
        ttk.Button(exposure_frame, text="Apply", command=self.update_exposure).grid(row=0, column=2)
        
        # FPS display
        self.fps_label = ttk.Label(main_frame, text="FPS: 0")
        self.fps_label.grid(row=4, column=0, sticky=tk.W, pady=5)
        
    def connect_camera(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.serial_port = None
        
        port = self.port_var.get()
        if port == "auto":
            port = self.find_arduino_port()
        
        if not port:
            messagebox.showerror("Error", "No Arduino port found")
            return
        
        try:
            self.serial_port = serial.Serial(port, 115200, timeout=1)
            time.sleep(2)
            self.status_label.config(text=f"Connected to {port}")
            self.connect_button.config(text="Disconnect")
            self.start_button.config(state=tk.NORMAL)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to connect: {e}")
    
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
    
    def start_stream(self):
        if not self.serial_port or not self.serial_port.is_open:
            messagebox.showerror("Error", "Not connected to camera")
            return
        
        self.streaming = True
        self.start_button.config(state=tk.DISABLED)
        self.stop_button.config(state=tk.NORMAL)
        self.status_label.config(text="Streaming...")
        
        self.stream_thread = threading.Thread(target=self.stream_loop, daemon=True)
        self.stream_thread.start()
    
    def stop_stream(self):
        self.streaming = False
        self.start_button.config(state=tk.NORMAL)
        self.stop_button.config(state=tk.DISABLED)
        self.status_label.config(text="Stopped")
    
    def stream_loop(self):
        while self.streaming:
            try:
                if self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line.startswith("STREAM,") and line.endswith(",STREAM"):
                        pixel_data = line[7:-7]
                        pixel_values = pixel_data.split(',')
                        
                        if len(pixel_values) == self.total_pixels:
                            pixels = []
                            for pixel in pixel_values:
                                if pixel.strip():
                                    try:
                                        value = int(pixel.strip())
                                        pixels.append(max(0, min(255, value // 256)))
                                    except ValueError:
                                        continue
                            
                            if len(pixels) == self.total_pixels:
                                frame = np.array(pixels).reshape(self.image_height, self.image_width)
                                self.exposure_buffer.append(frame)
                                
                                if len(self.exposure_buffer) >= self.exposure_frames:
                                    combined_frame = np.mean(self.exposure_buffer, axis=0).astype(np.uint8)
                                    self.exposure_buffer = []
                                    self.update_display(combined_frame)
                
                time.sleep(0.01)
            except Exception as e:
                print(f"Stream error: {e}")
                break
    
    def update_display(self, frame):
        self.current_frame = frame
        self.frame_count += 1
        
        # Update FPS
        current_time = time.time()
        if current_time - self.last_fps_time >= 1.0:
            fps = self.frame_count / (current_time - self.last_fps_time)
            self.fps_label.config(text=f"FPS: {fps:.1f}")
            self.frame_count = 0
            self.last_fps_time = current_time
        
        # Create display image
        img = Image.fromarray(frame, mode='L')
        img = img.resize(self.display_size, Image.NEAREST)
        
        # Convert to PhotoImage
        photo = ImageTk.PhotoImage(img)
        
        # Update canvas
        self.canvas.delete("all")
        self.canvas.create_image(self.display_size[0]//2, self.display_size[1]//2, image=photo)
        self.canvas.image = photo
    
    def update_exposure(self):
        self.exposure_frames = self.exposure_var.get()
        self.exposure_buffer = []
    
    def run(self):
        self.root.mainloop()
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()

def main():
    print("Agfa35 Camera Live Stream Viewer")
    print("Connect to your camera and click 'Start Stream' to begin")
    
    app = Agfa35CameraStreamer()
    app.run()

if __name__ == "__main__":
    main()