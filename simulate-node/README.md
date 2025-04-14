# Virtual Node Service

This service simulates a node that receives QR code scanner data via API calls. It's designed to work with the ESP32-S3-ETH QR code scanner project.

## Features

- Receives heartbeat signals from QR code scanners
- Receives and logs QR code data
- Provides a simple web interface to view registered scanners
- Logs all events to files for later analysis
- Compatible with the enhanced ESP32 firmware with network resilience features

## API Endpoints

- `PUT /api/node/qrCodeReaderHeartbeat?nodeUUID={nodeId}&qrReaderUUID={scannerId}`
  - Registers/updates a QR code reader
  - Optional parameter: `errorMessages` for reporting errors

- `PUT /api/node/recordQRCode?nodeUUID={nodeId}&qrReaderUUID={scannerId}&data={qrData}`
  - Records a scanned QR code
  - Required parameter: `data` containing the QR code contents

## Setup and Running

### Using Docker Compose

1. Make sure you have Docker and Docker Compose installed
2. Build and start the container:

```bash
docker-compose up -d
```

3. View logs:

```bash
docker-compose logs -f
```

4. Access the web interface at http://localhost:3000

### Testing the API

You can test the API endpoints using curl:

```bash
# Test heartbeat
curl -X PUT "http://localhost:3000/api/node/qrCodeReaderHeartbeat?nodeUUID=virtual-node&qrReaderUUID=99"

# Test QR code data submission
curl -X PUT "http://localhost:3000/api/node/recordQRCode?nodeUUID=virtual-node&qrReaderUUID=99&data=TEST123456"
```

## Data Storage

All data is stored in the `data` directory:
- `heartbeats.log` - Records all heartbeat events (including network reconnections)
- `qrcodes.log` - Records all scanned QR codes

## Configuration

You can configure the service using environment variables in the `docker-compose.yml` file:
- `PORT` - The port the service listens on (default: 3000)
- `NODE_ENV` - The Node.js environment (production, development)

## Analyzing Data

The included `find_missing_qr_seconds.py` script can be used to identify gaps in the heartbeat or scan data, which can help diagnose connectivity issues:

```bash
python find_missing_qr_seconds.py
``` 