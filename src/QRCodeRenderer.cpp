#include "QRCodeRenderer.h"

QRCodeRenderer::QRCodeRenderer()
    : _qrcodeData(nullptr)
    , _moduleCount(0)
    , _version(0)
    , _error(nullptr)
{
}

QRCodeRenderer::~QRCodeRenderer() {
    if (_qrcodeData) {
        free(_qrcodeData);
        _qrcodeData = nullptr;
    }
}

bool QRCodeRenderer::generate(const char* data) {
    _error = nullptr;
    _version = 0;
    _moduleCount = 0;

    if (!data || strlen(data) == 0) {
        _error = "empty QR data";
        return false;
    }

    // Alte Daten freigeben
    if (_qrcodeData) {
        free(_qrcodeData);
        _qrcodeData = nullptr;
    }

    // Finde kleinste Version die den Text kodieren kann
    int bestVersion = 0;
    for (int version = 3; version <= 12; version++) {
        uint8_t tempData[qrcode_getBufferSize(version)];
        if (qrcode_initText(&_qrcode, tempData, version, ECC_MEDIUM, data) == 0) {
            bestVersion = version;
            break;
        }
    }

    if (bestVersion == 0) {
        _error = "QR data too long";
        return false;
    }

    // Buffer allokieren und QR-Code generieren
    _qrcodeData = (uint8_t*)malloc(qrcode_getBufferSize(bestVersion));
    if (!_qrcodeData) {
        _error = "out of memory";
        return false;
    }

    if (qrcode_initText(&_qrcode, _qrcodeData, bestVersion, ECC_MEDIUM, data) != 0) {
        free(_qrcodeData);
        _qrcodeData = nullptr;
        _error = "QR generation failed";
        return false;
    }

    _version = bestVersion;
    _moduleCount = _qrcode.size;
    return true;
}

void QRCodeRenderer::setPixel(uint8_t* bitmap, int bitmapWidth, int bitmapHeight,
                               int bytesPerRow, int x, int y) {
    if (bitmap && x >= 0 && x < bitmapWidth && y >= 0 && y < bitmapHeight) {
        int byteIdx = y * bytesPerRow + (x / 8);
        int bitIdx = 7 - (x % 8);
        bitmap[byteIdx] &= ~(1 << bitIdx);
    }
}

bool QRCodeRenderer::draw(uint8_t* bitmap, int bitmapWidth, int bitmapHeight,
                          int bytesPerRow, int y, QRSize size, int* outHeight) {
    if (!_qrcodeData || _moduleCount == 0) {
        _error = "no QR code generated";
        return false;
    }

    if (!bitmap) {
        _error = "invalid bitmap";
        return false;
    }

    int scale = static_cast<int>(size);

    // Berechne automatische Skalierung wenn QR-Code zu gross waere
    int maxScale = bitmapWidth / _moduleCount;
    if (maxScale < 1) maxScale = 1;
    if (scale > maxScale) scale = maxScale;

    int qrPixels = _moduleCount * scale;
    int qrX = (bitmapWidth - qrPixels) / 2;

    // Zeichne QR-Code Module
    for (int qy = 0; qy < _moduleCount; qy++) {
        for (int qx = 0; qx < _moduleCount; qx++) {
            if (qrcode_getModule(&_qrcode, qx, qy)) {
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        setPixel(bitmap, bitmapWidth, bitmapHeight, bytesPerRow,
                                 qrX + qx * scale + sx, y + qy * scale + sy);
                    }
                }
            }
        }
    }

    if (outHeight) {
        *outHeight = qrPixels;
    }
    return true;
}

QRSize QRCodeRenderer::sizeFromString(const char* str) {
    if (!str || strlen(str) == 0) {
        return QRSize::Large;  // Default
    }

    char c = str[0];
    if (c == 's' || c == 'S') {
        return QRSize::Small;
    } else if (c == 'm' || c == 'M') {
        return QRSize::Medium;
    }
    // Default: Large (auch fuer 'L', 'l' oder unbekannt)
    return QRSize::Large;
}
