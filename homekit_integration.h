#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================================
// 🍏 NATIVE APPLE HOMEKIT INTEGRATION VIA HOMESPAN
// ============================================================================
// Позволяет подключать Jarvis напрямую к Apple HomeKit без мостов и сторонних серверов.
// Для активации раскомментируйте #define ENABLE_HOMESPAN в config.h
// и установите библиотеку HomeSpan через Менеджер библиотек Arduino IDE.
//
// Также Jarvis может транслироваться в Apple HomeKit и Яндекс Алису в 1 клик
// через Home Assistant (MQTT Auto-Discovery уже активен!).
// ============================================================================

#ifdef ENABLE_HOMESPAN
#include <HomeSpan.h>

struct JarvisTempSensor : Service::TemperatureSensor {
    SpanCharacteristic *temp;

    JarvisTempSensor() : Service::TemperatureSensor() {
        temp = new Characteristic::CurrentTemperature(20.0);
        temp->setRange(-40.0, 80.0);
    }

    void update(float currentT) {
        if (!isnan(currentT)) {
            temp->setVal(currentT);
        }
    }
};

struct JarvisHumSensor : Service::HumiditySensor {
    SpanCharacteristic *hum;

    JarvisHumSensor() : Service::HumiditySensor() {
        hum = new Characteristic::CurrentRelativeHumidity(50.0);
        hum->setRange(0.0, 100.0);
    }

    void update(float currentH) {
        if (!isnan(currentH)) {
            hum->setVal(currentH);
        }
    }
};

static JarvisTempSensor *hkTemp = nullptr;
static JarvisHumSensor  *hkHum  = nullptr;
static bool homekitInitialized = false;

inline void initHomeKit() {
    Serial.println("[HomeKit] Инициализация HomeSpan Apple HomeKit...");

    // Настройка параметров аксессуара
    homeSpan.setControlPin(0); // Пин сброса настроек (кнопка Boot)
    homeSpan.setStatusPin(2);  // Индикация статуса
    homeSpan.setLogLevel(1);

    // Запуск HomeSpan (работает поверх существующего Wi-Fi)
    homeSpan.begin(Category::Bridges, "Jarvis Climate Bridge");

    // Создание аксессуара
    new SpanAccessory();
        new Service::AccessoryInformation();
            new Characteristic::Identify();
            new Characteristic::Name("Jarvis Климат");
            new Characteristic::Manufacturer("Jarvis Project");
            new Characteristic::SerialNumber("JARVIS-ESP32C3-001");
            new Characteristic::Model("ESP32-C3 SuperMini");
            new Characteristic::FirmwareRevision("3.0");

    new SpanAccessory();
        new Service::AccessoryInformation();
            new Characteristic::Identify();
            new Characteristic::Name("Температура");
        hkTemp = new JarvisTempSensor();

    new SpanAccessory();
        new Service::AccessoryInformation();
            new Characteristic::Identify();
            new Characteristic::Name("Влажность");
        hkHum = new JarvisHumSensor();

    homekitInitialized = true;
    Serial.println("[HomeKit] ✅ Apple HomeKit готов к сопряжению! Код сопряжения по умолчанию: 466-37-726");
}

inline void updateHomeKit(float t, float h) {
    if (!homekitInitialized) return;
    if (hkTemp) hkTemp->update(t);
    if (hkHum)  hkHum->update(h);
}

inline void loopHomeKit() {
    if (!homekitInitialized) return;
    homeSpan.poll();
}

#else

// Заглушки, если HomeSpan отключен (экономит память и ресурсы)
inline void initHomeKit() {}
inline void updateHomeKit(float t, float h) {}
inline void loopHomeKit() {}

#endif
