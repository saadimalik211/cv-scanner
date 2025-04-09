#ifndef CONFIG_H
#define CONFIG_H

// W5500 Ethernet pins for Waveshare ESP32-S3-ETH
#define ETH_MISO_PIN       12
#define ETH_MOSI_PIN       11
#define ETH_SCLK_PIN       13
#define ETH_CS_PIN         14
#define ETH_INT_PIN        10
#define ETH_RST_PIN        9

// Network configuration - adjust to match your network
#define USE_STATIC_IP      true   // Set to false to use DHCP instead
#define STATIC_IP          192,168,0,123
#define STATIC_GATEWAY     192,168,0,1
#define STATIC_SUBNET      255,255,255,0
#define STATIC_DNS1        8,8,8,8
#define STATIC_DNS2        8,8,4,4

// UDP Broadcast Configuration
#define UDP_BROADCAST_PORT 4210
#define UDP_BROADCAST_IP   255,255,255,255

// GM812 Scanner Configuration
#define GM812_RX_PIN  21     // ESP32 RX pin connected to GM812 TX
#define GM812_TX_PIN  17     // ESP32 TX pin connected to GM812 RX
#define GM812_BAUD    9600   // Default baud rate for GM812 (9600 bps per documentation)

// Define the serial port for scanner directly
#define SCANNER_SERIAL Serial2

// Timing configuration
#define BARCODE_TIMEOUT    100     // ms to wait after receiving barcode data
#define MAIN_LOOP_DELAY    10      // ms delay in main loop
#define ETH_CONNECT_TIMEOUT 30000  // ms to wait for Ethernet connection

#endif // CONFIG_H 