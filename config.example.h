#pragma once

// ========== WI-FI ==========
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ========== TELEGRAM ==========
// Создайте бота в @BotFather и укажите токен ниже:
#define BOT_TOKEN "YOUR_TELEGRAM_BOT_TOKEN"
// Ваш Chat ID (можно узнать через @userinfobot):
#define CHAT_ID   "YOUR_TELEGRAM_CHAT_ID"

// ========== DHT / SHT SENSORS ==========
#define DHTPIN   0
#define DHTTYPE  DHT11

// I2C для SHT30 / SHT31 / BME280
#define I2C_SDA_PIN      4     // Стандартный SDA для ESP32-C3
#define I2C_SCL_PIN      5     // Стандартный SCL для ESP32-C3
#define SHT3X_I2C_ADDR   0x44  // Базовый I2C адрес SHT30/SHT31 (0x44 или 0x45)

// ========== MQTT & HOME ASSISTANT ==========
#define MQTT_ENABLED         true                  // Включить интеграцию с Home Assistant
#define MQTT_BROKER          "192.168.1.100"       // IP адрес брокера Home Assistant (Mosquitto)
#define MQTT_PORT            1883
#define MQTT_USER            ""                    // Логин (если требуется)
#define MQTT_PASS            ""                    // Пароль (если требуется)
#define MQTT_CLIENT_ID       "jarvis-esp32c3"
#define MQTT_TOPIC_PREFIX    "jarvis"
#define MQTT_PUB_INTERVAL    10000UL               // Публикация в HA каждые 10 секунд
#define MQTT_DISCOVERY_PREFIX "homeassistant"      // Префикс Home Assistant Auto-Discovery

// ========== WEBSERVER & SSE POOL ==========
#define MAX_SSE_CLIENTS      4                     // До 4 одновременных подключений (телефон, ПК, планшет)

// ========== RGB LED (HW-479) ==========
#define PIN_LED_R 1
#define PIN_LED_G 2
#define PIN_LED_B 3

// Аппаратный ШИМ LEDC
#define LEDC_PWM_FREQ       5000    // 5 кГц — полное отсутствие мерцания
#define LEDC_PWM_RESOLUTION 8       // 8 бит (0..255)

// ========== CALIBRATION OFFSETS ==========
#define DEFAULT_TEMP_OFFSET  0.0f   // Калибровка температуры (компенсация нагрева платы)
#define DEFAULT_HUM_OFFSET   0.0f   // Калибровка влажности
#define DEFAULT_MUTE_MS      3600000UL // 1 час заглушки по умолчанию

// ========== COMFORT DEFAULTS (ГОСТ 30494 / ВОЗ) ==========
#define DEFAULT_TEMP_MIN  20.0f  // Идеальный минимум для жилой комнаты
#define DEFAULT_TEMP_MAX  23.5f  // Оптимальный максимум для комфортного сна
#define DEFAULT_HUM_MIN   40.0f  // Норма влажности: защита слизистых
#define DEFAULT_HUM_MAX   60.0f  // Верхняя граница нормы: защита от сырости и плесени

// ========== CRITICAL ALERT THRESHOLDS ==========
#define ALERT_TEMP_HIGH  27.0f   // Критическая жара
#define ALERT_TEMP_LOW   17.5f   // Критическое похолодание
#define ALERT_HUM_LOW    30.0f   // Критически сухой воздух (тревога)
#define ALERT_HUM_HIGH   68.0f   // Критическая сырость (тревога)

// ========== NTP & TIMEZONE ==========
#define NTP_SERVER_1     "pool.ntp.org"
#define NTP_SERVER_2     "time.nist.gov"
#define GMT_OFFSET_SEC   (3 * 3600)  // UTC+3 (Москва)
#define DAYLIGHT_OFFSET  0

// ========== NIGHT MODE & BRIGHTNESS DEFAULTS ==========
#define DEFAULT_NIGHT_START_HOUR 23   // 23:00 начало тихого часа
#define DEFAULT_NIGHT_END_HOUR   7    // 07:00 утро
#define DEFAULT_DAY_BRIGHTNESS   80   // 80% приятная дневная яркость
#define DEFAULT_NIGHT_BRIGHTNESS 5    // 5% мягкий ночной режим

// ========== TIMING (ms) ==========
#define BOT_POLL_INTERVAL      2500UL       // Опрос Telegram в отдельной задаче FreeRTOS
#define AUTO_SEND_INTERVAL     14400000UL   // Авто-отчёт (4 часа)
#define ALERT_CHECK_INTERVAL   30000UL      // Проверка алертов
#define WIFI_CHECK_INTERVAL    10000UL      // Проверка Wi-Fi
#define SENSOR_READ_INTERVAL   2000UL       // Чтение датчика
#define STATS_RESET_INTERVAL   86400000UL   // Сброс суточных экстремумов

// ========== HISTORY ==========
#define HISTORY_SIZE      288             // 288 точек × 5 минут = полные 24 часа истории
#define HISTORY_INTERVAL  300000UL        // Каждые 5 минут

// ========== SHARED BUFFERS ==========
#define MSG_BUF_SIZE  512                 // Основной буфер сообщений
#define BIG_BUF_SIZE  1024                // Для полных отчётов

// ========== OTA ==========
#define OTA_HOSTNAME  "climat"
#define OTA_PASSWORD  "jarvis2026"

// ========== APPLE HOMEKIT (HOMESPAN) ==========
// Раскомментируйте строку ниже для включения нативной поддержки Apple HomeKit.
// Требуется установка библиотеки HomeSpan через Library Manager Arduino IDE.
// Код сопряжения HomeKit по умолчанию: 466-37-726
// #define ENABLE_HOMESPAN

// ========== WATCHDOG ==========
#define WDT_TIMEOUT_SEC  30
