# Nelko P21 Protocol Documentation

Reverse-engineered protocol information for the Nelko P21 thermal label printer.

## Connection

- **Type:** Bluetooth Classic SPP/RFCOMM (Serial Port Profile)
- **BLE:** Not supported - printer does not respond to BLE connections
- **Device Name:** Contains "P21" or "Nelko"
- **Pairing:** No PIN required
- **Chip:** JieLi AC6951C Bluetooth chip
- **USB:** Disabled - only responds with `ERROR0`

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
| `\x1B!?\r\n` | Printer ready status | Status byte |
| `\x1B!o\r\n` | Cancel pause status | - |

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
| TSPL2 print commands | ✓ Working |

## Commands That Don't Work

These standard TSPL2/ESC-POS commands were tested but received no response:

- `STATUS`, `INFO`, `VERSION` (TSPL2)
- `\x1B\x76`, `\x10\x04\x01`, `\x10\x04\x02` (ESC/POS)
- `!B`, `!S`, `!P`, `!V`, `!I` (proprietary attempts)
- `\x1B!?` (ESC!? - ready status, needs more testing)

## Limitations

1. **No paper/error detection** - Printer doesn't report out-of-paper or jam errors
2. **No print confirmation** - No acknowledgment after successful print
3. **Limited feedback** - Only battery and config queries confirmed working

## Hardware Notes

- Thermal printer (no ink required)
- Labels are heat-sensitive
- Print quality affected by DENSITY setting
- QR codes should use minimum version with maximum scaling for best readability

## ESP32 Implementation Notes

- Use Bluetooth Classic with `BluetoothSerial` library
- BLE (NimBLE) does not work with this printer
- SSL for MQTT may cause memory issues due to limited RAM
- Recommended partition: `huge_app.csv` (3MB app space)

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
