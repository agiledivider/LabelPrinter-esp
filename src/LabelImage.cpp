#include "LabelImage.h"

LabelImage::LabelImage(int width, int height)
    : _width(width)
    , _height(height)
    , _bytesPerRow(width / 8)
    , _bitmapSize(height * (width / 8))
    , _bitmap(nullptr)
    , _error(nullptr)
{
    _bitmap = (uint8_t*)malloc(_bitmapSize);
}

LabelImage::~LabelImage() {
    if (_bitmap) {
        free(_bitmap);
        _bitmap = nullptr;
    }
}

void LabelImage::clear() {
    if (_bitmap) {
        memset(_bitmap, 0xFF, _bitmapSize);
    }
}

void LabelImage::setPixel(int x, int y) {
    if (_bitmap && x >= 0 && x < _width && y >= 0 && y < _height) {
        int byteIdx = y * _bytesPerRow + (x / 8);
        int bitIdx = 7 - (x % 8);
        _bitmap[byteIdx] &= ~(1 << bitIdx);
    }
}

void LabelImage::drawTextLine(const char* start, int charCount, int y) {
    int textWidth = charCount * 6 - 1;  // 5 Pixel + 1 Pixel Abstand
    int startX = (_width - textWidth) / 2;
    if (startX < 0) startX = 0;

    const char* ptr = start;
    for (int charIndex = 0; charIndex < charCount && *ptr; charIndex++) {
        const uint8_t* glyph = getGlyph(&ptr);

        for (int col = 0; col < 5; col++) {
            uint8_t colData = pgm_read_byte(&glyph[col]);
            for (int row = 0; row < 7; row++) {
                if (colData & (1 << row)) {
                    setPixel(startX + charIndex * 6 + col, y + row);
                }
            }
        }
    }
}

void LabelImage::drawTextLineScaled(const char* start, int charCount, int y, int scale) {
    int charWidth = 5 * scale + scale;  // 5 Pixel * scale + spacing
    int textWidth = charCount * charWidth - scale;
    int startX = (_width - textWidth) / 2;
    if (startX < 0) startX = 0;

    const char* ptr = start;
    for (int charIndex = 0; charIndex < charCount && *ptr; charIndex++) {
        const uint8_t* glyph = getGlyph(&ptr);

        for (int col = 0; col < 5; col++) {
            uint8_t colData = pgm_read_byte(&glyph[col]);
            for (int row = 0; row < 7; row++) {
                if (colData & (1 << row)) {
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            setPixel(startX + charIndex * charWidth + col * scale + sx,
                                     y + row * scale + sy);
                        }
                    }
                }
            }
        }
    }
}

int LabelImage::drawTextCentered(const char* text, int y) {
    const int MAX_LINES = 10;
    const int CHARS_PER_LINE = _width / 6;
    const int LINE_HEIGHT = 9;

    int lineCount = 0;
    const char* ptr = text;

    while (*ptr && lineCount < MAX_LINES) {
        const char* lineStart = ptr;
        const char* lastSpace = nullptr;
        int charCount = 0;

        while (*ptr && charCount < CHARS_PER_LINE) {
            if (*ptr == ' ') {
                lastSpace = ptr;
            }

            if ((uint8_t)*ptr == 0xC3 && ptr[1] != 0) {
                ptr += 2;
            } else {
                ptr++;
            }
            charCount++;
        }

        if (*ptr && lastSpace && lastSpace > lineStart) {
            ptr = lastSpace + 1;
            charCount = 0;
            const char* tmp = lineStart;
            while (tmp < lastSpace) {
                if ((uint8_t)*tmp == 0xC3 && tmp[1] != 0) {
                    tmp += 2;
                } else {
                    tmp++;
                }
                charCount++;
            }
        }

        if (charCount > 0) {
            drawTextLine(lineStart, charCount, y + lineCount * LINE_HEIGHT);
        }
        lineCount++;

        while (*ptr == ' ') ptr++;
    }
    return lineCount;
}

void LabelImage::drawTextScaled(const char* text, int y, int scale) {
    int len = countDisplayChars(text);
    drawTextLineScaled(text, len, y, scale);
}

bool LabelImage::drawQRCode(const char* data, int y, QRSize size, int* outHeight) {
    QRCodeRenderer qr;

    if (!qr.generate(data)) {
        _error = qr.getError();
        return false;
    }

    if (!qr.draw(_bitmap, _width, _height, _bytesPerRow, y, size, outHeight)) {
        _error = qr.getError();
        return false;
    }

    return true;
}

void LabelImage::drawLogo(int y) {
    // Logo zentrieren (50px Logo auf 96px Label)
    int logoX = (_width - MSB_LOGO_WIDTH) / 2;

    for (int ly = 0; ly < MSB_LOGO_HEIGHT; ly++) {
        for (int lx = 0; lx < MSB_LOGO_WIDTH; lx++) {
            int byteIdx = ly * MSB_LOGO_BYTES_PER_ROW + (lx / 8);
            int bitIdx = 7 - (lx % 8);
            uint8_t pixel = pgm_read_byte(&msbLogo[byteIdx]);

            // Logo Daten sind invertiert: 1 = weiss, 0 = schwarz
            if (!(pixel & (1 << bitIdx))) {
                setPixel(logoX + lx, y + ly);
            }
        }
    }
}

bool LabelImage::generate(const char* link, const char* name, const char* id, QRSize qrSize) {
    _error = nullptr;

    if (!_bitmap) {
        _error = "out of memory";
        return false;
    }

    if (!link || strlen(link) == 0) {
        _error = "missing link";
        return false;
    }

    clear();

    // Layout
    int qrY = 10;
    int qrHeight = 0;

    if (!drawQRCode(link, qrY, qrSize, &qrHeight)) {
        // _error wird in drawQRCode gesetzt
        if (!_error) {
            _error = "QR code generation failed";
        }
        return false;
    }

    int idY = qrY + qrHeight + 10;
    int nameY = idY + 22;

    // Text zeichnen
    drawTextScaled(id, idY, 2);
    int textLines = drawTextCentered(name, nameY);

    // Logo Position berechnen (nach Text, mit Abstand)
    const int LINE_HEIGHT = 9;
    const int ID_HEIGHT = 14;  // 7 * 2 (scale)
    int textEndY = nameY + textLines * LINE_HEIGHT;

    // Logo am unteren Ende, mindestens 10px nach Text
    int logoY = textEndY + 10;

    // Sicherstellen, dass Logo aufs Label passt
    if (logoY + MSB_LOGO_HEIGHT <= _height) {
        drawLogo(logoY);
    }

    return true;
}

// Base64 Encoding Tabelle
static const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64Encode(const uint8_t* data, int len, char* out) {
    int i = 0, j = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        out[j++] = base64Chars[(triple >> 18) & 0x3F];
        out[j++] = base64Chars[(triple >> 12) & 0x3F];
        out[j++] = (i > len + 1) ? '=' : base64Chars[(triple >> 6) & 0x3F];
        out[j++] = (i > len) ? '=' : base64Chars[triple & 0x3F];
    }
    out[j] = '\0';
}

char* LabelImage::toDataURL() const {
    if (!_bitmap) return nullptr;

    // BMP Struktur fuer 1-bit Monochrom
    // Row padding: jede Zeile muss auf 4 Bytes ausgerichtet sein
    int rowPadding = (4 - (_bytesPerRow % 4)) % 4;
    int paddedRowSize = _bytesPerRow + rowPadding;
    int pixelDataSize = paddedRowSize * _height;

    // BMP Header Groessen
    const int fileHeaderSize = 14;
    const int infoHeaderSize = 40;
    const int colorTableSize = 8;  // 2 Farben * 4 Bytes
    int bmpSize = fileHeaderSize + infoHeaderSize + colorTableSize + pixelDataSize;

    uint8_t* bmp = (uint8_t*)malloc(bmpSize);
    if (!bmp) return nullptr;
    memset(bmp, 0, bmpSize);

    int dataOffset = fileHeaderSize + infoHeaderSize + colorTableSize;

    // BITMAPFILEHEADER (14 bytes)
    bmp[0] = 'B'; bmp[1] = 'M';
    bmp[2] = bmpSize & 0xFF;
    bmp[3] = (bmpSize >> 8) & 0xFF;
    bmp[4] = (bmpSize >> 16) & 0xFF;
    bmp[5] = (bmpSize >> 24) & 0xFF;
    bmp[10] = dataOffset & 0xFF;
    bmp[11] = (dataOffset >> 8) & 0xFF;

    // BITMAPINFOHEADER (40 bytes)
    bmp[14] = infoHeaderSize;
    bmp[18] = _width & 0xFF;
    bmp[19] = (_width >> 8) & 0xFF;
    bmp[22] = _height & 0xFF;
    bmp[23] = (_height >> 8) & 0xFF;
    bmp[26] = 1;  // planes
    bmp[28] = 1;  // bits per pixel
    bmp[34] = pixelDataSize & 0xFF;
    bmp[35] = (pixelDataSize >> 8) & 0xFF;
    bmp[36] = (pixelDataSize >> 16) & 0xFF;
    bmp[37] = (pixelDataSize >> 24) & 0xFF;

    // Color Table (schwarz und weiss)
    bmp[54] = 0x00; bmp[55] = 0x00; bmp[56] = 0x00; bmp[57] = 0x00;  // Schwarz
    bmp[58] = 0xFF; bmp[59] = 0xFF; bmp[60] = 0xFF; bmp[61] = 0x00;  // Weiss

    // Pixel-Daten (BMP speichert von unten nach oben)
    uint8_t* pixelData = bmp + dataOffset;
    for (int y = 0; y < _height; y++) {
        int srcY = _height - 1 - y;  // BMP ist bottom-up
        for (int x = 0; x < _bytesPerRow; x++) {
            // Bits invertieren: im Bitmap ist 0=schwarz, in BMP ist 0=erste Farbe (schwarz)
            pixelData[y * paddedRowSize + x] = _bitmap[srcY * _bytesPerRow + x];
        }
    }

    // Base64 Encoding
    int base64Len = ((bmpSize + 2) / 3) * 4;
    const char* prefix = "data:image/bmp;base64,";
    int prefixLen = strlen(prefix);

    char* result = (char*)malloc(prefixLen + base64Len + 1);
    if (!result) {
        free(bmp);
        return nullptr;
    }

    strcpy(result, prefix);
    base64Encode(bmp, bmpSize, result + prefixLen);

    free(bmp);
    return result;
}
