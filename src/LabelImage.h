#ifndef LABEL_IMAGE_H
#define LABEL_IMAGE_H

#include <Arduino.h>
#include "qrcode.h"
#include "font5x7.h"
#include "msb_logo.h"

class LabelImage {
public:
    // Konstruktor mit Label-Groesse in Pixel
    LabelImage(int width, int height);
    ~LabelImage();

    // Getter
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getBytesPerRow() const { return _bytesPerRow; }
    int getBitmapSize() const { return _bitmapSize; }

    // Generiert Label-Bild mit QR-Code und Text
    // Gibt true bei Erfolg zurueck, false bei Fehler
    bool generate(const char* link, const char* name, const char* id);

    // Zugriff auf Bitmap-Daten (nach generate())
    const uint8_t* getData() const { return _bitmap; }

    // Fehlertext nach fehlgeschlagenem generate()
    const char* getError() const { return _error; }

    // Generiert Base64-kodierte Data-URL (BMP-Format) zum Anzeigen im Browser
    // Rueckgabe muss mit free() freigegeben werden!
    char* toDataURL() const;

private:
    int _width;
    int _height;
    int _bytesPerRow;
    int _bitmapSize;
    uint8_t* _bitmap;
    const char* _error;

    // Bitmap loeschen (weiss)
    void clear();

    // Pixel setzen (schwarz)
    void setPixel(int x, int y);

    // Text-Rendering
    void drawTextLine(const char* start, int charCount, int y);
    void drawTextLineScaled(const char* start, int charCount, int y, int scale);
    int drawTextCentered(const char* text, int y);  // Returns number of lines
    void drawTextScaled(const char* text, int y, int scale);

    // QR-Code zeichnen
    bool drawQRCode(const char* data, int y, int* outHeight);

    // Logo zeichnen
    void drawLogo(int y);
};

#endif
