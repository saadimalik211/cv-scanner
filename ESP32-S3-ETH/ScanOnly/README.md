# GM812 Barcode Scanner - Minimal Implementation

This project provides a minimal, reliable implementation for interfacing with the GM812 barcode scanner module using an ESP32-S3 microcontroller.

## Hardware Setup

Connect the GM812 Scanner to the ESP32-S3 as follows:

1. **Scanner Pin 1 (VCC)** → **5V** power supply
2. **Scanner Pin 2 (TXD)** → **GPIO 21** on ESP32 (RX)
3. **Scanner Pin 3 (RXD)** → **GPIO 17** on ESP32 (TX)
4. **Scanner Pin 4 (GND)** → **GND** on ESP32

> **Important:** The scanner requires 5V power, not 3.3V. The TX/RX pins are 3.3V compatible.

## Project Structure

- `ScanOnly.ino`: Main Arduino sketch file
- `config.h`: Pin configuration and settings
- `README.md`: This file

## How It Works

This implementation:

1. Initializes the scanner with minimal, essential commands:
   - Factory reset to ensure clean state
   - Enable all barcode types
   - Enable UART output
   - Save configuration to flash memory

2. Listens for barcode data while filtering out protocol messages.

3. Automatically reinitializes the scanner if no activity is detected for a minute (to handle potential scanner power-down issues).

## Usage

1. Upload the code to your ESP32-S3 board
2. Open the Serial Monitor at 115200 baud
3. Scanned barcodes will appear in the serial console
4. The scanner is in passive mode - it will scan and beep when it sees a barcode without requiring triggers

## Customization

You can modify `config.h` to change pin assignments or the baud rate as needed for your hardware configuration.

## Troubleshooting

- If the scanner LED illuminates but no data appears, try repositioning the barcode or adjusting lighting conditions.
- If the scanner powers off, the code will automatically reinitialize it after the timeout period.
- Verify that you're using 5V power for the scanner, as 3.3V is insufficient.

## Notes

- This implementation is based on the GM812 Serial Documentation and extensive testing.
- The scanner outputs barcode data in plain ASCII format with a carriage return (0x0D) at the end. 