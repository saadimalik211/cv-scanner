# CV Scanner - Barcode Scanner System with API Integration

This project provides a complete barcode/QR code scanning solution with two main components:

1. **ESP32-S3-ETH Scanner** - A robust embedded scanner using the GM812 barcode module
2. **Virtual Node Service** - A Node.js server for receiving and logging scanner data

## 📋 Project Overview

This system enables barcode scanning with real-time data transmission to a central server via API calls. The ESP32 scans barcodes and sends the data to the server, which logs the information and provides a simple web interface for monitoring.

## 🔄 System Architecture


1. **GM812 Scanner** connects to the **ESP32-S3-ETH** microcontroller
2. ESP32 processes barcode data and sends it to the **Virtual Node Server** via API calls
3. The server logs the data and displays it in a web interface

## 📦 Project Structure

```
cv-scanner/
├── ESP32-S3-ETH/
│   └── ScanOnly/             # ESP32 scanner firmware
│       ├── ScanOnly.ino      # Main Arduino sketch
│       ├── config.h          # Configuration parameters
│       ├── serialdoc.md      # GM812 scanner documentation
│       └── README.md         # ESP32 project documentation
│
└── simulate-node/            # Virtual Node server
    ├── server.js             # Express.js API server
    ├── package.json          # Node.js dependencies
    ├── Dockerfile            # Container definition
    ├── docker-compose.yml    # Docker Compose configuration
    ├── data/                 # Data storage directory
    └── README.md             # Server documentation
```

## 🧰 ESP32-S3-ETH Scanner

### Hardware Requirements

- ESP32-S3 microcontroller with Ethernet (e.g., Waveshare ESP32-S3-ETH)
- GM812 barcode scanner module
- 5V power supply
- Ethernet connection

### Hardware Setup

**Scanner Connection:**
1. **Scanner Pin 1 (VCC)** → **5V** power supply
2. **Scanner Pin 2 (TXD)** → **GPIO 21** on ESP32 (RX)
3. **Scanner Pin 3 (RXD)** → **GPIO 17** on ESP32 (TX)
4. **Scanner Pin 4 (GND)** → **GND** on ESP32

**Ethernet Connection (Waveshare ESP32-S3-ETH):**
- **MISO** → GPIO 12
- **MOSI** → GPIO 11
- **SCLK** → GPIO 13
- **CS** → GPIO 14
- **INT** → GPIO 10
- **RST** → GPIO 9

### ESP32 Features

- Multi-core task management (scanner on Core 0, network on Core 1)
- Thread-safe data sharing between cores
- Robust barcode processing and filtering
- Configurable static IP or DHCP
- API integration with heartbeat and data submission
- Automatic reconnection on network failure

### ESP32 Configuration

Edit `ESP32-S3-ETH/ScanOnly/config.h` to set:
- Network settings (IP, gateway, subnet)
- API server address and port
- Node UUID and Scanner ID
- Scanner pins and baud rate
- Timing parameters

## 🖥️ Virtual Node Service

### Server Requirements

- Docker and Docker Compose
- Network access to the ESP32 scanner

### Server Features

- Receives heartbeat signals from scanners
- Logs all scanned barcode/QR code data
- Provides a simple web interface for monitoring
- Stores data in log files for analysis
- Containerized for easy deployment

### Server Setup

1. Navigate to the `simulate-node` directory
2. Build and start the container:

```bash
docker-compose up -d
```

3. Access the web interface at `http://<server-ip>:3000`

### API Endpoints

The server provides two main API endpoints:

1. **Scanner Registration/Heartbeat:**
   ```
   PUT /api/node/qrCodeReaderHeartbeat?nodeUUID={nodeId}&qrReaderUUID={scannerId}
   ```

2. **QR Code Data Submission:**
   ```
   PUT /api/node/recordQRCode?nodeUUID={nodeId}&qrReaderUUID={scannerId}&data={qrData}
   ```

### Data Storage

All data is stored in the `simulate-node/data` directory:
- `heartbeats.log` - Records all scanner heartbeats
- `qrcodes.log` - Records all scanned barcode data

## 🚀 Getting Started (Complete System)

### Step 1: Set Up the Virtual Node Server

1. Make sure Docker and Docker Compose are installed on your server
2. Clone this repository:
   ```
   git clone https://github.com/yourusername/cv-scanner.git
   cd cv-scanner/simulate-node
   ```
3. Start the server:
   ```
   docker-compose up -d
   ```
4. Note the server's IP address for the next step

### Step 2: Configure and Deploy the ESP32 Scanner

1. Open `ESP32-S3-ETH/ScanOnly/config.h` in the Arduino IDE
2. Update the API server address to match your server:
   ```c
   #define API_SERVER "http://your-server-ip"
   #define API_PORT 3000
   ```
3. Set the Node UUID and Scanner ID (or use defaults):
   ```c
   #define NODE_UUID "virtual-node"
   #define SCANNER_ID 99
   ```
4. Upload the code to your ESP32-S3 board
5. Connect the ESP32 to Ethernet and power
6. Monitor the serial output at 115200 baud (optional)

### Step 3: Test the System

1. Present a barcode to the scanner
2. Check the ESP32 serial output to confirm scanning
3. View the Virtual Node web interface at `http://<server-ip>:3000`
4. Verify that the barcode data appears in the logs

## 🔧 Troubleshooting

### ESP32 Scanner Issues

- If the scanner LED doesn't light up, check power connections (must be 5V)
- If no data appears when scanning, check the UART pins and baud rate
- If network connection fails, verify Ethernet cable and network settings
- For detailed debugging, set `DEBUG_MODE` to `true` in the ESP32 code

### Server Issues

- If the server fails to start, check Docker and Docker Compose installation
- If the ESP32 can't connect to the server, check network settings and firewall rules
- If the web interface shows no scanners, verify the ESP32 is sending heartbeats
- Check the server logs for error messages:
  ```
  docker-compose logs -f
  ```

## 📝 Development Notes

- The ESP32 scanner is in passive mode - it scans when it sees a barcode
- Thread-safe communication ensures no data loss between cores
- The server uses JSON for all API responses and logging
- The system is designed for robust operation in production environments

## 🔗 Further Resources

- [ESP32-S3 Documentation](https://www.espressif.com/en/products/socs/esp32-s3)
- [GM812 Scanner Manual](https://www.waveshare.com/w/upload/GM812_Series_Barcode_reader_module_User_Manual-V1.2.1.pdf)
- [Express.js Documentation](https://expressjs.com/)
- [Docker Documentation](https://docs.docker.com/) 