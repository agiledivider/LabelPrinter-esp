#ifndef LOG_H
#define LOG_H

#include <Arduino.h>

/**
 * Logging Abstraction
 *
 * Provides consistent logging interface with level prefixes.
 * Define LOG_LEVEL before including to control verbosity:
 *   0 = OFF (no logging)
 *   1 = ERROR only
 *   2 = ERROR + INFO
 *   3 = ERROR + INFO + DEBUG (default)
 */

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

// Error messages (always shown unless LOG_LEVEL=0)
#if LOG_LEVEL >= 1
    #define LOG_ERROR(msg) Serial.printf("[ERROR] %s\n", msg)
    #define LOG_ERRORF(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_ERROR(msg)
    #define LOG_ERRORF(fmt, ...)
#endif

// Info messages (shown at LOG_LEVEL >= 2)
#if LOG_LEVEL >= 2
    #define LOG_INFO(msg) Serial.println(msg)
    #define LOG_INFOF(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_INFO(msg)
    #define LOG_INFOF(fmt, ...)
#endif

// Debug messages (shown at LOG_LEVEL >= 3)
#if LOG_LEVEL >= 3
    #define LOG_DEBUG(msg) Serial.printf("[DEBUG] %s\n", msg)
    #define LOG_DEBUGF(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_DEBUG(msg)
    #define LOG_DEBUGF(fmt, ...)
#endif

// Raw output (no prefix, no newline) - for progress indicators etc.
#define LOG_RAW(msg) Serial.print(msg)
#define LOG_RAWF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

// Hex dump helper
#define LOG_HEX(data, len) do { \
    for (int _i = 0; _i < (len); _i++) { \
        Serial.printf("%02X ", (data)[_i]); \
    } \
} while(0)

#endif // LOG_H
