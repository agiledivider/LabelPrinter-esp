# Backlog

## Planned Features

### Configuration Reset via MQTT/Web
- Add ability to force reset configuration data remotely
- Useful for troubleshooting or reprovisioning devices without serial access
- Options:
  - MQTT command to trigger factory reset
  - Web portal button to clear all settings
  - Confirmation mechanism to prevent accidental resets

### Auto-Start Config Portal on WiFi Failure
- Automatically start configuration portal if WiFi connection cannot be established
- Configurable timeout before triggering portal (e.g., 60-300 seconds)
- Useful for devices moved to new networks or credential changes
- Should persist timeout setting in NVS
- Consider: distinguish between "never connected" vs "lost connection" scenarios

### Multiple WiFi Configurations
- Store multiple SSID/password pairs for fallback connectivity
- Automatically try next network if primary fails
- Configurable priority order
- Useful for devices that move between locations (home/office/workshop)
- UI in config portal to add/remove/reorder networks
- Consider maximum number of stored networks (ESP32 NVS constraints)

### Password-Protected Configuration Access
- View current configuration anytime via web interface (not just in setup mode)
- Passwords and secrets displayed as masked/hidden (e.g., ••••••••)
- Require admin password to unlock editing
- Admin password set during initial configuration
- Read-only view available without password
- Options:
  - Accessible via device IP on local network
  - Session timeout for security
  - Password recovery mechanism (e.g., physical button hold)
