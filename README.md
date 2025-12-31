# Nelko P21 Label Printer - ESP32

ESP32-basierter Controller für den Nelko P21 Thermodrucker via Bluetooth SPP.

## Hardware
- ESP32 DevKit (oder kompatibel)
- Nelko P21 Thermodrucker

## Drucker-Spezifikationen
- **Bluetooth**: SPP (Serial Port Profile)
- **Labelgröße**: 96 x 284 Pixel
- **Format**: Bitmap-basiert

## Befehle (Serial Monitor @ 115200 Baud)
| Befehl | Beschreibung |
|--------|--------------|
| `scan` | Nach Bluetooth-Geräten suchen |
| `connect` | Mit Drucker verbinden (Name: P21) |
| `connect <name>` | Mit spezifischem Gerät verbinden |
| `disc` | Verbindung trennen |
| `status` | Verbindungsstatus anzeigen |
| `list` | Gefundene Geräte auflisten |
| `test` | Test-Befehl senden |
| `help` | Hilfe anzeigen |

## Nutzung
1. `pio run -t upload` - Firmware flashen
2. `pio device monitor` - Serial Monitor öffnen
3. `scan` eingeben - Nach Drucker suchen
4. `connect` eingeben - Mit Drucker verbinden

## Nächste Schritte
- [ ] Drucker-Protokoll analysieren (Befehle herausfinden)
- [ ] Bitmap-Konvertierung implementieren (96x284px)
- [ ] Label-Druckfunktion erstellen