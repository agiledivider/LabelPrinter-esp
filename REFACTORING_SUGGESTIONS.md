# Refactoring Suggestions for LabelPrinter-esp

## Completed

### 1. Extract Command Handler Pattern (main.cpp) ✅
Replaced 45-line if-else chain with table-driven command dispatch.
- Added `Command` struct and `CommandHandler` typedef
- Created command lookup table with handler functions
- Commit: `12ecfcc`

### 2. Standardize Language ✅
Changed all German strings to English across codebase.
- Updated main.cpp, NelkoP21Printer.cpp, WiFiManager.cpp, MqttManager.cpp
- Commit: `12ecfcc`

### 3. Extract Magic Numbers to Constants (LabelImage) ✅
Created `LabelLayout` namespace with named constants.
- Font: `GLYPH_WIDTH`, `GLYPH_HEIGHT`, `CHAR_SPACING`, `CHAR_WIDTH`
- Text: `LINE_HEIGHT`, `MAX_TEXT_LINES`, `ID_SCALE`, `ID_HEIGHT`
- Margins: `QR_TOP_MARGIN`, `QR_BOTTOM_MARGIN`, `ID_BOTTOM_MARGIN`, `LOGO_TOP_MARGIN`
- Commit: `792299b`

### 4. Consolidate String Copying ✅
Created `StringUtils.h` with `safeCopy()` helper function.
- Replaced 21 `strncpy` + null-termination patterns across 4 files
- Updated: MqttManager.cpp, WiFiManager.cpp, ConfigManager.cpp, ConfigPortal.cpp

### 5. Extract Protocol Constants ✅
Created `TSPL2.h` namespace with protocol constants.
- Query commands: `BATTERY`, `CONFIG`, `STATUS` with echo lengths
- Status codes: `OK`, `NO_PAPER`
- Response sizes, timeouts, config byte indices
- Bitmap header format and print command

### 6. Create Logging Abstraction ✅
Created `Log.h` with level-based logging macros.
- `LOG_ERROR` / `LOG_ERRORF` - Error messages (level >= 1)
- `LOG_INFO` / `LOG_INFOF` - Info messages (level >= 2)
- `LOG_DEBUG` / `LOG_DEBUGF` - Debug messages (level >= 3)
- `LOG_RAW` / `LOG_RAWF` / `LOG_HEX` - Raw output (always enabled)
- Replaced 152 `Serial.print*` calls across 6 files

### 7. Add Error Enum ✅
Created `PrintError.h` with strongly-typed error codes.
- `PrintError` enum class with 11 error types
- `printErrorToString()` helper for human-readable messages
- Updated: Printer.h, NelkoP21Printer, QRCodeRenderer, LabelImage, main.cpp
- Replaced all string-based error passing with type-safe enum

### 8. Move printFrame() to LabelImage ✅
Added `generateFrame()` method to LabelImage class.
- Moved low-level bitmap manipulation from main.cpp to LabelImage
- Simplified printFrame() to use LabelImage::generateFrame()
- Better encapsulation of image generation logic

### 9. Extract JSON Building ✅
Created `JsonHelpers.h` with helper functions for MQTT messages.
- `buildResult()` - Print result JSON (printId, success, error)
- `buildErrorResult()` - Error-only result for non-PrintError cases
- `buildStatus()` - Status JSON (printer, battery, wifi, heap, uptime)
- Simplified sendResult(), mqttCallback(), and publishStatus() in main.cpp

### 10. Extract HTML to PROGMEM ✅
Created `ConfigPortalHtml.h` with static HTML content stored in flash.
- Split config page into 12 PROGMEM string segments for value interpolation
- Moved success page to single PROGMEM string
- Updated `getConfigPage()` to concatenate FPSTR() segments with config values
- Updated `getSuccessPage()` to return FPSTR() directly
- Saves ~4KB RAM by keeping HTML in flash instead of heap

---

## Remaining

### 1. Remove Singleton Anti-pattern (NelkoP21Printer.cpp:4,83-84)
The static instance pointer for Bluetooth callback is a singleton anti-pattern.

```cpp
// Current: Static instance pointer
static NelkoP21Printer* _instance = nullptr;
static void btCallback(...) { if (_instance) ... }

// Suggested: Lambda capture or member callback wrapper
```

### 2. Consider Smart Pointers (main.cpp:21, LabelImage.cpp)
Raw pointers with manual memory management.

```cpp
// Current
Printer* printer = nullptr;
uint8_t* _bitmap = (uint8_t*)malloc(_bitmapSize);

// Suggested (where available)
std::unique_ptr<Printer> printer;
std::unique_ptr<uint8_t[]> _bitmap;
```

---

## Priority Order (Suggested)
1. #1 - Remove singleton (architectural improvement)
2. #2 - Smart pointers (modern C++, but may have ESP32 limitations)
