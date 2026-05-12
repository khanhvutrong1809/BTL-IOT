#include "temp_humi_monitor.h"
#include "led_blinky.h"
#include "neo_blinky.h"

namespace {
constexpr uint8_t kPrimaryLcdAddress = 0x21;
constexpr uint8_t kSecondaryLcdAddress = 0x27;
constexpr uint8_t kAlternateLcdAddress = 0x3F;
constexpr uint8_t kLcdColumns = 16;
constexpr uint8_t kLcdRows = 2;
constexpr uint8_t kI2cSdaPin = 11;
constexpr uint8_t kI2cSclPin = 12;
constexpr TickType_t kSensorDelayTicks = pdMS_TO_TICKS(5000);

LiquidCrystal_I2C lcdPrimary(kPrimaryLcdAddress, kLcdColumns, kLcdRows);
LiquidCrystal_I2C lcdSecondary(kSecondaryLcdAddress, kLcdColumns, kLcdRows);
LiquidCrystal_I2C lcdAlternate(kAlternateLcdAddress, kLcdColumns, kLcdRows);
LiquidCrystal_I2C* activeLcd = nullptr;
uint8_t activeLcdAddress = 0;
bool showStateScreen = false;

bool isI2cDeviceAvailable(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void logI2cDevices() {
    Serial.println("[I2C] Scanning bus...");
    bool foundDevice = false;

    for (uint8_t address = 1; address < 127; ++address) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C] Found device at 0x%02X\n", address);
            foundDevice = true;
        }
    }

    if (!foundDevice) {
        Serial.println("[I2C] No devices found");
    }
}

void writeLcdLine(uint8_t row, const String& content) {
    if (activeLcd == nullptr) {
        return;
    }

    activeLcd->setCursor(0, row);
    activeLcd->print("                ");
    activeLcd->setCursor(0, row);
    activeLcd->print(content);
}

const char* stateLabel(SystemState_t state) {
    switch (state) {
        case SYSTEM_STATE_NORMAL:
            return "NORMAL";
        case SYSTEM_STATE_WARNING:
            return "WARNING";
        case SYSTEM_STATE_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

void renderLcdReadings(float temperature, float humidity, bool hasValidReading, SystemState_t state) {
    if (activeLcd == nullptr) {
        return;
    }

    if (!hasValidReading) {
        writeLcdLine(0, "Temp: Error");
        writeLcdLine(1, "Humi: Error");
        return;
    }

    if (showStateScreen) {
        writeLcdLine(0, "System State");
        writeLcdLine(1, stateLabel(state));
        return;
    }

    if (hasValidReading) {
        writeLcdLine(0, "Temp: " + String(temperature, 1) + " C");
        writeLcdLine(1, "Humi: " + String(humidity, 1) + " %");
        return;
    }
}
}  // namespace

DHT20 dht20;

void updateSystemState(float temperature, float humidity) {
    bool tempInRange = (temperature >= 20.0f && temperature <= 30.0f);
    bool humidityInRange = (humidity >= 40.0f && humidity <= 70.0f);

    SystemState_t newState;

    if (tempInRange && humidityInRange) {
        newState = SYSTEM_STATE_NORMAL;
    } else if (!tempInRange && !humidityInRange) {
        newState = SYSTEM_STATE_CRITICAL;
    } else {
        newState = SYSTEM_STATE_WARNING;
    }

    if (newState != currentSystemState) {
        xSemaphoreTake(xSemaphoreStateNormal, 0);
        xSemaphoreTake(xSemaphoreStateWarning, 0);
        xSemaphoreTake(xSemaphoreStateCritical, 0);

        switch (newState) {
            case SYSTEM_STATE_NORMAL:
                xSemaphoreGive(xSemaphoreStateNormal);
                Serial.println("[SYSTEM] NORMAL");
                break;
            case SYSTEM_STATE_WARNING:
                xSemaphoreGive(xSemaphoreStateWarning);
                Serial.println("[SYSTEM] WARNING");
                break;
            case SYSTEM_STATE_CRITICAL:
                xSemaphoreGive(xSemaphoreStateCritical);
                Serial.println("[SYSTEM] CRITICAL");
                break;
        }

        currentSystemState = newState;
    }
}

void temp_humi_monitor(void *pvParameters) {
    Wire.begin(kI2cSdaPin, kI2cSclPin);
    Serial.begin(115200);
    delay(200);

    logI2cDevices();

    bool primaryDetected = isI2cDeviceAvailable(kPrimaryLcdAddress);
    bool secondaryDetected = isI2cDeviceAvailable(kSecondaryLcdAddress);
    bool alternateDetected = isI2cDeviceAvailable(kAlternateLcdAddress);
    Serial.printf("[LCD] 0x%02X %s | 0x%02X %s | 0x%02X %s\n",
                  kPrimaryLcdAddress,
                  primaryDetected ? "detected" : "not detected",
                  kSecondaryLcdAddress,
                  secondaryDetected ? "detected" : "not detected",
                  kAlternateLcdAddress,
                  alternateDetected ? "detected" : "not detected");

    if (primaryDetected) {
        activeLcd = &lcdPrimary;
        activeLcdAddress = kPrimaryLcdAddress;
    } else if (secondaryDetected) {
        activeLcd = &lcdSecondary;
        activeLcdAddress = kSecondaryLcdAddress;
    } else if (alternateDetected) {
        activeLcd = &lcdAlternate;
        activeLcdAddress = kAlternateLcdAddress;
    }

    if (activeLcd != nullptr) {
        activeLcd->begin();
        activeLcd->backlight();
        activeLcd->clear();
        writeLcdLine(0, "LCD init OK");
        writeLcdLine(1, "Addr: 0x" + String(activeLcdAddress, HEX));
    }

    bool dhtDetected = dht20.begin(kI2cSdaPin, kI2cSclPin);
    Serial.printf("[DHT20] %s\n", dhtDetected ? "detected" : "not detected");
    delay(1500);
    renderLcdReadings(0.0f, 0.0f, false, currentSystemState);

    while (1) {
        dht20.read();

        float temperature = dht20.getTemperature();
        float humidity = dht20.getHumidity();
        bool hasValidReading = !(isnan(temperature) || isnan(humidity));

        if (!hasValidReading) {
            Serial.println("Loi: Khong doc duoc cam bien DHT!");
            temperature = -1.0f;
            humidity = -1.0f;
        } else {
            Serial.printf("[SENSOR] Temp: %.1f C | Humi: %.1f %%\n", temperature, humidity);
        }
        Serial.printf("[LCD] runtime %s", activeLcd != nullptr ? "active" : "missing");
        if (activeLcd != nullptr) {
            Serial.printf(" at 0x%02X", activeLcdAddress);
        }
        Serial.println();

        if (xQueueTemperature != NULL) {
            xQueueOverwrite(xQueueTemperature, &temperature);
        }
        if (xQueueHumidity != NULL) {
            xQueueOverwrite(xQueueHumidity, &humidity);
        }

        updateSystemState(temperature, humidity);

        if (xSemaphoreTempUpdate != NULL) {
            xSemaphoreGive(xSemaphoreTempUpdate);
        }
        if (xSemaphoreHumidityUpdate != NULL) {
            xSemaphoreGive(xSemaphoreHumidityUpdate);
        }

        renderLcdReadings(temperature, humidity, hasValidReading, currentSystemState);
        showStateScreen = !showStateScreen;

        vTaskDelay(kSensorDelayTicks);
    }
}
