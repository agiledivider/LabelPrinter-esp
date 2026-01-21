#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <cstring>

/**
 * Safely copy a string with guaranteed null-termination.
 * @param dest Destination buffer
 * @param src Source string
 * @param destSize Size of destination buffer
 */
inline void safeCopy(char* dest, const char* src, size_t destSize) {
    if (destSize == 0) return;
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

#endif // STRING_UTILS_H
