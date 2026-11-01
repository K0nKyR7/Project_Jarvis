# 🌟 Project Jarvis — Умный мониторинг микроклимата v2.4 Hardened

[![ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3%20SuperMini-blue.svg)](https://espressif.com)
[![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20Multitasking-green.svg)](https://freertos.org)
[![Home Assistant](https://img.shields.io/badge/Integration-Home%20Assistant%20MQTT-41bdf5.svg)](https://www.home-assistant.io)
[![PWA](https://img.shields.io/badge/Web-PWA%20Glassmorphism-ff69b4.svg)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Автономная инженерная станция мониторинга качества воздуха и микроклимата на базе микроконтроллера **ESP32-C3 SuperMini**. 

Проект сочетает в себе **встроенный PWA-дашборд с 10 дизайнерскими темами** в стиле Glassmorphism, **многопоточного Telegram-бота** на базе FreeRTOS с защитой мьютексами, бесшовную **интеграцию с Home Assistant через MQTT Auto-Discovery** и **аппаратный ШИМ 5 кГц с гамма-коррекцией CIE1931** для визуальной индикации комфорта.

---

## ⚡ Ключевые возможности

* **📱 Realtime PWA Web-панель (SSE Multi-Client)**:
  * Потоковая передача данных без перезагрузки страниц (Server-Sent Events) с поддержкой до 4 параллельных клиентов.
  * 10 визуальных стилей оформления: от теплых интерьеров (🏡 Тёплый, 🌫️ Туман, 🪴 Оазис, 💛 Сканди, 🍸 Лаунж) до альтернативных стилей (🐍 VIPERR, 💿 Y2K Chrome, 🧊 DRAIN, 🌙 Midnight, ⚡ Cyberpunk).
  * 24-часовые интерактивные графики с сенсорным скраббером (управление пальцем на смартфонах и планшетах).
  * Скачивание суточного архива в Excel/CSV (`/api/export.csv`) в один клик.
* **🤖 Потокобезопасный Telegram-бот (FreeRTOS Task)**:
  * Запросы к HTTPS Telegram API работают в отдельной задаче ядра с рекурсивным мьютексом `botMutex` — никаких фризов веб-интерфейса.
  * Экстренные оповещения при выходе за границы нормы (жара, холод, сухость, сырость).
  * Интеллектуальный Snooze: команды `/mute 1h`, `/mute 2h`, `/unmute` для временного отключения уведомлений при проветривании.
* **🏠 Home Assistant MQTT Auto-Discovery**:
  * Встроенный легковесный клиент MQTT v3.1.1 (работает напрямую через сокет, без ограничений буфера `PubSubClient`).
  * Датчики температуры, влажности, точки росы, ощущаемой температуры и управление подсветкой сами появляются в Home Assistant без ручной правки YAML.
* **🔌 Универсальный драйвер датчиков**:
  * Автоматическое сканирование шины I2C (GPIO 4 / GPIO 5) на старте. Если найден Sensirion **SHT30/SHT31** — опрос с проверкой CRC-8; если нет — прозрачный фоллбек на **DHT11**.
* **💡 Аппаратная RGB-индикация (LEDC + CIE1931)**:
  * Аппаратный ШИМ 5 кГц с логарифмической коррекцией яркости по кривой чувствительности человеческого глаза CIE1931.
  * Автоматический тихий ночной режим по астрономическому времени NTP.
* **🎯 Калибровка датчиков**:
  * Возможность программной компенсации самонагрева ESP32 прямо из веб-панели (смещения $\pm 5.0^\circ\text{C}$ и $\pm 15\%$, сохраняемые в NVS).

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
* **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)` или `Default 4MB with ffat`
* **Upload Speed**: `921600`

После прошивки откройте в браузере: **`http://climat.local`** (или прямой IP-адрес из Serial Monitor).

---

## 📡 REST API & SSE Эндпоинты

| Маршрут | Метод | Описание |
| :--- | :--- | :--- |
| `/` | `GET` | Главный веб-интерфейс PWA (Gzip) |
| `/api/events` | `GET` | Постоянный поток Server-Sent Events (SSE Realtime) |
| `/api/data` | `GET` | Текущие метрики, история, тренды и статус в JSON |
| `/api/save` | `POST` | Сохранение настроек комфорта, яркости и калибровки в NVS |
| `/api/export.csv` | `GET` | Скачивание 24-часовой истории в формате CSV |
| `/api/mute` | `POST` | Временное отключение критических алертов в Telegram на 1 час |
| `/api/unmute` | `POST` | Досрочное включение алертов |
| `/api/health` | `GET` | Диагностика памяти, аптайма, RSSI и состояния датчиков |

---

## 📄 Лицензия
Распространяется под лицензией MIT. Подробности в файле [LICENSE](LICENSE).
