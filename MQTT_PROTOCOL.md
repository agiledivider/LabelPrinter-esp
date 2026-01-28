# MQTT Protocol Documentation

This document describes all MQTT messages published by the LabelPrinter-ESP device.

## Topics Overview

| Topic Pattern | Direction | Description |
|---------------|-----------|-------------|
| `{deviceName}/status` | OUT | Periodic status updates, connection events |
| `{deviceName}/result` | OUT | Print job results |
| `{deviceName}/print` | IN | Print job requests |
| `labelprinter/{deviceName}/cmd` | IN | Commands (log control) |
| `labelprinter/{deviceName}/log` | OUT | Real-time log entries, log dumps |

> **Note:** `{deviceName}` is configured during device setup (e.g., "labelprinter")

---

## Outbound Messages (Device → Client)

### 1. Periodic Status Message

**Topic:** `{deviceName}/status`
**Frequency:** Every 30 seconds
**Trigger:** Timer in main loop

```json
{
  "printer": "connected" | "disconnected",
  "battery": 0-100,
  "lastSeen": 123,
  "wifi": -67,
  "heap": 123456,
  "uptime": 3600
}
```

| Field | Type | Description |
|-------|------|-------------|
| `printer` | string | Printer connection state: `"connected"` or `"disconnected"` |
| `battery` | number | Battery percentage (0-100), omitted if unknown (-1) |
| `lastSeen` | number | Seconds since last printer activity, omitted if 0 |
| `wifi` | number | WiFi RSSI in dBm (e.g., -67) |
| `heap` | number | Free heap memory in bytes |
| `uptime` | number | Device uptime in seconds |

---

### 2. Printer Connection Event (F006)

**Topic:** `{deviceName}/status`
**Trigger:** Printer connects or disconnects

```json
{
  "event": "printer_connected" | "printer_disconnected",
  "timestamp": 123456
}
```

| Field | Type | Description |
|-------|------|-------------|
| `event` | string | Event type: `"printer_connected"` or `"printer_disconnected"` |
| `timestamp` | number | `millis()` value when event occurred |

> **Distinction:** Messages with `event` field are connection events. Messages with `printer` field are periodic status updates.

---

### 3. Print Job Result

**Topic:** `{deviceName}/result`
**Trigger:** After processing a print request

#### Success Response

```json
{
  "printId": "abc123",
  "success": true
}
```

#### Error Response

```json
{
  "printId": "abc123",
  "success": false,
  "error": "printer not connected"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `printId` | string | Echo of the print request ID (optional, omitted if not provided) |
| `success` | boolean | `true` if print succeeded, `false` otherwise |
| `error` | string | Error message (only present when `success: false`) |

#### Possible Error Values

| Error | Description |
|-------|-------------|
| `"printer not connected"` | Bluetooth connection lost |
| `"printer not ready"` | Printer not ready for printing |
| `"no paper"` | Paper roll empty |
| `"out of memory"` | ESP32 memory allocation failed |
| `"missing link"` | Required `link` field missing in request |
| `"QR code generation failed"` | QR code library error |
| `"empty QR data"` | Empty string passed to QR generator |
| `"QR data too long"` | Data exceeds QR code capacity |
| `"no QR code generated"` | QR code not generated before render |
| `"invalid bitmap"` | Bitmap data corruption |
| `"invalid JSON"` | Request JSON parse error |

---

### 4. Real-time Log Entry

**Topic:** `labelprinter/{deviceName}/log`
**Trigger:** Each log message (when MQTT logging enabled)

```json
{
  "ts": 123456,
  "level": "INFO",
  "component": "Printer",
  "msg": "Printer connected!"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `ts` | number | Timestamp in milliseconds (`millis()`) |
| `level` | string | Log level: `"DEBUG"`, `"INFO"`, `"WARN"`, `"ERROR"` |
| `component` | string | Source component (see table below) |
| `msg` | string | Log message text (max 127 chars) |

#### Log Components

| Component | Description |
|-----------|-------------|
| `"System"` | General system messages |
| `"WiFi"` | WiFi connection events |
| `"MQTT"` | MQTT connection and message events |
| `"Printer"` | Bluetooth printer events |
| `"Config"` | Configuration loading/saving |
| `"Label"` | Label generation |
| `"Portal"` | Configuration portal |

---

### 5. Log Dump Response

**Topic:** `labelprinter/{deviceName}/log`
**Trigger:** Response to `getLogs` command

```json
[
  {"ts": 1000, "level": "INFO", "component": "System", "msg": "Boot complete"},
  {"ts": 2000, "level": "DEBUG", "component": "WiFi", "msg": "Connecting..."},
  {"ts": 3000, "level": "INFO", "component": "WiFi", "msg": "Connected"}
]
```

Array of log entries (up to 32 entries from circular buffer).

---

## Inbound Messages (Client → Device)

### 1. Print Request

**Topic:** `{deviceName}/print`

```json
{
  "printId": "abc123",
  "link": "https://example.com/item/123",
  "name": "Item Name",
  "id": "123",
  "size": "L"
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `printId` | string | No | Job ID echoed in result |
| `link` | string | Yes | URL for QR code |
| `name` | string | Yes | Item name (top line) |
| `id` | string | Yes | Item ID (bottom line) |
| `size` | string | No | QR size: `"S"`, `"M"`, `"L"` (default: `"L"`) |

---

### 2. Log Command

**Topic:** `labelprinter/{deviceName}/cmd`

#### Set Log Level

```json
{
  "cmd": "log",
  "action": "setLevel",
  "level": "DEBUG"
}
```

| Level | Description |
|-------|-------------|
| `"DEBUG"` | All messages |
| `"INFO"` | Info, warnings, errors |
| `"WARN"` | Warnings and errors only |
| `"ERROR"` | Errors only |
| `"NONE"` | Logging disabled |

#### Get Current Log Level

```json
{
  "cmd": "log",
  "action": "getLevel"
}
```

Response appears in serial output and real-time log stream.

#### Get Recent Logs

```json
{
  "cmd": "log",
  "action": "getLogs"
}
```

Response: JSON array published to log topic (see Log Dump Response above).

#### Save Logs to Flash

```json
{
  "cmd": "log",
  "action": "saveLogs"
}
```

Saves circular buffer to LittleFS for crash recovery.

#### Clear Saved Logs

```json
{
  "cmd": "log",
  "action": "clearLogs"
}
```

Clears persistent log storage.

---

## Message Type Detection

To distinguish message types on the status topic:

```typescript
interface StatusMessage {
  printer: "connected" | "disconnected";
  battery?: number;
  lastSeen?: number;
  wifi: number;
  heap: number;
  uptime: number;
}

interface ConnectionEvent {
  event: "printer_connected" | "printer_disconnected";
  timestamp: number;
}

type StatusTopicMessage = StatusMessage | ConnectionEvent;

function isConnectionEvent(msg: StatusTopicMessage): msg is ConnectionEvent {
  return "event" in msg;
}

function isStatusMessage(msg: StatusTopicMessage): msg is StatusMessage {
  return "printer" in msg && !("event" in msg);
}
```

---

## Example: Monitoring Printer State

```typescript
import mqtt from "mqtt";

const client = mqtt.connect("mqtt://broker.example.com");
const deviceName = "labelprinter";

client.subscribe(`${deviceName}/status`);
client.subscribe(`${deviceName}/result`);

client.on("message", (topic, payload) => {
  const msg = JSON.parse(payload.toString());

  if (topic.endsWith("/status")) {
    if ("event" in msg) {
      // Connection event
      console.log(`Printer ${msg.event} at ${msg.timestamp}ms`);
    } else {
      // Periodic status
      console.log(`Status: ${msg.printer}, Battery: ${msg.battery}%`);
    }
  } else if (topic.endsWith("/result")) {
    if (msg.success) {
      console.log(`Print ${msg.printId} succeeded`);
    } else {
      console.log(`Print ${msg.printId} failed: ${msg.error}`);
    }
  }
});
```

---

## Configuration Reference

Default topic configuration (auto-generated from device name):

| Setting | Default Pattern | Example |
|---------|-----------------|---------|
| Print Topic | `{deviceName}/print` | `labelprinter/print` |
| Status Topic | `{deviceName}/status` | `labelprinter/status` |
| Result Topic | `{deviceName}/result` | `labelprinter/result` |
| Command Topic | `labelprinter/{deviceName}/cmd` | `labelprinter/labelprinter/cmd` |
| Log Topic | `labelprinter/{deviceName}/log` | `labelprinter/labelprinter/log` |

Topics can be customized in the configuration portal.
