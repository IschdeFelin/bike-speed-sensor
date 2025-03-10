#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


// Gerät Variablen
#define LED_PIN 2
#define LED2_PIN 26
#define WHEEL_SENSOR_PIN 25
#define MAX_CLIENTS 3
uint32_t lastLoopTime = 0;
int connectedClients = 0;

// Gemessene Daten
uint32_t totalRevolutions = 0;          // Gesamtzahl der Radumdrehungen
uint16_t lastEventTime = 0;             // Zeit in 1/1024 Sekunden seit der letzten Umdrehung
uint32_t lastPulseTime = 0;             // Dei Zeit des letzten Impulses
volatile bool isWheelDetected = false;  // Ob das Rad erkannt wurde (eine Umdrehung)

// BLE Objekte
BLEServer* pServer;
BLEService* pCSCS;
BLECharacteristic* pCSCMeasurement;
BLECharacteristic* pCSCFeature;


void IRAM_ATTR onWheelPulse() {
    isWheelDetected = true;
    // Serial.println("Umdrehung erkannt");
}

void sendSpeedData() {
    uint8_t speedData[7];

    // Flags: Nur Radumdrehungen enthalten
    speedData[0] = 0x01; // 0x01 für Radumdrehungsdaten vorhanden; 0x02 für Kurbeldrehungsdaten; 0x03 für beides --> kann sich während der Laufzeit ändern, jenachdem welche Daten gerade verfügbar sind

    // Radumdrehungen (4 Byte, Little Endian)
    memcpy(&speedData[1], (void*)&totalRevolutions, 4);

    // Eventzeit (2 Byte, Little Endian)
    memcpy(&speedData[5], (void*)&lastEventTime, 2);

    // Daten an die Charakteristik senden
    pCSCMeasurement->setValue(speedData, sizeof(speedData));
    pCSCMeasurement->notify();

    Serial.printf("Radumdrehungen: %u, Letztes Ereignis: %u\n", totalRevolutions, lastEventTime);
}

void sendResponse(BLECharacteristic* pCharacteristic, uint8_t opCode, uint8_t requestOpCode, uint8_t responseValue) {
    uint8_t response[] = {opCode, requestOpCode, responseValue};
    pCharacteristic->setValue(response, sizeof(response));
    pCharacteristic->indicate();
}

// Callback-Klasse für den BLE-Server, um Verbindungsereignisse zu handhaben
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        connectedClients++;

        // Werben fortsetzen, falls Platz für weitere Verbindungen
        if (connectedClients < MAX_CLIENTS) {
            BLEDevice::startAdvertising();
        }

        // Ausgabe und LED Steuerung
        Serial.printf("Neues Gerät verbunden. Aktive Verbindungen: %d\n", connectedClients);
        digitalWrite(LED_PIN, HIGH);
    }

    void onDisconnect(BLEServer* pServer) {
        connectedClients--;
        Serial.println("Device Disconnected");
        BLEDevice::startAdvertising();
    }
};

// Callback-Klasse für den SC Control Point
class SCControlPointCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        String value = pCharacteristic->getValue();
        Serial.println("SC Control Point: Daten empfangen");

        if (value.length() < 5) {
            Serial.println("SC Control Point: Daten zu kurz");
            // Fehler: Ungültiger Parameter (z.B. zu wenige Daten)
            sendResponse(pCharacteristic, 0x10, 0x00, 0x03);
            return;
        }

        uint8_t opCode = value[0];  // Der erste Byte ist der Op Code

        if (opCode == 0x01) {  // Befehl 0x01: Set Cumulative Value
            uint32_t newRevolutions;
            memcpy(&newRevolutions, &value[1], 4);  // Extrahiere die 4-Byte-Daten (evtl. mit "uint32_t newValue = *(uint32_t*)&value[1];", da schneller)
            totalRevolutions = newRevolutions;

            Serial.printf("SC Control Point: Radumdrehungen auf %u gesetzt\n", totalRevolutions);

            // Erfolgscode (Response Code Op Code; Request Op Code, der ursprüngliche Befehl; Response Value, „Success“)
            sendResponse(pCharacteristic, 0x10, 0x01, 0x01);
        } else {
            Serial.println("SC Control Point: Nicht unterstützter Op Code");
            // Fehler: Unsupported Op Code
            sendResponse(pCharacteristic, 0x10, opCode, 0x02);
        }
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // Status LED, Daten LED und Radsensor
    pinMode(LED_PIN, OUTPUT);
    pinMode(WHEEL_SENSOR_PIN, INPUT);
    pinMode(LED2_PIN, OUTPUT);

    // Interrupt für steigende Flanke einrichten (wenn Rad erkannt, also wenn HIGH)
    attachInterrupt(WHEEL_SENSOR_PIN, onWheelPulse, RISING);

    // BLE initialisieren
    BLEDevice::init("ESP32 Speed Sensor");

    // BLE-Server erstellen
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Cycling Speed and Cadence Service (CSCS) erstellen
    pCSCS = pServer->createService(BLEUUID((uint16_t)0x1816));

    // CSC Measurement Charakteristik erstellen
    pCSCMeasurement = pCSCS->createCharacteristic(
        BLEUUID((uint16_t)0x2A5B),
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCSCMeasurement->addDescriptor(new BLE2902());

    // CSC Feature Charakteristik erstellen
    pCSCFeature = pCSCS->createCharacteristic(
        BLEUUID((uint16_t)0x2A5C),
        BLECharacteristic::PROPERTY_READ
    );
    uint16_t features = 0x0001; // Unterstützt nur Radumdrehungen --> Für Radumdrehungen und Kadenz "0x03", da dann "00000011"; Für Nur Kadenz "0x02", da dann "00000010"
    pCSCFeature->setValue((uint8_t*)&features, sizeof(features));

    // CS Control Point Charakteristik erstellen
    BLECharacteristic* pSCControlPoint = pCSCS->createCharacteristic(
        BLEUUID((uint16_t)0x2A55),
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_INDICATE
    );
    pSCControlPoint->addDescriptor(new BLE2902());
    pSCControlPoint->setCallbacks(new SCControlPointCallbacks());

    // Service starten
    pCSCS->start();

    // BLE-Advertising starten
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(pCSCS->getUUID());
    pAdvertising->start();

    Serial.println("BLE Cycling Speed and Cadence Service gestartet!");
}

void loop() {
    uint32_t currentMillis = millis();
    // Wenn ein Rad erkannt wurde und die letzte Erkennunt mindestens 20ms her ist
    if (isWheelDetected && (currentMillis - lastPulseTime > 20)) {
        Serial.println("Radumdrehung erkannt");
        totalRevolutions++;             // Zähler für Radumdrehungen erhöhen
        lastPulseTime = currentMillis;  // NEU, NACH TEST... eventuell entfernen für schnellere Geschwindigkeiten

        // Berechne die Zeit des letzten Ereignisses in 1/1024 Sekunden
        lastEventTime = (currentMillis * 1024) / 1000;  // Umrechnung von Millisekunden in 1/1024 Sekunden
        lastEventTime %= 65536;                         // Roll-Over nach 64 Sekunden

        isWheelDetected = false;

        digitalWrite(LED2_PIN, HIGH);
        delay(2);
        digitalWrite(LED2_PIN, LOW);
    }

    // Jede Sekunde ausführen
    if (currentMillis - lastLoopTime >= 500) {
        lastLoopTime = currentMillis;

        // Wenn mindestens ein Client verbunden ist werden die Geschwindigkeitsdaten gesendet, ansonsten blinkt die Status LED
        if (connectedClients > 0) {
            // Geschwindigkeit senden
            sendSpeedData();
        } else {
            // LED blinkt im 500 ms Takt
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
}