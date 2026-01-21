#ifndef TSPL2_H
#define TSPL2_H

/**
 * TSPL2 Protocol Constants
 *
 * TSPL2 (TSC Printer Language 2) is a command language for thermal printers.
 * This header defines constants for the subset used by Nelko P21 printers.
 */
namespace TSPL2 {

// Query Commands
namespace Query {
    constexpr const char* BATTERY = "BATTERY?\r\n";
    constexpr const char* CONFIG = "CONFIG?\r\n";
    constexpr const char* STATUS = "\x1B!o\r\n";  // ESC ! o

    // Echo lengths (chars to skip after sending command)
    constexpr int BATTERY_ECHO_LEN = 8;  // "BATTERY?"
    constexpr int CONFIG_ECHO_LEN = 7;   // "CONFIG?"
}

// Status Codes (returned in first byte of status response)
namespace Status {
    constexpr uint8_t OK = 0x00;
    constexpr uint8_t NO_PAPER = 0x04;
}

// Response Sizes
namespace Response {
    constexpr int CONFIG_SIZE = 10;
    constexpr int STATUS_SIZE = 16;
}

// Bitmap Command Format
// Usage: snprintf(buf, size, BITMAP_HEADER_FMT, width_mm, height_mm, gap_mm, bytes_per_row, height_px)
constexpr const char* BITMAP_HEADER_FMT =
    "SIZE %.1f mm,%.1f mm\r\n"
    "GAP %.1f mm,0 mm\r\n"
    "DIRECTION 0,0\r\n"
    "DENSITY 15\r\n"
    "CLS\r\n"
    "BITMAP 0,0,%d,%d,1,";

constexpr const char* PRINT_COMMAND = "\r\nPRINT 1\r\n";

// Timeouts (milliseconds)
namespace Timeout {
    constexpr unsigned long QUERY = 500;
    constexpr unsigned long STATUS = 1000;
    constexpr unsigned long COMMAND = 1000;
    constexpr unsigned long BATTERY_RESPONSE = 300;
}

// Config Response Byte Indices
namespace ConfigIndex {
    constexpr int PROTOCOL = 0;
    constexpr int DPI = 1;
    constexpr int HW_MAJOR = 2;
    constexpr int HW_MINOR = 3;
    constexpr int HW_PATCH = 4;
    constexpr int FW_MAJOR = 5;
    constexpr int FW_MINOR = 6;
    constexpr int FW_PATCH = 7;
    constexpr int AUTO_OFF = 8;
    constexpr int BEEP = 9;
}

}  // namespace TSPL2

#endif // TSPL2_H
