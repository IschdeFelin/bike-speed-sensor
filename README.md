# ESP32 Bluetooth Fahrrad-Geschwindigkeitssensor

Dieses Projekt implementiert einen Fahrrad-Geschwindigkeitssensor basierend auf einem ESP32, einem Linienverfolgungssensor (Reflexions-Lichtschranke) und Bluetooth Low Energy (BLE). Der Sensor verwendet den BLE-Service **Cycling Speed and Cadence Service (CSCS)**, um Radumdrehungsdaten an verbundene Geräte (z. B. eine Smartphone-App oder ein Fahrradcomputer) zu senden.

## Funktionen

- **BLE Cycling Speed and Cadence Service (CSCS)** gemäß Bluetooth-Spezifikation.
- **Radumdrehungserkennung** über eine Lichtschranke mit GPIO-Interrupt.
- **Anzeige des Verbindungsstatus** über eine LED (GPIO 2).
- **Visuelle Bestätigung bei erkannter Umdrehung** über eine weitere LED (GPIO 26).
- **Mehrfachverbindungen**: Bis zu 3 Clients können gleichzeitig verbunden sein.
- **SC Control Point Unterstützung**, z. B. zum Zurücksetzen des Umdrehungszählers.

## Verwendete Hardware

- ESP32 Dev Board
- Linienverfolgungssensor (Reflexionssensor) auf GPIO 25
- LED für Bluetooth-Status auf GPIO 2
- LED zur Anzeige von Radumdrehungen auf GPIO 26

## Anschlussdiagramm

| Komponente               | GPIO-Pin am ESP32 |
|--------------------------|-------------------|
| Lichtschranke            | 25                |
| Status-LED (BLE-Verbindung) | 2             |
| Impuls-LED (bei Umdrehung)  | 26            |

## Bluetooth-Implementierung

- **Service UUID**: `0x1816` (Cycling Speed and Cadence)
- **Charakteristiken**:
  - `0x2A5B` - CSC Measurement (Notify)
  - `0x2A5C` - CSC Feature (Read, unterstützt nur Radumdrehungen)
  - `0x2A55` - SC Control Point (Write, Indicate)

## Firmware-Logik

- Bei einer erkannten Radumdrehung wird ein Zähler erhöht und die Zeit (in 1/1024 s) seit Systemstart berechnet.
- Die gemessenen Daten werden periodisch (alle 500 ms) via BLE übertragen, falls ein Client verbunden ist.
- Die Status-LED blinkt, wenn keine Clients verbunden sind.
- Die Impuls-LED blinkt kurz bei jeder erkannten Umdrehung.

## Projektaufbau

Der Quellcode ist in der Datei `SensorV1.ino` enthalten (siehe oben).

## Kompilieren und Hochladen

1. Öffne die `.ino`-Datei in der Arduino IDE.
2. Wähle das passende ESP32 Board aus (`ESP32 Dev Module`).
3. Lade den Sketch auf das Board hoch.

## Getestete Clients

- nRF Connect (Android)
- Garmin Vivoactive 3 Music
- Garmin Vivoactive 5

## TODO / Weiterentwicklung

- Optionale Unterstützung für Kadenz (Trittfrequenz)
- Batterie-Status per BLE

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz. Siehe `LICENSE` für Details.
