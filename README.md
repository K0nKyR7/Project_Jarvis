# 🌟 Project Jarvis — Умный мониторинг микроклимата v3.0 Ultimate

[![ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3%20SuperMini-blue.svg)](https://espressif.com)
[![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20Multitasking-green.svg)](https://freertos.org)
[![Home Assistant](https://img.shields.io/badge/Integration-Home%20Assistant%20MQTT-41bdf5.svg)](https://www.home-assistant.io)
[![Apple HomeKit](https://img.shields.io/badge/Apple-HomeKit%20Ready-black.svg)](#)
[![Yandex Alice](https://img.shields.io/badge/Voice-Яндекс%20Алиса-red.svg)](#)
[![Web OTA](https://img.shields.io/badge/Firmware-Web%20OTA%20Upload-orange.svg)](#)
[![PWA](https://img.shields.io/badge/Web-PWA%20Installable-ff69b4.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Автономная инженерная станция мониторинга качества воздуха и микроклимата на базе микроконтроллера **ESP32-C3 SuperMini**. 

Проект сочетает в себе **встроенный PWA-дашборд с 10 дизайнерскими темами** в стиле Glassmorphism 2.0, **многопоточного Telegram-бота** на базе FreeRTOS с защитой мьютексами, бесшовную **интеграцию с Home Assistant, Apple HomeKit и Яндекс Алисой**, **прошивку по воздуху Web OTA** прямо из браузера, автономный **Wi-Fi Captive Portal** для смены сети без перепрошивки и **аппаратный ШИМ 5 кГц с гамма-коррекцией CIE1931** для визуальной индикации комфорта.

---

## ⚡ Ключевые возможности v3.0

* **📡 Обновление прошивки по воздуху (Web OTA)**:
  * Прошивка платы прямо через браузер перетаскиванием `.bin` файла в веб-интерфейсе. Больше не нужно носить устройство к ПК и подключать кабелем.
* **📶 Wi-Fi Captive Portal (автономная смена сети)**:
  * Если устройство не может подключиться к сохранённому роутеру, оно автоматически поднимает сеть `Jarvis-Setup` (192.168.4.1) и открывает портал сканирования и ввода нового пароля.
* **📲 Установка как нативное приложение (PWA + Service Worker)**:
  * Поддержка Progressive Web App: установка на экран «Домой» в Safari (iOS) и Chrome (Android), работа в полноэкранном режиме и кэширование интерфейса.
* **🍏 Интеграция с Apple HomeKit**:
  * Поддержка нативного HomeKit через библиотеку `HomeSpan` или в 1 клик через мост Home Assistant. Доступно в приложении «Дом» на iPhone, iPad, Mac, Apple Watch и голосовое управление через Siri: *«Привет, Siri, какая температура в комнате?»*.
* **🏠 Home Assistant Auto-Discovery (MQTT)**:
  * Встроенный нулевой по зависимостям MQTT клиент v3.1.1. Моментальное обнаружение всех сенсоров (температура, влажность, точка росы, ощущаемая температура, абсолютная влажность, риск плесени, статус подсветки) без ручной правки YAML.
* **🗣️ Голосовое управление Яндекс Алисой**:
  * Бесшовное подключение к умному дому Яндекса: голосовые ответы Алисы на колонках Яндекс Станция и в приложении: *«В комнате 22.4 градуса, влажность в норме»*.
* **📱 Realtime Web-панель (SSE Multi-Client & Glassmorphism 2.0)**:
  * Потоковая передача данных без перезагрузки страниц (Server-Sent Events) с поддержкой до 4 параллельных клиентов.
  * 10 визуальных стилей оформления: от уютных интерьеров (🏡 Тёплый, 🌫️ Туман, 🪴 Оазис, 💛 Сканди, 🍸 Лаунж) до альтернативных стилей (🐍 VIPERR, 💿 Y2K Chrome, 🧊 DRAIN, 🌙 Midnight, ⚡ Cyberpunk).
  * Графики с сенсорным скраббером за 24 часа и мгновенный экспорт в Excel/CSV (`/api/export.csv`).
* **🤖 Потокобезопасный Telegram-бот (FreeRTOS Task)**:
  * Запросы к HTTPS Telegram API работают в отдельной задаче ядра с рекурсивным мьютексом `botMutex` — никаких фризов веб-интерфейса и SSE.
  * Экстренные оповещения и тихий режим Snooze (`/mute 1h`, `/unmute`).
* **💡 Аппаратная RGB-индикация (LEDC + CIE1931)**:
  * Аппаратный ШИМ 5 кГц с логарифмической коррекцией яркости по кривой чувствительности человеческого глаза CIE1931.
  * Автоматический тихий ночной режим по астрономическому времени NTP.

Полное руководство по настройке интеграций читайте в [docs/integrations-homekit-alice-ha.md](docs/integrations-homekit-alice-ha.md).

---

## 🔌 Схема подключения (ESP32-C3 SuperMini)

| Модуль | Пин модуля | Пин ESP32-C3 | Описание |
| :--- | :--- | :--- | :--- |
| **SHT30 / SHT31 (I2C)** | SDA | **GPIO 4** | Линия данных I2C |
| | SCL | **GPIO 5** | Линия тактирования I2C |
| | VCC / GND | **3.3V / GND** | Питание 3.3 В |
| **DHT11 (Резервный)** | DATA | **GPIO 0** | 1-Wire шина данных |
| | VCC / GND | **3.3V / GND** | Питание 3.3 В |
| **RGB LED (HW-479)** | R (Красный) | **GPIO 1** | Аппаратный ШИМ LEDC |
| | G (Зелёный) | **GPIO 2** | Аппаратный ШИМ LEDC |
| | B (Синий) | **GPIO 3** | Аппаратный ШИМ LEDC |
| | - (GND / Общий катод)| **GND** | Общий минус |

---

## 🚀 Быстрый старт

### 1. Подготовка окружения
1. Установите [Arduino IDE](https://www.arduino.cc/en/software) (версии 2.x).
2. Добавьте поддержку плат ESP32 в настройках (`Дополнительные ссылки для Менеджера плат`):
   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Установите библиотеки через **Менеджер библиотек**:
   * `ArduinoJson` (v6.x)
   * `UniversalTelegramBot` (v1.3.0)
   * `DHT sensor library` (от Adafruit)
   * `HomeSpan` *(опционально, для прямого Apple HomeKit)*

### 2. Настройка конфигурации
1. Скопируйте файл `config.example.h` в `config.h`:
   ```bash
   cp config.example.h config.h
   ```
2. Откройте `config.h` и укажите параметры вашей сети:
   ```cpp
   const char* ssid = "ИМЯ_ВАШЕГО_WIFI";
   const char* password = "ПАРОЛЬ_ОТ_WIFI";
   
   #define BOT_TOKEN "ВАШ_ТОКЕН_ИЗ_BOTFATHER"
   #define CHAT_ID   "ВАШ_CHAT_ID"
   ```

### 3. Прошивка
* **Плата**: `ESP32C3 Dev Module`
* **CPU Frequency**: `160MHz` (или `80MHz` для максимального охлаждения)
* **Flash Size**: `4MB (32Mb)`
* **Partition Scheme**: `Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)`
* **Upload Speed**: `921600`

После прошивки откройте в браузере: **`http://climat.local`** (или прямой IP-адрес из Serial Monitor).

---

## 📡 REST API, SSE & OTA Эндпоинты

| Маршрут | Метод | Описание |
| :--- | :--- | :--- |
| `/` | `GET` | Главный веб-интерфейс PWA (Gzip) |
| `/manifest.json` | `GET` | Манифест PWA-приложения |
| `/sw.js` | `GET` | Service Worker для оффлайн кэширования PWA |
| `/icon.svg` | `GET` | Векторная иконка приложения высокой четкости |
| `/api/events` | `GET` | Постоянный поток Server-Sent Events (SSE Realtime) |
| `/api/data` | `GET` | Текущие метрики, история, тренды и статус в JSON |
| `/api/save` | `POST` | Сохранение настроек комфорта, яркости и калибровки в NVS |
| `/api/update` | `POST` | Обновление прошивки по воздуху (Web OTA .bin) |
| `/api/export.csv` | `GET` | Скачивание 24-часовой истории в формате CSV |
| `/api/mute` | `POST` | Временное отключение критических алертов в Telegram на 1 час |
| `/api/unmute` | `POST` | Досрочное включение алертов |
| `/api/health` | `GET` | Диагностика памяти, аптайма, RSSI и состояния датчиков |

---

## 📄 Лицензия
Распространяется под лицензией MIT. Подробности в файле [LICENSE](LICENSE).
