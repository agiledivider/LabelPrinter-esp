# Backlog

## Planned Features

### F001: Configuration Reset via MQTT/Web
- Add ability to force reset configuration data remotely
- Useful for troubleshooting or reprovisioning devices without serial access
- Options:
  - MQTT command to trigger factory reset
  - Web portal button to clear all settings
  - Confirmation mechanism to prevent accidental resets

### F002: Auto-Start Config Portal on WiFi Failure
- Automatically start configuration portal if WiFi connection cannot be established
- Configurable timeout before triggering portal (e.g., 60-300 seconds)
- Useful for devices moved to new networks or credential changes
- Should persist timeout setting in NVS
- Consider: distinguish between "never connected" vs "lost connection" scenarios

### F003: Multiple WiFi Configurations
- Store multiple SSID/password pairs for fallback connectivity
- Automatically try next network if primary fails
- Configurable priority order
- Useful for devices that move between locations (home/office/workshop)
- UI in config portal to add/remove/reorder networks
- Consider maximum number of stored networks (ESP32 NVS constraints)

### F004: Password-Protected Configuration Access
- View current configuration anytime via web interface (not just in setup mode)
- Passwords and secrets displayed as masked/hidden (e.g., ••••••••)
- Require admin password to unlock editing
- Admin password set during initial configuration
- Read-only view available without password
- Options:
  - Accessible via device IP on local network
  - Session timeout for security
  - Password recovery mechanism (e.g., physical button hold)

### F005: Hardware Button Configuration Reset ✅
- Use existing ESP32 buttons (BOOT/EN) to trigger configuration reset
- Hold button during boot for X seconds to clear config and start portal
- Visual feedback via LED or serial output during hold
- Provides recovery option when network/serial access unavailable
- Consider: different hold durations for different actions (reset vs portal only)

### F006: Bluetooth Printer Auto-Reconnect
- Automatically detect when printer disconnects (power off, out of range, sleep)
- Periodic reconnection attempts with configurable interval
- Exponential backoff to avoid excessive scanning
- Status reporting via MQTT when connection state changes
- Queue print jobs while disconnected, execute on reconnect
- Options:
  - Max reconnect attempts before giving up
  - Reconnect interval (e.g., 10-60 seconds)
  - MQTT notification on disconnect/reconnect events
- Consider: battery drain from continuous BT scanning

### F007: Enhanced Logging System
- Configurable log levels (DEBUG, INFO, WARN, ERROR)
- Multiple log outputs: Serial, MQTT, web interface
- Persistent log buffer in SPIFFS/LittleFS for post-mortem analysis
- Timestamped entries with uptime or RTC time
- Remote log level adjustment via MQTT command
- Options:
  - Log rotation with configurable max size
  - Filter logs by component (WiFi, MQTT, Printer, etc.)
  - Stream logs to remote syslog server
- Consider: memory constraints on ESP32 for log buffering

### F008: Encrypted MQTT Messages
- End-to-end encryption of MQTT payloads (independent of TLS transport)
- Symmetric encryption using AES-256-GCM or ChaCha20-Poly1305
- Pre-shared key configured via config portal
- Message authentication to prevent tampering
- Options:
  - Key rotation mechanism
  - Nonce/IV management to prevent replay attacks
  - Fallback to unencrypted mode if key not configured
  - Encrypt only sensitive fields vs entire payload
- Consider: ESP32 hardware crypto acceleration for performance
