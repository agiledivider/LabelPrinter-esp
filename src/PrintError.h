#ifndef PRINT_ERROR_H
#define PRINT_ERROR_H

/**
 * Strongly-typed error codes for print operations.
 * Replaces string-based error passing throughout the codebase.
 */
enum class PrintError {
    None = 0,

    // Printer errors
    PrinterNotConnected,
    PrinterNotReady,
    NoPaper,

    // Label generation errors
    OutOfMemory,
    MissingLink,

    // QR code errors
    QRCodeFailed,
    QRDataEmpty,
    QRDataTooLong,
    QRNotGenerated,
    InvalidBitmap
};

/**
 * Convert PrintError to human-readable string.
 * Returns nullptr for PrintError::None.
 */
inline const char* printErrorToString(PrintError error) {
    switch (error) {
        case PrintError::None:                return nullptr;
        case PrintError::PrinterNotConnected: return "printer not connected";
        case PrintError::PrinterNotReady:     return "printer not ready";
        case PrintError::NoPaper:             return "no paper";
        case PrintError::OutOfMemory:         return "out of memory";
        case PrintError::MissingLink:         return "missing link";
        case PrintError::QRCodeFailed:        return "QR code generation failed";
        case PrintError::QRDataEmpty:         return "empty QR data";
        case PrintError::QRDataTooLong:       return "QR data too long";
        case PrintError::QRNotGenerated:      return "no QR code generated";
        case PrintError::InvalidBitmap:       return "invalid bitmap";
        default:                              return "unknown error";
    }
}

#endif
