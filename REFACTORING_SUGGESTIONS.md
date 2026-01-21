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

---

## Remaining

### 4. Remove Singleton Anti-pattern (NelkoP21Printer.cpp:4,83-84)
The static instance pointer for Bluetooth callback is a singleton anti-pattern.

```cpp
// Current: Static instance pointer
static NelkoP21Printer* _instance = nullptr;
static void btCallback(...) { if (_instance) ... }

// Suggested: Lambda capture or member callback wrapper
```

### 5. Extract JSON Building (main.cpp:96-112, 139-159)
JSON construction is repeated and inline.

```cpp
// Suggested: Create StatusBuilder class
class StatusBuilder {
public:
    StatusBuilder& printer(bool connected);
    StatusBuilder& battery(int level);
    StatusBuilder& wifi(int rssi);
    String build();
};
```

### 6. Move printFrame() to LabelImage (main.cpp:65-90)
Low-level bitmap manipulation belongs in the image class.

```cpp
// Suggested: LabelImage::generateFrame()
LabelImage label(width, height);
label.generateFrame();
printer->sendBitmap(label.getData());
```

### 7. Extract HTML to PROGMEM (ConfigPortal.cpp:147-262)
Large HTML string uses RAM instead of flash.

```cpp
// Suggested: Use PROGMEM for static content
const char CONFIG_PAGE_HEADER[] PROGMEM = R"rawliteral(...)rawliteral";
```

### 8. Consider Smart Pointers (main.cpp:21, LabelImage.cpp)
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
1. #6 - Move printFrame to LabelImage (better encapsulation)
2. #5 - Extract JSON building (reduces duplication)
3. #7 - Extract HTML to PROGMEM (saves RAM)
4. #4 - Remove singleton (architectural improvement)
5. #8 - Smart pointers (modern C++, but may have ESP32 limitations)
