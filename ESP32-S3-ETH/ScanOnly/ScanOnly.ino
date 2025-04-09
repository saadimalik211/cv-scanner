/*
 * GM812 Scanner - Multi-Core Implementation with Ethernet
 * Scanner and Ethernet running on separate cores
 */

#include <ETH.h>
#include <SPI.h>
#include <WiFiUdp.h>
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Network status variable
bool networkConnected = false;

// UDP client for broadcasting
WiFiUDP udpClient;
bool udpInitialized = false;

// Shared data structure for barcode information
typedef struct {
  char data[256];
  int length;
  bool newData;
  SemaphoreHandle_t mutex;
} BarcodeData_t;

BarcodeData_t barcodeData = {
  .data = {0},
  .length = 0,
  .newData = false
};

// Simple buffer for collecting raw data
char rawBuffer[512]; 
int bufferIndex = 0;
unsigned long lastDataTime = 0;
bool newDataAvailable = false;

// Task handles for the scanner and network tasks
TaskHandle_t scannerTaskHandle = NULL;
TaskHandle_t networkTaskHandle = NULL;

// Mutexes for synchronization
SemaphoreHandle_t serialMutex;
SemaphoreHandle_t bufferMutex;

// Task function prototypes
void scannerTask(void *pvParameters);
void networkTask(void *pvParameters);

// Debug flag to enable/disable verbose output
const bool DEBUG_MODE = false;

// Safe serial print functions that use the mutex
void safePrint(const char* message) {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print(message);
    xSemaphoreGive(serialMutex);
  }
}

void safePrintln(const char* message) {
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println(message);
    xSemaphoreGive(serialMutex);
  }
}

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

// Ethernet event handler
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
        break;
      case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        networkConnected = false;
        break;
      case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        networkConnected = false;
        break;
      default:
        break;
    }
    xSemaphoreGive(serialMutex);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Create mutexes before any multitasking
  serialMutex = xSemaphoreCreateMutex();
  bufferMutex = xSemaphoreCreateMutex();
  barcodeData.mutex = xSemaphoreCreateMutex();
  
  if (serialMutex == NULL || bufferMutex == NULL || barcodeData.mutex == NULL) {
    Serial.println("Error creating mutexes");
    while(1); // Fatal error
  }
  
  Serial.println("\n\nGM812 Scanner - Multi-Core Implementation with Ethernet");
  
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
}

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
    }
    xSemaphoreGive(serialMutex);
  }
}

void printBarcode() {
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

// Scanner task function running on Core 0
void scannerTask(void *pvParameters) {
  // Print which core this task is running on
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("Scanner task running on core: ");
    Serial.println(xPortGetCoreID());
    xSemaphoreGive(serialMutex);
  }
  
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
    }
    
    // If we have new data and there's been a pause in reception, process it
    if (newDataAvailable && millis() - lastDataTime > BARCODE_TIMEOUT) {
      printBarcode();
    }
    
    // Small delay to prevent watchdog issues and allow other tasks to run
    vTaskDelay(MAIN_LOOP_DELAY / portTICK_PERIOD_MS);
  }
}

// Network task function running on Core 1
void networkTask(void *pvParameters) {
  // Print which core this task is running on
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.print("Network task running on core: ");
    Serial.println(xPortGetCoreID());
    xSemaphoreGive(serialMutex);
  }
  
  // Initialize Ethernet
  initEthernet();
  
  // Send initial heartbeat when network is connected
  if (networkConnected) {
    sendHeartbeat();
  }
  
  // Network monitoring loop
  unsigned long lastHeartbeatTime = 0;
  
  while(true) {
    // Check Ethernet connectivity periodically
    static unsigned long lastConnectionCheck = 0;
    if (millis() - lastConnectionCheck > 30000) { // Check every 30 seconds
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
          sendHeartbeat();
          lastHeartbeatTime = millis();
        }
      }
      lastConnectionCheck = millis();
    }
    
    // Send periodic heartbeat
    if (networkConnected && (millis() - lastHeartbeatTime > HEARTBEAT_INTERVAL)) {
      sendHeartbeat();
      lastHeartbeatTime = millis();
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

// Main loop is empty since we're using FreeRTOS tasks
void loop() {
  // The main loop is not used as our code runs in dedicated tasks
  // If you want to add code here, be aware it runs on Core 1
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// Function to send heartbeat
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
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.PUT("{}");  // Empty JSON body
  
  bool success = (httpResponseCode >= 200 && httpResponseCode < 300);
  
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    if (success) {
      Serial.println("Heartbeat sent successfully");
    } else {
      Serial.printf("Heartbeat failed, code: %d\n", httpResponseCode);
      Serial.printf("URL: %s\n", url.c_str());
    }
    xSemaphoreGive(serialMutex);
  }
  
  http.end();
  return success;
}

// Function to send barcode data
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
  
  int httpResponseCode = http.PUT("{}");  // Empty JSON body
  
  bool success = (httpResponseCode >= 200 && httpResponseCode < 300);
  
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    if (success) {
      Serial.println("Barcode data sent successfully");
    } else {
      Serial.printf("Barcode data submission failed, code: %d\n", httpResponseCode);
      Serial.printf("URL: %s\n", url.c_str());
    }
    xSemaphoreGive(serialMutex);
  }
  
  http.end();
  return success;
}

// Helper function to URL encode a string
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