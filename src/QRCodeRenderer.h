#ifndef QR_CODE_RENDERER_H
#define QR_CODE_RENDERER_H

#include <Arduino.h>
#include "qrcode.h"
#include "PrintError.h"

enum class QRSize {
    Small = 1,
    Medium = 2,
    Large = 3
};

class QRCodeRenderer {
public:
    QRCodeRenderer();
    ~QRCodeRenderer();

    // Generiert QR-Code aus Daten
    // Gibt true bei Erfolg zurueck
    bool generate(const char* data);

    // Zeichnet QR-Code in ein Bitmap
    // bitmap: Ziel-Bitmap (1-bit, 0=weiss, gesetztes Bit=schwarz)
    // bitmapWidth/Height: Groesse des Bitmaps in Pixel
    // bytesPerRow: Bytes pro Zeile im Bitmap
    // y: Y-Position zum Zeichnen
    // size: Skalierung (Small=1x, Medium=2x, Large=3x)
    // outHeight: Optionaler Output fuer die gezeichnete Hoehe
    // Gibt true bei Erfolg zurueck
    bool draw(uint8_t* bitmap, int bitmapWidth, int bitmapHeight,
              int bytesPerRow, int y, QRSize size = QRSize::Large,
              int* outHeight = nullptr);

    // QR-Code Eigenschaften (nach generate())
    int getModuleCount() const { return _moduleCount; }
    int getVersion() const { return _version; }

    // Error code after failed generate()
    PrintError getError() const { return _error; }

    // Hilfsfunktion: Konvertiert String zu QRSize
    static QRSize sizeFromString(const char* str);

private:
    QRCode _qrcode;
    uint8_t* _qrcodeData;
    int _moduleCount;
    int _version;
    PrintError _error;

    // Pixel im Bitmap setzen
    void setPixel(uint8_t* bitmap, int bitmapWidth, int bitmapHeight,
                  int bytesPerRow, int x, int y);
};

#endif