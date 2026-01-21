#include "LabelImage.h"

LabelImage::LabelImage(int width, int height)
    : _width(width)
    , _height(height)
    , _bytesPerRow(width / 8)
    , _bitmapSize(height * (width / 8))
    , _bitmap(nullptr)
    , _error(PrintError::None)
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
    using namespace LabelLayout;
    int textWidth = charCount * CHAR_WIDTH - CHAR_SPACING;
    int startX = (_width - textWidth) / 2;
    if (startX < 0) startX = 0;

    const char* ptr = start;
    for (int charIndex = 0; charIndex < charCount && *ptr; charIndex++) {
        const uint8_t* glyph = getGlyph(&ptr);

        for (int col = 0; col < GLYPH_WIDTH; col++) {
            uint8_t colData = pgm_read_byte(&glyph[col]);
            for (int row = 0; row < GLYPH_HEIGHT; row++) {
                if (colData & (1 << row)) {
                    setPixel(startX + charIndex * CHAR_WIDTH + col, y + row);
                }
            }
        }
    }
}

void LabelImage::drawTextLineScaled(const char* start, int charCount, int y, int scale) {
    using namespace LabelLayout;
    int charWidth = GLYPH_WIDTH * scale + scale;  // Glyph width * scale + spacing
    int textWidth = charCount * charWidth - scale;
    int startX = (_width - textWidth) / 2;
    if (startX < 0) startX = 0;

    const char* ptr = start;
    for (int charIndex = 0; charIndex < charCount && *ptr; charIndex++) {
        const uint8_t* glyph = getGlyph(&ptr);

        for (int col = 0; col < GLYPH_WIDTH; col++) {
            uint8_t colData = pgm_read_byte(&glyph[col]);
            for (int row = 0; row < GLYPH_HEIGHT; row++) {
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
    using namespace LabelLayout;
    const int CHARS_PER_LINE = _width / CHAR_WIDTH;

    int lineCount = 0;
    const char* ptr = text;

    while (*ptr && lineCount < MAX_TEXT_LINES) {
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

    _error = PrintError::None;
    return true;
}

void LabelImage::drawLogo(int y) {
    // Center logo horizontally
    int logoX = (_width - MSB_LOGO_WIDTH) / 2;

    for (int ly = 0; ly < MSB_LOGO_HEIGHT; ly++) {
        for (int lx = 0; lx < MSB_LOGO_WIDTH; lx++) {
            int byteIdx = ly * MSB_LOGO_BYTES_PER_ROW + (lx / 8);
            int bitIdx = 7 - (lx % 8);
            uint8_t pixel = pgm_read_byte(&msbLogo[byteIdx]);

            // Logo data is inverted: 1 = white, 0 = black
            if (!(pixel & (1 << bitIdx))) {
                setPixel(logoX + lx, y + ly);
            }
        }
    }
}

bool LabelImage::generate(const char* link, const char* name, const char* id, QRSize qrSize) {
    using namespace LabelLayout;
    _error = PrintError::None;

    if (!_bitmap) {
        _error = PrintError::OutOfMemory;
        return false;
    }

    if (!link || strlen(link) == 0) {
        _error = PrintError::MissingLink;
        return false;
    }

    clear();

    // Layout: QR code at top with margin
    int qrY = QR_TOP_MARGIN;
    int qrHeight = 0;

    if (!drawQRCode(link, qrY, qrSize, &qrHeight)) {
        // _error is set in drawQRCode
        if (_error == PrintError::None) {
            _error = PrintError::QRCodeFailed;
        }
        return false;
    }

    // ID below QR code
    int idY = qrY + qrHeight + QR_BOTTOM_MARGIN;
    // Name below ID
    int nameY = idY + ID_HEIGHT + ID_BOTTOM_MARGIN;

    // Draw text elements
    drawTextScaled(id, idY, ID_SCALE);
    int textLines = drawTextCentered(name, nameY);

    // Calculate logo position (below text with margin)
    int textEndY = nameY + textLines * LINE_HEIGHT;
    int logoY = textEndY + LOGO_TOP_MARGIN;

    // Only draw logo if it fits on the label
    if (logoY + MSB_LOGO_HEIGHT <= _height) {
        drawLogo(logoY);
    }

    return true;
}

bool LabelImage::generateFrame() {
    _error = PrintError::None;

    if (!_bitmap) {
        _error = PrintError::OutOfMemory;
        return false;
    }

    // Fill with white
    memset(_bitmap, 0xFF, _bitmapSize);

    // Draw top border (full row black)
    memset(_bitmap, 0x00, _bytesPerRow);

    // Draw bottom border (full row black)
    memset(_bitmap + (_height - 1) * _bytesPerRow, 0x00, _bytesPerRow);

    // Draw left and right borders
    for (int y = 0; y < _height; y++) {
        // Left edge: clear bit 7 (leftmost pixel)
        _bitmap[y * _bytesPerRow] &= 0x7F;
        // Right edge: clear bit 0 (rightmost pixel)
        _bitmap[y * _bytesPerRow + _bytesPerRow - 1] &= 0xFE;
    }

    return true;
}

// Base64 encoding table
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

    // BMP structure for 1-bit monochrome
    // Row padding: each row must be aligned to 4 bytes
    int rowPadding = (4 - (_bytesPerRow % 4)) % 4;
    int paddedRowSize = _bytesPerRow + rowPadding;
    int pixelDataSize = paddedRowSize * _height;

    // BMP header sizes
    const int fileHeaderSize = 14;
    const int infoHeaderSize = 40;
    const int colorTableSize = 8;  // 2 colors * 4 bytes
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

    // Color table (black and white)
    bmp[54] = 0x00; bmp[55] = 0x00; bmp[56] = 0x00; bmp[57] = 0x00;  // Black
    bmp[58] = 0xFF; bmp[59] = 0xFF; bmp[60] = 0xFF; bmp[61] = 0x00;  // White

    // Pixel data (BMP stores bottom-up)
    uint8_t* pixelData = bmp + dataOffset;
    for (int y = 0; y < _height; y++) {
        int srcY = _height - 1 - y;  // BMP is bottom-up
        for (int x = 0; x < _bytesPerRow; x++) {
            // Copy pixel data (0=black in both formats)
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
