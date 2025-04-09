# GM812 Barcode Scanner - ESP32 Serial Integration

This document serves as a **complete reference** for interfacing the GM812-S / GM812-L Barcode Scanner Module with an ESP32 (or any microcontroller) via UART, including **all serial commands and configuration options** documented in the manufacturer's PDF.

---

## 🔌 UART Interface

**Default Serial Settings:**
- **Voltage Level:** 3.3V TTL (ESP32-safe)
- **Baud Rate:** `9600` bps (configurable)
- **Data Bits:** `8`
- **Stop Bits:** `1`
- **Parity:** `None`
- **Flow Control:** `None`

---

## 📟 6-Pin Connector Pinout
| Pin | Name | Direction | Description           |
|-----|------|-----------|-----------------------|
| 1   | VCC  | Input     | +5V Power Input       |
| 2   | TXD  | Output    | Scanner → ESP32       |
| 3   | RXD  | Input     | ESP32 → Scanner       |
| 4   | GND  | -         | Ground                |
| 5   | D-   | I/O       | USB Data - (not used) |
| 6   | D+   | I/O       | USB Data + (not used) |

> ⚠️ Do NOT connect VCC to 3.3V. Scanner requires 5V power.

---

## 🔁 Basic Serial Commands (Appendix A & B)

| Function | Command |
|----------|---------|
| **Trigger Mode** | `7E 00 08 01 00 02 01 AB CD` |
| **Save Settings to Flash** | `7E 00 09 01 00 00 00 DE C8` |
| **Set Baud to 9600** | `7E 00 08 02 00 2A 39 01 A7 EA` |
| **Find Baud Rate** | `7E 00 07 01 00 2A 02 D8 0F` |
| **OUT1 Low** | `7E 00 08 01 00 E7 00 AB CD` |
| **OUT1 High** | `7E 00 08 01 00 E7 01 AB CD` |
| **Enable AIM ID** | `7E 00 08 01 00 D0 80 AB CD` |
| **Disable AIM ID** | `7E 00 08 01 00 D0 00 AB CD` |
| **Full Area + All Codes** | `7E 00 08 01 00 2C 02 AB CD` |
| **Enable Code39** | `7E 00 08 01 00 36 01 AB CD` |
| **Set Code39 Min Len** | `7E 00 08 01 00 37 00 AB CD` |
| **Set Code39 Max Len** | `7E 00 08 01 00 38 FF AB CD` |
| **Output Start Part** | `7E 00 08 01 00 B0 01 AB CD` |
| **Cut M from Start** | `7E 00 08 01 00 B1 FF AB CD` |
| **Output End Part** | `7E 00 08 01 00 B0 02 AB CD` |
| **Cut N from End** | `7E 00 08 01 00 B2 FF AB CD` |
| **Output Center Part** | `7E 00 08 01 00 B0 03 AB CD` |
| **Cut 3 Chars Start** | `7E 00 08 01 00 B1 03 AB CD` |
| **Cut 2 Chars End** | `7E 00 08 01 00 B2 02 AB CD` |

---

## 📥 Zone Bit Access (Read/Write)

**Read Zone Bit:**
```hex
7E 00 07 01 [ADDR_H] [ADDR_L] [LEN] [CRC_H] [CRC_L]
```
**Write Zone Bit:**
```hex
7E 00 08 01 [ADDR_H] [ADDR_L] [DATA] [CRC_H] [CRC_L]
```
**Save All Zone Bits to Flash:**
```hex
7E 00 09 01 00 00 00 [CRC_H] [CRC_L]
```

To **reset** zone bits to defaults:
```hex
7E 00 08 01 00 D9 50 [CRC_H] [CRC_L]
```

---

## 🧠 Zone Bit Map (Full)

| Address | Description |
|---------|-------------|
| `0x0000` | Working mode, LED control, mute, lighting control |
| `0x0001` | Output mode, trigger mode control |
| `0x0002` | Trigger command response time, power timeout |
| `0x0003` | HID/USB settings, enter behavior |
| `0x0004` | Image stabilization time |
| `0x0005` | Minimum read interval |
| `0x0006` | Timeout in continuous mode |
| `0x0007` | Max brightness |
| `0x0008` | Auto brightness |
| `0x0009` | Image gain value |
| `0x000A` | Beep frequency (Hz) |
| `0x000B` | Beep duration (ms) |
| `0x000C` | HID interval, caps lock simulation, buzzer mode |
| `0x000D` | Output encoding (0=RAW, 1=UTF8, 2=GBK, 3=Unicode) |
| `0x000E` | Output delimiter (0=CR, 1=LF, 2=CRLF, 3=TAB, 4=None) |
| `0x000F` | USB keyboard compatibility flags |
| `0x0010` | Continuous mode indicator light |
| `0x0011` | Output hex representation |
| `0x0020` | Output interval time |
| `0x0021` | Output timeout |
| `0x0022` | Append preamble length |
| `0x0023` | Append suffix length |
| `0x0024`–`0x0029` | Serial interface settings |
| `0x0030`–`0x005F` | Symbology enable/disable flags |
| `0x0060`–`0x009F` | Symbology min/max length settings |
| `0x00A0`–`0x00AF` | Reserved |
| `0x00B0` | Output segment control (0=All, 1=Start, 2=End, 3=Middle) |
| `0x00B1` | Number of chars to remove from start |
| `0x00B2` | Number of chars to remove from end |
| `0x00B3` | Number of chars to remove from middle |
| `0x00C0`–`0x00D0` | Reserved or specialized batch settings |
| `0x00D9` | Special control commands (e.g., 0x50 = reset all zone bits) |
| `0x00E7` | OUT1 GPIO pin control |
| `0x0100`–`0x0115` | Prefix characters (max 22 bytes) |
| `0x0116`–`0x0132` | Suffix characters (max 22 bytes) |

---

## 🔐 CRC Algorithm

```c
unsigned int crc_cal_by_bit(unsigned char* ptr, unsigned int len) {
    unsigned int crc = 0;
    while(len-- != 0) {
        for(unsigned char i = 0x80; i != 0; i /= 2) {
            crc *= 2;
            if ((crc & 0x10000) != 0) crc ^= 0x11021;
            if ((*ptr & i) != 0) crc ^= 0x1021;
        }
        ptr++;
    }
    return crc;
}
```

> You can skip CRC by using `AB CD` as placeholder.

---

## 📤 Output Options
- Add/remove prefix/suffix (zone bit `0x0100` – `0x0115`, `0x0116` – `0x0132`)
- Set RAW, UTF-8, GBK, Unicode output (zone bit `0x000D`)
- Choose line endings: CR / LF / CRLF / TAB / None (zone bit `0x000E`)
- Add Code ID (Appendix D)
- Add AIM ID (Appendix C)
- Enable/disable HID keyboard, USB serial, virtual keyboard

---

## 🔎 AIM ID & Code ID Summary
- **AIM ID:** 3-character prefix (e.g., `]C0` = Code 128)
- **Code ID:** Single character ID, customizable per symbology

Refer to Appendix C and D for full lists.

---

## 📘 Example (ESP32 Arduino)
```cpp
#define RXD2 20
#define TXD2 19

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(500);
  byte trigger[] = {0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD};
  Serial2.write(trigger, sizeof(trigger));
}

void loop() {
  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}
```

---

## 📚 References
- **GM812-S / GM812-L Barcode Module Manual**
- **Version:** V1.2.1 (March 2024)
- **Manufacturer:** Hangzhou Grow Technology Co., Ltd.

---

Want QR code setup sheets or a filtered summary of only *useful developer commands*? Let me know!

---

## 🧩 Additional Features & Edge Settings (Advanced)

These features are less commonly used but critical in edge cases, diagnostics, or production environments.

---

### ⏱️ Serial Port Heartbeat (Link Health)

To periodically confirm that the scanner is still responsive:

**Recommended Use:** Send every 10 seconds. If no reply after 3 attempts, reset the scanner or reinit comms.

- **Heartbeat Packet:**
  ```hex
  7E 00 0A 01 00 00 00 30 1A 03 00 00 01 00 33 31
  ```
- **Expected Response:**
  ```
  02 00 00 01 00 33 31
  ```

---

### 🔃 Setup Code Enable/Disable (for QR Config)

You can enable/disable the ability to configure the scanner via QR codes:

| Action | Command |
|--------|---------|
| Enable QR Config | `_SET_CODE_ON` |
| Disable QR Config | `_SET_CODE_OFF` |
| Save Setting | `7E 00 09 01 00 00 00 AB CD` |

---

### 🔁 User Default Configuration Save/Restore

Allows saving the current scanner config as a custom default, which you can restore anytime.

| Action | Method |
|--------|--------|
| **Save Current Config as Default** | Scan “Save as user default” QR code |
| **Restore User Default Config**    | Scan “Restore user default” QR code |

> Tip: This is helpful if you want to quickly load a preset setup during production.

---

### 📛 Tail Character Output (End of Scan Marker)

Adds a **marker** (e.g., CR, LF, TAB) to the **end of each scan** for clean parsing.

- **Set Tail via zone bit `0x000E`** or use QR-based config.
- Examples:
  | Tail Type | Description |
  |-----------|-------------|
  | `CR`      | Carriage return (`\r`) |
  | `LF`      | Line feed (`\n`) |
  | `CRLF`    | Windows-style newline (`\r\n`) |
  | `TAB`     | Tab character |
  | `None`    | No terminator |

---

### ❌ Read-Fail Message (RF Output)

Scanner can send a custom message (e.g., `FAIL`) when a scan fails.

| Feature | Command |
|---------|---------|
| **Enable RF output** | Scan "Enable RF output" QR |
| **Change RF Message** | Enter up to 15 ASCII chars (in hex) |
| **Zone Bit Range** | `0x00F0–0x00FF` |

> Example: `"FAIL"` = `46 41 49 4C` in hex.

---

### 🧳 Virtual Keyboard Mode

Use when HID/USB keyboard emulation doesn’t work natively (e.g., on Android):

- **Modes:**
  - `Standard Keyboard` – Uses real keycodes
  - `Virtual Keyboard` – Simulates ASCII by Alt/Control combos
- **Control Char Output (< 0x20):**
  - `Ctrl Mode`
  - `Alt Mode` *(default)*

> Useful when physical keyboard layouts are inconsistent or platform lacks HID drivers.

---

### 🧬 Symbology Fine-Tuning (Optional)

For **production barcodes**, you can fine-tune:

| Symbology | Customizations |
|-----------|----------------|
| Code 39 / 128 / 93 / Codabar | Min length, checksum on/off, ASCII mode |
| EAN / UPC | Extra-code, transfer check char, output mode |
| ITF / MSI / Industrial 2-of-5 | Checksum (MOD-10/16), odd-length handling |
| QR / DM / PDF417 | Enable/disable, AIM ID, Code ID |

To explore these, reference:
- **Zone bit `0x0030–0x005F`** for enable/disable
- **Zone bit `0x0060–0x009F`** for min/max lengths

---