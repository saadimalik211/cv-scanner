/*
 * GM812 Scanner - Multi-Core Implementation with Ethernet
 * --------------------------------------------------------
 * This project implements a barcode scanner using the GM812 module
 * with an ESP32-S3 microcontroller and W5500 Ethernet connectivity.
 * It uses a dual-core approach where:
 *  - Core 0: Handles scanner data acquisition and processing
 *  - Core 1: Manages network communication and API integration
 *
 * Thread-safe communication between cores is implemented using FreeRTOS
 * mutexes and a shared data structure.
 */

//=====================================================================
// 1. INCLUDES AND CONFIGURATION
//=====================================================================

#include <ETH.h>           // Ethernet library
#include <SPI.h>           // SPI library for Ethernet
#include <WiFiUdp.h>       // WiFi UDP library (legacy from previous version)
#include <WiFi.h>          // WiFi library for event handling
#include <HTTPClient.h>     // HTTP client for API requests
#include "config.h"        // Project configuration
#include <esp_task_wdt.h>  // ESP32 task watchdog timer

//=====================================================================
// 2. GLOBAL VARIABLES
//=====================================================================

// ----- Debug Configuration -----
const bool DEBUG_MODE = false;  // Set to true for verbose debug output

// ----- Task Handles -----
TaskHandle_t scannerTaskHandle = NULL;  // Handle for scanner task (Core 0)
TaskHandle_t networkTaskHandle = NULL;  // Handle for network task (Core 1)

// ----- Synchronization Objects -----
SemaphoreHandle_t serialMutex;   // Protects access to the Serial interface
SemaphoreHandle_t bufferMutex;   // Protects access to the barcode buffer
SemaphoreHandle_t errorMutex;    // Protects access to error messages

// ----- Network State -----
bool networkConnected = false;   // Tracks Ethernet connection status
WiFiUDP udpClient;               // Legacy UDP client (not used in current version)
bool udpInitialized = false;     // Legacy UDP flag (not used in current version)

// ----- Watchdog Configuration -----
unsigned long lastSuccessfulHeartbeat = 0;  // Track last successful heartbeat
unsigned long consecutiveFailedHeartbeats = 0; // Track consecutive heartbeat failures
unsigned long lastNetworkActivity = 0;     // Track any successful network activity
const unsigned long NETWORK_WATCHDOG_TIMEOUT = 15 * 60 * 1000; // 15 minutes watchdog
const unsigned long MAX_FAILED_HEARTBEATS = 5;  // Number of failures before reconnect

// ----- Error Reporting -----
String pendingErrorMessages = "";      // Buffer for storing error messages to report
const unsigned int MAX_ERROR_LENGTH = 200;  // Maximum length for error message string

// ----- Scanner Buffer -----
char rawBuffer[512];             // Raw data buffer from scanner
int bufferIndex = 0;             // Current position in raw buffer
unsigned long lastDataTime = 0;  // Timestamp of last received data
bool newDataAvailable = false;   // Flag indicating new data is in buffer

// ----- Barcode Data Structure -----
typedef struct {
  char data[256];              // Processed barcode data
  int length;                  // Length of barcode data
  bool newData;                // Flag indicating new barcode is available
  SemaphoreHandle_t mutex;     // Mutex protecting this structure
} BarcodeData_t;

BarcodeData_t barcodeData = {
  .data = {0},
  .length = 0,
  .newData = false
};

//=====================================================================
// 3. UTILITY FUNCTIONS
//=====================================================================

/**
 * Safely prints a message to the serial port using mutex protection
 * 
 * @param message The message to print
 */
void safePrint(const char* message) {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print(message);
    xSemaphoreGive(serialMutex);
  }
}

/**
 * Safely prints a message followed by a newline to the serial port
 * 
 * @param message The message to print
 */
void safePrintln(const char* message) {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println(message);
    xSemaphoreGive(serialMutex);
  }
}

/**
 * Safely prints a formatted message to the serial port
 * 
 * @param format Printf-style format string
 * @param ... Variable arguments for the format string
 */
void safePrintf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print(buffer);
    xSemaphoreGive(serialMutex);
  }
}

/**
 * Encodes a string for URL transmission (percent encoding)
 * 
 * @param str The string to encode
 * @return String The URL-encoded string
 */
String urlEncode(const char* str) {
  String encodedString = "";
  char c;
  for (int i = 0; i < strlen(str); i++) {
    c = str[i];
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encodedString += c;
    } else {
      encodedString += '%';
      encodedString += String(c, HEX);
    }
  }
  return encodedString;
}

/**
 * Records an error message to be sent with the next heartbeat
 * Thread-safe function that appends to the pendingErrorMessages buffer
 * 
 * @param errorMessage The error message to record
 */
void recordError(const char* errorMessage) {
  // Calculate uptime in seconds for timestamp
  unsigned long uptimeSeconds = millis() / 1000;
  
  // Format with timestamp: [uptime] message
  char formattedError[MAX_ERROR_LENGTH];
  snprintf(formattedError, MAX_ERROR_LENGTH, "[%lu] %s", uptimeSeconds, errorMessage);
  
  // Take the mutex to safely update the error message string
  if (xSemaphoreTake(errorMutex, portMAX_DELAY) == pdTRUE) {
    // Add separator if there are already messages
    if (pendingErrorMessages.length() > 0) {
      pendingErrorMessages += "; ";
    }
    
    // Add the new error message
    pendingErrorMessages += formattedError;
    
    // Truncate if too long (URL length limits)
    if (pendingErrorMessages.length() > MAX_ERROR_LENGTH) {
      pendingErrorMessages = pendingErrorMessages.substring(0, MAX_ERROR_LENGTH - 3) + "...";
    }
    
    xSemaphoreGive(errorMutex);
  }
  
  // Also log to serial console
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("ERROR: ");
    Serial.println(errorMessage);
    xSemaphoreGive(serialMutex);
  }
}

//=====================================================================
// 4. ETHERNET AND NETWORK FUNCTIONS
//=====================================================================

/**
 * Handler for Ethernet events
 * 
 * @param event The event type that occurred
 */
void WiFiEvent(arduino_event_id_t event) {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    switch (event) {
      case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        ETH.setHostname("esp32-scanner");
        break;
      case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
      case ARDUINO_EVENT_ETH_GOT_IP:
        networkConnected = true;
        Serial.println("ETH Connected with IP");
        Serial.print("IP Address: ");
        Serial.println(ETH.localIP());
        Serial.print("MAC Address: ");
        Serial.println(ETH.macAddress());
        
        // Record successful connection
        {
          char connMsg[100];
          snprintf(connMsg, sizeof(connMsg), "Ethernet connected with IP: %s", ETH.localIP().toString().c_str());
          recordError(connMsg);
        }
        break;
      case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        networkConnected = false;
        recordError("Ethernet disconnected");
        break;
      case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        networkConnected = false;
        recordError("Ethernet stopped");
        break;
      default:
        break;
    }
    xSemaphoreGive(serialMutex);
  }
}

/**
 * Performs a hard reset of the Ethernet hardware
 * Used when Ethernet appears stuck but still "connected"
 */
void resetEthernet() {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("RESETTING ETHERNET HARDWARE");
    xSemaphoreGive(serialMutex);
  }
  
  // Record this event
  recordError("Ethernet hardware reset performed");
  
  // Hard reset W5500 chip
  pinMode(ETH_RST_PIN, OUTPUT);
  digitalWrite(ETH_RST_PIN, LOW);
  delay(100);
  digitalWrite(ETH_RST_PIN, HIGH);
  
  // Re-initialize Ethernet
  networkConnected = false;
  initEthernet();
}

/**
 * Initializes the Ethernet interface
 */
void initEthernet() {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("Initializing Ethernet...");
    xSemaphoreGive(serialMutex);
  }
  
  // Register event handler
  WiFi.onEvent(WiFiEvent);
  
  // Start Ethernet using W5500 with SPI interface
  if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                 SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.println("ETH start Failed!");
      xSemaphoreGive(serialMutex);
    }
    recordError("Ethernet hardware initialization failed");
    return;
  }
  
  // Configure static IP if enabled
  #if USE_STATIC_IP
    IPAddress localIP(STATIC_IP);
    IPAddress gateway(STATIC_GATEWAY);
    IPAddress subnet(STATIC_SUBNET);
    IPAddress dns1(STATIC_DNS1);
    IPAddress dns2(STATIC_DNS2);
    
    ETH.config(localIP, gateway, subnet, dns1, dns2);
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.println("Using static IP configuration");
      xSemaphoreGive(serialMutex);
    }
  #else
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.println("Using DHCP");
      xSemaphoreGive(serialMutex);
    }
  #endif
  
  // Wait for Ethernet connection with timeout
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("Waiting for Ethernet link...");
    xSemaphoreGive(serialMutex);
  }
  
  unsigned long startTime = millis();
  while (!networkConnected && millis() - startTime < ETH_CONNECT_TIMEOUT) {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.print(".");
      xSemaphoreGive(serialMutex);
    }
    delay(500);
  }
  
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println();
    
    if (networkConnected) {
      Serial.println("Ethernet connected successfully!");
    } else {
      Serial.println("Failed to connect to Ethernet within timeout");
      Serial.println("Continuing without network functionality");
      recordError("Ethernet connection timeout - failed to connect within specified time");
    }
    xSemaphoreGive(serialMutex);
  }
}

/**
 * Sends a heartbeat signal to the API server
 * 
 * @return bool True if heartbeat was successfully sent, false otherwise
 */
bool sendHeartbeat() {
  if (!networkConnected) return false;
  
  HTTPClient http;
  String url = String(API_SERVER);
  
  // Check if API_SERVER already includes http:// or https://
  if (url.startsWith("http")) {
    // Add port only if it's not the default ports (80 for HTTP, 443 for HTTPS)
    if ((url.startsWith("http://") && API_PORT != 80) ||
        (url.startsWith("https://") && API_PORT != 443)) {
      url += ":" + String(API_PORT);
    }
  } else {
    // If no protocol is specified, add it with the port
    url = "http://" + url + ":" + String(API_PORT);
  }
  
  url += "/api/node/qrCodeReaderHeartbeat?nodeUUID=" + String(NODE_UUID) + "&qrReaderUUID=" + String(SCANNER_ID);
  
  // Add error messages if any are pending
  String errorMessages = "";
  if (xSemaphoreTake(errorMutex, portMAX_DELAY) == pdTRUE) {
    if (pendingErrorMessages.length() > 0) {
      errorMessages = pendingErrorMessages;
      url += "&errorMessages=" + urlEncode(errorMessages.c_str());
      // Clear pending messages after sending
      pendingErrorMessages = "";
    }
    xSemaphoreGive(errorMutex);
  }
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 second timeout for heartbeat requests
  
  int httpResponseCode = http.PUT("{}");  // Empty JSON body
  
  bool success = (httpResponseCode >= 200 && httpResponseCode < 300);
  
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    if (success) {
      Serial.println("Heartbeat sent successfully");
      if (errorMessages.length() > 0) {
        Serial.println("Error messages sent with heartbeat");
      }
      consecutiveFailedHeartbeats = 0;
      lastSuccessfulHeartbeat = millis();
      lastNetworkActivity = millis();
    } else {
      Serial.printf("Heartbeat failed, code: %d\n", httpResponseCode);
      Serial.printf("URL: %s\n", url.c_str());
      consecutiveFailedHeartbeats++;
      Serial.printf("Consecutive failed heartbeats: %lu\n", consecutiveFailedHeartbeats);
      
      // Record the error for next successful heartbeat
      char errorMsg[100];
      snprintf(errorMsg, sizeof(errorMsg), "Heartbeat failed with HTTP code %d", httpResponseCode);
      recordError(errorMsg);
    }
    xSemaphoreGive(serialMutex);
  }
  
  http.end();
  return success;
}

/**
 * Sends barcode data to the API server
 * 
 * @param barcodeData Pointer to a null-terminated string containing the barcode data
 * @return bool True if data was successfully sent, false otherwise
 */
bool sendBarcodeData(const char* barcodeData) {
  if (!networkConnected) return false;
  
  HTTPClient http;
  
  // URL encode the barcode data
  String encodedData = urlEncode(barcodeData);
  
  String url = String(API_SERVER);
  
  // Check if API_SERVER already includes http:// or https://
  if (url.startsWith("http")) {
    // Add port only if it's not the default ports (80 for HTTP, 443 for HTTPS)
    if ((url.startsWith("http://") && API_PORT != 80) ||
        (url.startsWith("https://") && API_PORT != 443)) {
      url += ":" + String(API_PORT);
    }
  } else {
    // If no protocol is specified, add it with the port
    url = "http://" + url + ":" + String(API_PORT);
  }
  
  url += "/api/node/recordQRCode?nodeUUID=" + String(NODE_UUID) 
         + "&qrReaderUUID=" + String(SCANNER_ID) + "&data=" + encodedData;
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 second timeout
  
  int httpResponseCode = http.PUT("{}");  // Empty JSON body
  
  bool success = (httpResponseCode >= 200 && httpResponseCode < 300);
  
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    if (success) {
      Serial.println("Barcode data sent successfully");
      lastNetworkActivity = millis();
    } else {
      Serial.printf("Barcode data submission failed, code: %d\n", httpResponseCode);
      Serial.printf("URL: %s\n", url.c_str());
      
      // Record the error for next heartbeat
      char errorMsg[100];
      snprintf(errorMsg, sizeof(errorMsg), "QR code submission failed with HTTP code %d", httpResponseCode);
      recordError(errorMsg);
    }
    xSemaphoreGive(serialMutex);
  }
  
  http.end();
  return success;
}

//=====================================================================
// 5. SCANNER FUNCTIONS
//=====================================================================

/**
 * Initializes the scanner UART interface
 */
void init_scanner() {
  // Initialize the scanner UART
  SCANNER_SERIAL.begin(GM812_BAUD, SERIAL_8N1, GM812_RX_PIN, GM812_TX_PIN);
  Serial.printf("Scanner UART initialized on RX:%d, TX:%d at %d baud\n", 
                GM812_RX_PIN, GM812_TX_PIN, GM812_BAUD);
  
  // Clear any pending data
  while (SCANNER_SERIAL.available()) SCANNER_SERIAL.read();
  
  // Reset buffer
  bufferIndex = 0;
  
  Serial.println("Ready to receive barcode data!");
}

/**
 * Processes raw scanner data to extract barcode information
 * Takes data from the rawBuffer and processes it into barcodeData
 */
void processBarcode() {
  // Take the buffer mutex to ensure exclusive access to the buffer
  if (xSemaphoreTake(bufferMutex, portMAX_DELAY) != pdTRUE) {
    return;
  }
  
  if (bufferIndex == 0) {
    xSemaphoreGive(bufferMutex);
    return;
  }
  
  // Create a local copy of the buffer to process
  char localBuffer[512];
  int localBufferSize = bufferIndex;
  
  memcpy(localBuffer, rawBuffer, bufferIndex);
  localBuffer[bufferIndex] = '\0';
  
  // Reset the global buffer for next data
  bufferIndex = 0;
  newDataAvailable = false;
  
  // Release the buffer mutex
  xSemaphoreGive(bufferMutex);
  
  // Now process the local copy
  
  // Look for real barcode data (printable ASCII text)
  char barcodeBuffer[256] = {0};
  int barcodeLen = 0;
  
  // First scan through to identify if this is a barcode or protocol message
  bool isProtocolMessage = false;
  if (localBufferSize > 2 && (uint8_t)localBuffer[0] == 0x02 && (uint8_t)localBuffer[1] == 0x00) {
    isProtocolMessage = true;
  }
  
  // Take the mutex before any serial output
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    if (isProtocolMessage) {
      // This is a protocol message, just print it for debugging
      if (DEBUG_MODE) {
        Serial.print("Protocol message: ");
        for (int i = 0; i < localBufferSize; i++) {
          Serial.printf("%02X ", (uint8_t)localBuffer[i]);
        }
        Serial.println();
      }
    } 
    else {
      // Extract printable ASCII characters (this is the barcode data)
      for (int i = 0; i < localBufferSize; i++) {
        char c = localBuffer[i];
        if (c >= 32 && c <= 126) { // Printable ASCII
          barcodeBuffer[barcodeLen++] = c;
        }
      }
      
      // Only print if we found some barcode data
      if (barcodeLen > 0) {
        barcodeBuffer[barcodeLen] = '\0'; // Ensure null termination
        
        Serial.println("\n----------------------------------------");
        Serial.printf("BARCODE DETECTED (%d characters)\n", barcodeLen);
        Serial.print("DATA: ");
        Serial.println(barcodeBuffer);
        
        // Print hex values for debug
        if (DEBUG_MODE) {
          Serial.print("HEX: ");
          for (int i = 0; i < barcodeLen; i++) {
            Serial.printf("%02X ", (uint8_t)barcodeBuffer[i]);
            if ((i + 1) % 16 == 0 && i < barcodeLen - 1) {
              Serial.print("\n     ");
            }
          }
        }
        Serial.println("----------------------------------------\n");
        
        // Store barcode data in shared structure for network task
        if (xSemaphoreTake(barcodeData.mutex, portMAX_DELAY) == pdTRUE) {
          strncpy(barcodeData.data, barcodeBuffer, sizeof(barcodeData.data) - 1);
          barcodeData.data[sizeof(barcodeData.data) - 1] = '\0'; // Ensure null-termination
          barcodeData.length = barcodeLen;
          barcodeData.newData = true;
          xSemaphoreGive(barcodeData.mutex);
        }
      }
    }
    // Release the mutex after completing all serial output
    xSemaphoreGive(serialMutex);
  }
}

//=====================================================================
// 6. TASK MANAGEMENT
//=====================================================================

/**
 * Scanner task - runs on Core 1
 * Responsible for reading data from the scanner and processing barcodes
 * 
 * @param pvParameters Task parameters (not used)
 */
void scannerTask(void *pvParameters) {
  // Print which core this task is running on
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("Scanner task running on core: ");
    Serial.println(xPortGetCoreID());
    xSemaphoreGive(serialMutex);
  }
  
  // Variables for tracking scanner activity
  unsigned long lastScannerActivity = millis();
  bool scannerInactivityReported = false;
  const unsigned long SCANNER_INACTIVITY_THRESHOLD = 5 * 60 * 1000; // 5 minutes
  
  // Task loop for scanner operations
  while(true) {
    // Read data from scanner when available
    if (SCANNER_SERIAL.available() > 0) {
      // Take the buffer mutex before accessing the shared buffer
      if (xSemaphoreTake(bufferMutex, portMAX_DELAY) == pdTRUE) {
        // Read all available bytes into our buffer
        while (SCANNER_SERIAL.available() > 0 && bufferIndex < sizeof(rawBuffer) - 1) {
          rawBuffer[bufferIndex++] = SCANNER_SERIAL.read();
          newDataAvailable = true;
          lastDataTime = millis();
        }
        // Release the buffer mutex
        xSemaphoreGive(bufferMutex);
      }
      
      // Update scanner activity timestamp and reset inactivity flag
      lastScannerActivity = millis();
      if (scannerInactivityReported) {
        recordError("Scanner activity resumed after period of inactivity");
        scannerInactivityReported = false;
      }
    }
    
    // If we have new data and there's been a pause in reception, process it
    if (newDataAvailable && millis() - lastDataTime > BARCODE_TIMEOUT) {
      processBarcode();
    }
    
    // Check for scanner inactivity
    if (!scannerInactivityReported && millis() - lastScannerActivity > SCANNER_INACTIVITY_THRESHOLD) {
      recordError("Scanner inactivity detected - no data received for 5+ minutes");
      scannerInactivityReported = true;
    }
    
    // Small delay to prevent watchdog issues and allow other tasks to run
    vTaskDelay(MAIN_LOOP_DELAY / portTICK_PERIOD_MS);
  }
}

/**
 * Network task - runs on Core 0
 * Responsible for maintaining Ethernet connection and sending data to the server
 * 
 * @param pvParameters Task parameters (not used)
 */
void networkTask(void *pvParameters) {
  // Print which core this task is running on
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("Network task running on core: ");
    Serial.println(xPortGetCoreID());
    xSemaphoreGive(serialMutex);
  }
  
  // Subscribe this task to the watchdog
  esp_task_wdt_add(NULL);
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("Network task subscribed to watchdog");
    xSemaphoreGive(serialMutex);
  }
  
  // Initialize Ethernet
  initEthernet();
  
  // Send initial heartbeat when network is connected
  if (networkConnected) {
    if (sendHeartbeat()) {
      lastSuccessfulHeartbeat = millis();
      lastNetworkActivity = millis();
    }
  }
  
  // Network monitoring loop
  unsigned long lastHeartbeatTime = 0;
  unsigned long lastNetworkStatusCheck = 0;
  
  while(true) {
    // Reset watchdog timer to prevent system reset
    esp_task_wdt_reset();
    
    unsigned long currentTime = millis();
    
    // Advanced network status check every 15 seconds
    if (currentTime - lastNetworkStatusCheck > 15000) {
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.printf("Network status: %s, Last activity: %lus ago\n", 
                     networkConnected ? "Connected" : "Disconnected",
                     (currentTime - lastNetworkActivity) / 1000);
        xSemaphoreGive(serialMutex);
      }
      
      // Network watchdog - if no successful activity for NETWORK_WATCHDOG_TIMEOUT, force reset
      if (currentTime - lastNetworkActivity > NETWORK_WATCHDOG_TIMEOUT && lastNetworkActivity > 0) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
          Serial.println("NETWORK WATCHDOG TRIGGERED: No network activity for >15 minutes");
          Serial.println("Performing hardware reset of Ethernet controller");
          xSemaphoreGive(serialMutex);
        }
        
        // Record this event
        char errorMsg[100];
        unsigned long inactivitySecs = (currentTime - lastNetworkActivity) / 1000;
        snprintf(errorMsg, sizeof(errorMsg), "Network watchdog triggered - no activity for %lu seconds", inactivitySecs);
        recordError(errorMsg);
        
        resetEthernet();
        lastNetworkActivity = currentTime; // Reset timer to prevent immediate retry
      }
      
      // Check for too many consecutive heartbeat failures
      if (consecutiveFailedHeartbeats >= MAX_FAILED_HEARTBEATS) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
          Serial.printf("Too many failed heartbeats (%lu), resetting Ethernet\n", 
                       consecutiveFailedHeartbeats);
          xSemaphoreGive(serialMutex);
        }
        
        // Record this event
        char errorMsg[100];
        snprintf(errorMsg, sizeof(errorMsg), "Multiple consecutive heartbeat failures (%lu) triggered reset", 
                 consecutiveFailedHeartbeats);
        recordError(errorMsg);
        
        resetEthernet();
        consecutiveFailedHeartbeats = 0; // Reset counter
      }
      
      // Check Ethernet connection (ETH.linkStatus() might help on some boards)
      if (!networkConnected) {
        // Try to reconnect if connection was lost
        if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
          Serial.println("Ethernet disconnected, attempting to reconnect...");
          xSemaphoreGive(serialMutex);
        }
        // Re-initialize Ethernet
        initEthernet();
        
        // Send heartbeat after reconnection
        if (networkConnected) {
          if (sendHeartbeat()) {
            lastHeartbeatTime = currentTime;
            lastSuccessfulHeartbeat = currentTime;
            lastNetworkActivity = currentTime;
          }
        }
      }
      
      lastNetworkStatusCheck = currentTime;
    }
    
    // Send periodic heartbeat
    if (networkConnected && (currentTime - lastHeartbeatTime > HEARTBEAT_INTERVAL)) {
      sendHeartbeat();
      lastHeartbeatTime = currentTime;
    }
    
    // Check if there's new barcode data to send
    if (networkConnected) {
      if (xSemaphoreTake(barcodeData.mutex, portMAX_DELAY) == pdTRUE) {
        if (barcodeData.newData) {
          // Send barcode data via API
          sendBarcodeData(barcodeData.data);
          
          // Mark data as processed
          barcodeData.newData = false;
        }
        xSemaphoreGive(barcodeData.mutex);
      }
    }
    
    // Small delay to prevent watchdog issues
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

//=====================================================================
// 7. SETUP AND LOOP
//=====================================================================

/**
 * Arduino setup function - called once at startup
 * Initializes hardware, creates tasks, and starts the scheduler
 */
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Enable ESP32 hardware watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 60000,                // 60 second timeout
    .idle_core_mask = (1 << 1),         // Core 1 is idle and not monitored
    .trigger_panic = true               // Trigger a panic/reboot on timeout
  };
  esp_task_wdt_init(&wdt_config);
  
  // Create mutexes before any multitasking
  serialMutex = xSemaphoreCreateMutex();
  bufferMutex = xSemaphoreCreateMutex();
  errorMutex = xSemaphoreCreateMutex();
  barcodeData.mutex = xSemaphoreCreateMutex();
  
  if (serialMutex == NULL || bufferMutex == NULL || 
      errorMutex == NULL || barcodeData.mutex == NULL) {
    Serial.println("Error creating mutexes");
    while(1); // Fatal error
  }
  
  Serial.println("\n\nGM812 Scanner - Multi-Core Implementation with Ethernet");
  
  // Record startup event for first heartbeat
  recordError("System started - firmware version 1.1 with error reporting");
  
  // Initialize scanner UART
  init_scanner();
  
  // Create scanner task on Core 0
  xTaskCreatePinnedToCore(
    scannerTask,          // Function to implement the task
    "ScannerTask",        // Name of the task
    4096,                 // Stack size in words
    NULL,                 // Task input parameter
    1,                    // Priority of the task (1 being low)
    &scannerTaskHandle,   // Task handle
    0                     // Core where the task should run (0 = Core 0)
  );
  
  Serial.println("Scanner task started on Core 0");
  
  // Create network task on Core 1
  xTaskCreatePinnedToCore(
    networkTask,          // Function to implement the task
    "NetworkTask",        // Name of the task
    4096,                 // Stack size in words
    NULL,                 // Task input parameter
    1,                    // Priority of the task (1 being low)
    &networkTaskHandle,   // Task handle
    1                     // Core where the task should run (1 = Core 1)
  );
  
  Serial.println("Network task started on Core 1");
  
  // Subscribe network task to watchdog
  Serial.println("Network watchdog enabled with 60s timeout");
}

/**
 * Arduino loop function - called repeatedly
 * Not used in this implementation as we're using FreeRTOS tasks
 */
void loop() {
  // The main loop is not used as our code runs in dedicated tasks
  // If you want to add code here, be aware it runs on Core 1
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}