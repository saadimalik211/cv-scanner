# GM812 Barcode Scanner - Multi-Core Ethernet Implementation

This project provides a robust implementation for interfacing with the GM812 barcode scanner module using an ESP32-S3 microcontroller with Ethernet connectivity.

## Hardware Setup

### Scanner Connection
Connect the GM812 Scanner to the ESP32-S3 as follows:

1. **Scanner Pin 1 (VCC)** → **5V** power supply
2. **Scanner Pin 2 (TXD)** → **GPIO 21** on ESP32 (RX)
3. **Scanner Pin 3 (RXD)** → **GPIO 17** on ESP32 (TX)
4. **Scanner Pin 4 (GND)** → **GND** on ESP32

> **Important:** The scanner requires 5V power, not 3.3V. The TX/RX pins are 3.3V compatible.

### Ethernet Connection
This project uses the Waveshare ESP32-S3-ETH module with W5500 Ethernet chip:

- **MISO** → GPIO 12
- **MOSI** → GPIO 11
- **SCLK** → GPIO 13
- **CS** → GPIO 14
- **INT** → GPIO 10
- **RST** → GPIO 9

## Project Structure

- `ScanOnly.ino`: Main Arduino sketch file with multi-core implementation
- `config.h`: Pin configuration, network settings, and timing parameters
- `serialdoc.md`: Comprehensive GM812 scanner documentation
- `README.md`: This file

## How It Works

This implementation:

1. Uses FreeRTOS multi-core task management:
   - **Core 0**: Dedicated to handling scanner data
   - **Core 1**: Manages Ethernet/UDP network communications

2. Provides robust barcode processing:
   - Captures data from the scanner UART
   - Filters out protocol messages to extract valid barcode data
   - Thread-safe data sharing between cores using mutexes

3. Network features:
   - Configurable static IP or DHCP
   - UDP broadcasting of scanned barcodes to network clients
   - Automatic Ethernet reconnection on link failure

## Usage

1. Configure network settings in `config.h` to match your network
2. Upload the code to your ESP32-S3 board
3. Open the Serial Monitor at 115200 baud
4. Connect the Ethernet cable
5. Scanned barcodes will appear in:
   - Serial console
   - Broadcast via UDP to port 4210

## Customization

You can modify `config.h` to change:
- Scanner pin assignments
- Network settings (IP, gateway, subnet, DNS)
- UDP broadcast port and destination
- Timing parameters

## Troubleshooting

- If the scanner LED illuminates but no data appears, try repositioning the barcode or adjusting lighting conditions.
- If Ethernet fails to connect, check your cable and network settings.
- For detailed debugging, set DEBUG_MODE to true in the code.

## Notes

- This implementation uses thread-safe communication between cores.
- The scanner is in passive mode - it will scan when it sees a barcode without requiring triggers.
- UDP broadcasts allow multiple network clients to receive scanned data simultaneously. 
