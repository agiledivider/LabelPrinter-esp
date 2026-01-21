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

### 8. Add Error Enum (Multiple files)
String-based error passing is error-prone.

```cpp
enum class PrintError {
    None,
    PrinterNotConnected,
    PrinterNotReady,
    NoPaper,
    OutOfMemory,
    QRCodeFailed
};
```

### 9. Create Logging Abstraction (All files)
Direct `Serial.println()` scattered everywhere.

```cpp
#define LOG_INFO(msg) Serial.println(msg)
#define LOG_ERROR(msg) Serial.printf("[ERROR] %s\n", msg)
#define LOG_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
```

### 10. Extract Protocol Constants (NelkoP21Printer.cpp)
TSPL2 commands as magic strings.

```cpp
namespace TSPL2 {
    const char* QUERY_BATTERY = "BATTERY?\r\n";
    const char* QUERY_STATUS = "\x1B!o\r\n";
    const char* QUERY_CONFIG = "CONFIG?\r\n";
}
```

### 11. Consider Smart Pointers (main.cpp:21, LabelImage.cpp)
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
1. #10 - Extract protocol constants (improves readability)
2. #9 - Create logging abstraction (enables future log levels)
3. #8 - Add error enum (type safety)
4. #6 - Move printFrame to LabelImage (better encapsulation)
5. #5 - Extract JSON building (reduces duplication)
6. #7 - Extract HTML to PROGMEM (saves RAM)
7. #4 - Remove singleton (architectural improvement)
8. #11 - Smart pointers (modern C++, but may have ESP32 limitations)
