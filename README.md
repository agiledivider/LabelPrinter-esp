# Nelko P21 MQTT Label Printer

ESP32-based controller for the Nelko P21 thermal printer via Bluetooth SPP with MQTT integration.

## Features

- **Bluetooth SPP** connection to Nelko P21 printer
- **MQTT** remote print commands with JSON payload
- **WiFi** connectivity with automatic reconnection
- **Captive Portal** for initial device configuration
- **QR Code** generation with configurable sizes (S/M/L)
- **Status reporting** via MQTT (battery, connection, heap)

## Hardware

- ESP32 DevKit (or compatible)
- Nelko P21 Thermal Printer

## Printer Specifications

- **Protocol**: TSPL2 (TSC Printer Language)
- **Connection**: Bluetooth SPP (Serial Port Profile)
- **Label Size**: 96 x 284 pixels
- **Format**: 1-bit bitmap

## Quick Start

1. Flash the firmware:
   ```bash
   pio run -t upload
   ```

2. On first boot, connect to WiFi network `LabelPrinter-Setup`

3. Open `http://192.168.4.1` to configure:
   - WiFi credentials
   - MQTT broker settings
   - Device name and topics

4. Device restarts and connects to your network

## MQTT Interface

### Print Command

**Topic**: `{deviceName}/print` (configurable)

```json
{
  "printId": "unique-id",
  "link": "https://example.com/item/123",
  "name": "Item Name",
  "id": "123",
  "size": "L"
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `printId` | No | Correlation ID for result tracking |
| `link` | Yes | URL encoded in QR code |
| `name` | Yes | Text displayed on label |
| `id` | Yes | ID displayed on label |
| `size` | No | QR size: `S`, `M`, `L` (default) |

### Status Updates

**Topic**: `{deviceName}/status` (every 30s)

```json
{
  "printer": "connected",
  "battery": 85,
  "lastSeen": 5,
  "wifi": -45,
  "heap": 180000,
  "uptime": 3600
}
```

### Print Results

**Topic**: `{deviceName}/result`

```json
{
  "printId": "unique-id",
  "success": true
}
```

Or on error:
```json
{
  "printId": "unique-id",
  "success": false,
  "error": "no paper"
}
```

## Serial Commands

Connect via serial monitor at 115200 baud:

| Command | Description |
|---------|-------------|
| `scan` | Scan and connect to printer |
| `disconnect` | Disconnect from printer |
| `status` | Show printer status |
| `config` | Query printer configuration |
| `battery` | Query battery level |
| `frame` | Print test frame |
| `qrcode` | Print test QR label |
| `wifi` | Reconnect WiFi |
| `mqtt` | Reconnect MQTT |
| `setup` | Start configuration portal |
| `clearconfig` | Clear config and restart |
| `help` | Show available commands |

## Configuration

Configuration is stored in NVS (Non-Volatile Storage) and persists across reboots.

To reconfigure:
- Run `setup` command, or
- Run `clearconfig` to reset and restart

## Project Structure

```
src/
├── main.cpp           # Application entry point
├── ConfigManager.*    # NVS configuration storage
├── ConfigPortal.*     # Captive portal web server
├── WiFiManager.*      # WiFi connection management
├── MqttManager.*      # MQTT client wrapper
├── Printer.h          # Printer interface
├── NelkoP21Printer.*  # Nelko P21 implementation
├── LabelImage.*       # Bitmap generation
├── QRCodeRenderer.*   # QR code rendering
├── PrintError.h       # Error type definitions
├── JsonHelpers.h      # MQTT JSON builders
├── Log.h              # Logging macros
├── StringUtils.h      # String utilities
└── TSPL2.h            # Printer protocol constants
```

## Building

Requires PlatformIO:

```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor
pio device monitor
```

## License

MIT
