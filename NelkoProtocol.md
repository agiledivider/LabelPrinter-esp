# Nelko P21 Protocol Documentation

Reverse-engineered protocol information for the Nelko P21 thermal label printer.

## Connection

- **Type:** Bluetooth Classic SPP/RFCOMM (Serial Port Profile) or BLE GATT
- **Device Name:** Contains "P21" or "Nelko"
- **Pairing:** No PIN required
- **Chip:** JieLi AC6951C Bluetooth chip
- **USB:** Disabled - only responds with `ERROR0`

### BLE GATT (used by official app)

| Handle | Description |
|--------|-------------|
| 0x0006 | Write commands |
| 0x0008 | Receive notifications (responses) |

### Bluetooth Classic SPP

Standard serial connection. May require restart if scan fails after idle.

## Label Specifications

| Property | Value |
|----------|-------|
| Width | 96 pixels (14.0 mm) |
| Height | 284 pixels (40.0 mm) |
| Gap | 5.0 mm |
| Resolution | 203 DPI (0xCB) |
| Color | 1-bit monochrome (thermal) |

## Command Format

All commands require **CRLF** (`\r\n`) line termination. Responses are also CRLF terminated.

## Status & Query Commands

**Note:** All query commands echo the command back before the response data.

| Command | Description | Response |
|---------|-------------|----------|
| `BATTERY?\r\n` | Query battery level | Echo + 2 bytes (first = BCD percentage) |
| `CONFIG?\r\n` | Query device config | Echo + 10 bytes (see below) |
| `\x1B!o\r\n` | Query printer status | 16 bytes (paper status, dimensions) |

### BATTERY Response

- Format: Echo "BATTERY?" + 2 bytes
- First byte: Battery percentage in **BCD format**
  - 0x99 = 99%
  - 0x66 = 66%
  - 0x42 = 42%
- Second byte: Unknown (possibly charging status)

### CONFIG Response (10 bytes)

Example: `00 CB 00 02 02 04 02 0A 02 01`

| Byte | Example | Description |
|------|---------|-------------|
| 0 | 0x00 | Protocol type (0 = TSPL2) |
| 1 | 0xCB | DPI resolution (0xCB = 203) |
| 2-4 | 0x00 0x02 0x02 | Hardware version (v0.2.2) |
| 5-7 | 0x04 0x02 0x0A | Firmware version (v4.2.10) |
| 8 | 0x02 | Auto-off timeout (0=never, 1=15min, 2=30min, 3=60min) |
| 9 | 0x01 | Beep (0=off, 1=on) |

## Print Protocol (TSPL2)

The printer uses a subset of TSPL2 (TSC Printer Language) commands.

### Basic Print Sequence

```
SIZE 14.0 mm,40.0 mm\r\n
GAP 5.0 mm,0 mm\r\n
DIRECTION 0,0\r\n
DENSITY 15\r\n
CLS\r\n
BITMAP 0,0,12,284,1,<binary data>
PRINT 1\r\n
```

### TSPL2 Command Reference

| Command | Description |
|---------|-------------|
| `SIZE w,h` | Label size in mm |
| `GAP g,o` | Gap between labels (g=gap, o=offset) |
| `DIRECTION d,m` | Print direction (0=normal) |
| `DENSITY n` | Print darkness (0-15, 15=darkest) |
| `CLS` | Clear image buffer |
| `BITMAP x,y,w,h,m,data` | Draw bitmap at x,y with width in bytes, height in pixels, mode, and binary data |
| `PRINT n` | Print n copies |
| `BAR` | Print solid black label |
| `SELFTEST` | Trigger test print |
| `INITIALPRINTER` | Factory reset |
| `BEEP n` | Beep control (n=0x00 or 0x01) |

### Bitmap Format

- **Bytes per row:** 12 (96 pixels / 8)
- **Total size:** 3408 bytes (12 × 284)
- **Bit order:** MSB first (bit 7 = leftmost pixel)
- **Color:** 0 = black (printed), 1 = white (not printed)
- **Orientation:** Top-to-bottom, left-to-right
- **Error correction:** None (no checksums)

## Confirmed Working Commands

| Command | Status |
|---------|--------|
| `BATTERY?` | ✓ Working (BCD response) |
| `CONFIG?` | ✓ Working (10 bytes) |
| `ESC!o` | ✓ Working (16 bytes status response) |
| TSPL2 print commands | ✓ Working |

### ESC!o Status Response (16 bytes)

Command: `0x1B 0x21 0x6F 0x0D 0x0A` (ESC!o\r\n)

**Example responses:**
- Paper OK: `00 0C 01 12 03 00 03 01 12 12 15 28 0F 0E ED 03`
- Paper Error: `04 0C 00 00 00 00 00 00 00 00 00 00 00 00 09 BF`

| Byte | Paper OK | Paper Error | Description |
|------|----------|-------------|-------------|
| 0 | 0x00 | 0x04 | **Status code** (0=OK, 4=paper error) |
| 1 | 0x0C | 0x0C | Packet type (constant) |
| 2-10 | varies | 0x00 | Unknown |
| 11 | 0x28 (40) | 0x00 | Label height in mm |
| 12 | 0x0F (15) | 0x00 | Unknown |
| 13 | 0x0E (14) | 0x00 | Label width in mm |
| 14-15 | varies | 0x09 0xBF | Checksum / flags |

**Status codes (Byte 0):**
- `0x00` = Ready / Paper OK
- `0x04` = Paper error / No paper

**Paper dimensions (when status OK):**
- Byte 11: Label height in mm (e.g., 0x28 = 40mm)
- Byte 13: Label width in mm (e.g., 0x0E = 14mm)

## Commands That Don't Work

These standard TSPL2/ESC-POS commands were tested but received no response:

- `STATUS`, `INFO`, `VERSION` (TSPL2)
- `\x1B\x76`, `\x10\x04\x01`, `\x10\x04\x02` (ESC/POS)
- `!B`, `!S`, `!P`, `!V`, `!I` (proprietary attempts)
- `\x1B!?` (ESC!? - ready status, needs more testing)

## Limitations

1. **No print confirmation** - No acknowledgment after successful print
2. **No detailed error codes** - Only basic paper present/absent detection

## Hardware Notes

- Thermal printer (no ink required)
- Labels are heat-sensitive
- Print quality affected by DENSITY setting
- QR codes should use minimum version with maximum scaling for best readability

## ESP32 Implementation Notes

- **Bluetooth Classic:** Use `BluetoothSerial` library (SPP)
- **BLE alternative:** Use NimBLE with GATT handles 0x0006 (write) / 0x0008 (notify)
- **Idle issue:** Bluetooth scan may fail after idle period - restart with `SerialBT.end()` / `SerialBT.begin()`
- **Memory:** SSL for MQTT may cause issues due to limited RAM
- **Partition:** Recommended `huge_app.csv` (3MB app space)

## Firmware Updates

The official app checks for updates via:
```
POST http://app.nelko.net/api/firmware/verify
```
With device metadata including hardware version and current firmware.

## References

- https://github.com/merlinschumacher/nelko-p21-print - Python implementation with protocol details
- TSPL2 Programming Manual (TSC printers)
- Similar protocol used by other budget thermal label printers (e.g., Phomemo, Niimbot)
