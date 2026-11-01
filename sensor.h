#pragma once
#include <DHT.h>
#include <Wire.h>
#include <time.h>
#include <math.h>
#include "config.h"

// ========== ТАБЛИЦА ЛОГАРИФМИЧЕСКОЙ ГАММА-КОРРЕКЦИИ CIE1931 (256 ЗНАЧЕНИЙ) ==========
// Восприятие яркости глазом нелинейно: таблица делает диммирование плавным и естественным
static const uint8_t cie1931_lut[256] PROGMEM = {
    0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   4,   4,   4,   4,
    5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,  10,
   10,  10,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  18,
   18,  19,  20,  20,  21,  22,  22,  23,  24,  24,  25,  26,  27,  27,  28,  29,
   30,  31,  32,  33,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
   45,  46,  47,  48,  50,  51,  52,  53,  54,  56,  57,  58,  59,  61,  62,  63,
   65,  66,  67,  69,  70,  72,  73,  75,  76,  78,  79,  81,  82,  84,  86,  87,
   89,  91,  92,  94,  96,  98,  99, 101, 103, 105, 107, 109, 111, 113, 115, 117,
  119, 121, 123, 125, 127, 129, 132, 134, 136, 138, 141, 143, 145, 148, 150, 153,
  155, 158, 160, 163, 165, 168, 171, 173, 176, 179, 181, 184, 187, 190, 193, 196,
  199, 202, 205, 208, 211, 214, 217, 220, 223, 226, 230, 233, 236, 240, 243, 247,
  250, 254, 255
};

inline uint8_t applyCie1931(uint8_t val) {
    return pgm_read_byte(&cie1931_lut[val]);
}

// ========== ГЛОБАЛЬНОЕ СОСТОЯНИЕ ДАТЧИКА ==========

// ========== УНИВЕРСАЛЬНЫЙ ДРАЙВЕР ДАТЧИКОВ (DHT11 + SHT30/SHT31 I2C) ==========
enum SensorType {
    SENSOR_NONE = 0,
    SENSOR_DHT11,
    SENSOR_SHT3X
};

SensorType activeSensorType = SENSOR_NONE;
uint8_t activeShtAddress = SHT3X_I2C_ADDR;
DHT dht(DHTPIN, DHTTYPE);

// Вычисление контрольной суммы Sensirion CRC-8 (полином 0x31)
static uint8_t sht3xCrc8(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int j = len; j; --j) {
        crc ^= *data++;
        for (int i = 8; i; --i) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

// Прямой опрос Sensirion SHT30 / SHT31 по шине I2C без лишних библиотек
bool readSht3xDirect(float &t, float &h, uint8_t addr) {
    Wire.beginTransmission(addr);
    Wire.write(0x24); // High repeatability measurement
    Wire.write(0x00); // Clock stretching disabled
    if (Wire.endTransmission() != 0) return false;

    delay(20); // Время выполнения высокоточного измерения SHT3x (макс. 15 мс)

    if (Wire.requestFrom((int)addr, 6) != 6) return false;

    uint8_t buf[6];
    for (int i = 0; i < 6; i++) {
        buf[i] = Wire.read();
    }

    if (sht3xCrc8(buf, 2) != buf[2]) return false;
    if (sht3xCrc8(buf + 3, 2) != buf[5]) return false;

    uint16_t rawT = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t rawH = ((uint16_t)buf[3] << 8) | buf[4];

    t = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    h = 100.0f * ((float)rawH / 65535.0f);
    if (h < 0.0f) h = 0.0f;
    if (h > 100.0f) h = 100.0f;
    return true;
}

const char* getSensorModelName() {
    switch (activeSensorType) {
        case SENSOR_SHT3X: return "SHT30/31 (I2C)";
        case SENSOR_DHT11: return "DHT11 (1-Wire)";
        default:           return "Не определён";
    }
}

// Автоопределение и инициализация подключенного датчика
void initSensors() {
    Serial.println("[SENSOR] Инициализация универсального слоя датчиков...");
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000); // 100 кГц стандартная скорость I2C

    // Проверяем наличие SHT3x на адресах 0x44 или 0x45
    float testT = 0, testH = 0;
    if (readSht3xDirect(testT, testH, 0x44)) {
        activeSensorType = SENSOR_SHT3X;
        activeShtAddress = 0x44;
        Serial.printf("[SENSOR] ✅ Обнаружен Sensirion SHT3x (I2C: 0x44)! T=%.1f°C, H=%.1f%%\n", testT, testH);
        return;
    }
    if (readSht3xDirect(testT, testH, 0x45)) {
        activeSensorType = SENSOR_SHT3X;
        activeShtAddress = 0x45;
        Serial.printf("[SENSOR] ✅ Обнаружен Sensirion SHT3x (I2C: 0x45)! T=%.1f°C, H=%.1f%%\n", testT, testH);
        return;
    }

    // Если по I2C датчик не отозвался — переключаемся на DHT11
    activeSensorType = SENSOR_DHT11;
    dht.begin();
    Serial.printf("[SENSOR] ℹ️ I2C SHT3x не найден. Активирован DHT11 на GPIO %d.\n", DHTPIN);
}

float cachedTemp = NAN;
float cachedHum  = NAN;
unsigned long lastSensorRead = 0;

// Калибровочные смещения (NVS) — компенсация нагрева платы и погрешности
float tempOffset = DEFAULT_TEMP_OFFSET;
float humOffset  = DEFAULT_HUM_OFFSET;

// Интеллектуальный таймер подавления алертов (Snooze / Mute)
unsigned long alertMuteUntil = 0;

// Динамические границы комфорта (NVS)
float tempMin = DEFAULT_TEMP_MIN;
float tempMax = DEFAULT_TEMP_MAX;
float humMin  = DEFAULT_HUM_MIN;
float humMax  = DEFAULT_HUM_MAX;

// Настройки яркости и ночного режима (NVS)
int dayBrightness   = DEFAULT_DAY_BRIGHTNESS;   // 0..100%
int nightBrightness = DEFAULT_NIGHT_BRIGHTNESS; // 0..100%
int nightStartHour  = DEFAULT_NIGHT_START_HOUR; // 23
int nightEndHour    = DEFAULT_NIGHT_END_HOUR;   // 7

// Суточные экстремумы
float minTemp = 999.0f;
float maxTemp = -999.0f;
float minHum  = 999.0f;
float maxHum  = -999.0f;
unsigned long statsResetTime = 0;

// Кольцевой буфер истории (96 точек × 5 мин = 8 часов)
float tempHistory[HISTORY_SIZE];
float humHistory[HISTORY_SIZE];
int   historyIndex = 0;
bool  historyFull  = false;
unsigned long lastHistorySave = 0;
// Флаги оптимизации работы с памятью и SSE Delta Push
bool historyDirty = false;
bool historyUpdatedForSse = false;


// ========== МАТЕМАТИКА: ТОЧКА РОСЫ И HEAT INDEX ==========

float calculateDewPoint(float t, float h) {
    if (isnan(t) || isnan(h) || h <= 0.0f) return NAN;
    const float a = 17.27f;
    const float b = 237.7f;
    float alpha = ((a * t) / (b + t)) + log(h / 100.0f);
    return (b * alpha) / (a - alpha);
}

float calculateHeatIndex(float t, float h) {
    if (isnan(t) || isnan(h)) return NAN;
    if (t < 20.0f) return t;

    float tf = (t * 1.8f) + 32.0f;
    float hif = 0.5f * (tf + 61.0f + ((tf - 68.0f) * 1.2f) + (h * 0.094f));

    if (hif >= 80.0f) {
        float c1 = -42.379f;
        float c2 = 2.04901523f;
        float c3 = 10.14333127f;
        float c4 = -0.22475541f;
        float c5 = -0.00683783f;
        float c6 = -0.05481717f;
        float c7 = 0.00122874f;
        float c8 = 0.00085282f;
        float c9 = -0.00000199f;

        hif = c1 + (c2 * tf) + (c3 * h) + (c4 * tf * h) + (c5 * tf * tf) +
              (c6 * h * h) + (c7 * tf * tf * h) + (c8 * tf * h * h) + (c9 * tf * tf * h * h);
    }

    return (hif - 32.0f) / 1.8f;
}

const char* getDewPointDesc(float dp) {
    if (isnan(dp)) return "Неизвестно";
    if (dp < 10.0f) return "Сухо и прохладно";
    if (dp <= 15.0f) return "Идеальный комфорт";
    if (dp <= 18.0f) return "Приятно, чуть влажно";
    if (dp <= 21.0f) return "Душно, влажно";
    return "Очень душно, тяжело";
}

// Абсолютная влажность (г/м³ воздуха) по формуле Магнуса-Тетенса
float calculateAbsoluteHumidity(float t, float h) {
    if (isnan(t) || isnan(h) || h <= 0.0f) return NAN;
    float pW = (h / 100.0f) * 6.112f * exp((17.67f * t) / (t + 243.5f));
    return (216.7f * pW) / (273.15f + t);
}

// Оценка риска образования плесени на холодных поверхностях
const char* getMoldRiskLevel(float t, float h, float dp) {
    if (isnan(t) || isnan(h)) return "Н/Д";
    if (h >= 68.0f || (h >= 62.0f && dp >= 16.5f)) return "Высокий ⚠️";
    if (h >= 58.0f) return "Умеренный 🟡";
    return "Низкий ✅";
}

// Расчёт скорости и тренда изменения температуры и влажности за последний час
void calculateTrends(float &tDeltaHour, float &hDeltaHour, const char* &tTrendStr, const char* &hTrendStr) {
    int count = historyFull ? HISTORY_SIZE : historyIndex;
    if (count < 2) {
        tDeltaHour = 0.0f; hDeltaHour = 0.0f;
        tTrendStr = "→ стаб."; hTrendStr = "→ стаб.";
        return;
    }

    // 12 точек по 5 минут = 1 час назад (или максимально доступная глубина)
    int pts = min(12, count - 1);
    int currentIdx = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;
    int pastIdx = (currentIdx - pts + HISTORY_SIZE) % HISTORY_SIZE;

    float dt = tempHistory[currentIdx] - tempHistory[pastIdx];
    float dh = humHistory[currentIdx] - humHistory[pastIdx];
    float hours = (pts * 5.0f) / 60.0f;

    if (hours > 0.05f) {
        tDeltaHour = dt / hours;
        hDeltaHour = dh / hours;
    } else {
        tDeltaHour = 0.0f; hDeltaHour = 0.0f;
    }

    static char tBuf[24];
    static char hBuf[24];

    if (fabs(tDeltaHour) < 0.15f) {
        snprintf(tBuf, sizeof(tBuf), "→ стаб.");
    } else if (tDeltaHour > 0) {
        snprintf(tBuf, sizeof(tBuf), "↑ +%.1f°/ч", tDeltaHour);
    } else {
        snprintf(tBuf, sizeof(tBuf), "↓ %.1f°/ч", tDeltaHour);
    }

    if (fabs(hDeltaHour) < 0.8f) {
        snprintf(hBuf, sizeof(hBuf), "→ стаб.");
    } else if (hDeltaHour > 0) {
        snprintf(hBuf, sizeof(hBuf), "↑ +%.0f%%/ч", hDeltaHour);
    } else {
        snprintf(hBuf, sizeof(hBuf), "↓ %.0f%%/ч", hDeltaHour);
    }

    tTrendStr = tBuf;
    hTrendStr = hBuf;
}

// ========== ВРЕМЯ И НОЧНОЙ РЕЖИМ ==========

bool isNightTime() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    if (!localtime_r(&now, &timeinfo)) {
        return false;
    }

    int hour = timeinfo.tm_hour;
    if (nightStartHour > nightEndHour) {
        return (hour >= nightStartHour || hour < nightEndHour);
    } else {
        return (hour >= nightStartHour && hour < nightEndHour);
    }
}

int getCurrentBrightness() {
    return isNightTime() ? nightBrightness : dayBrightness;
}

void getFormattedTime(char* buf, size_t bufSize) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo)) {
        strftime(buf, bufSize, "%H:%M:%S", &timeinfo);
    } else {
        snprintf(buf, bufSize, "--:--:--");
    }
}

// ========== ЧТЕНИЕ ДАТЧИКА С КЕШИРОВАНИЕМ ==========

static uint8_t shtFailCount = 0;

void readSensorCached() {
    if (millis() - lastSensorRead < SENSOR_READ_INTERVAL) return;

    float rawT = NAN, rawH = NAN;

    if (activeSensorType == SENSOR_SHT3X) {
        if (readSht3xDirect(rawT, rawH, activeShtAddress)) {
            shtFailCount = 0;
        } else {
            shtFailCount++;
            if (shtFailCount >= 5) {
                // Если I2C перестал отвечать, пробуем резервный опрос DHT11
                rawT = dht.readTemperature();
                rawH = dht.readHumidity();
            }
        }
    } else {
        rawT = dht.readTemperature();
        rawH = dht.readHumidity();
    }

    if (!isnan(rawT)) {
        cachedTemp = rawT + tempOffset;
    }
    if (!isnan(rawH)) {
        cachedHum = constrain(rawH + humOffset, 0.0f, 100.0f);
    }

    lastSensorRead = millis();
}

inline bool sensorOk() {
    return !isnan(cachedTemp) && !isnan(cachedHum);
}

// ========== ИНИЦИАЛИЗАЦИЯ И УПРАВЛЕНИЕ АППАРАТНЫМ ШИМ LEDC ==========

void initLedcHardware() {
    // В ESP32 Arduino Core 3.x ledcAttach заменяет старый ledcSetup
    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        ledcAttach(PIN_LED_R, LEDC_PWM_FREQ, LEDC_PWM_RESOLUTION);
        ledcAttach(PIN_LED_G, LEDC_PWM_FREQ, LEDC_PWM_RESOLUTION);
        ledcAttach(PIN_LED_B, LEDC_PWM_FREQ, LEDC_PWM_RESOLUTION);
    #else
        ledcSetup(0, LEDC_PWM_FREQ, LEDC_PWM_RESOLUTION);
        ledcSetup(1, LEDC_PWM_FREQ, LEDC_PWM_RESOLUTION);
        ledcSetup(2, LEDC_PWM_FREQ, LEDC_PWM_RESOLUTION);
        ledcAttachPin(PIN_LED_R, 0);
        ledcAttachPin(PIN_LED_G, 1);
        ledcAttachPin(PIN_LED_B, 2);
    #endif
}

void setLedColor(int r, int g, int b) {
    int bFactor = getCurrentBrightness(); // 0..100%
    float scale = bFactor / 100.0f;

    // Линейное масштабирование яркости
    uint8_t scaledR = constrain((int)(r * scale), 0, 255);
    uint8_t scaledG = constrain((int)(g * scale), 0, 255);
    uint8_t scaledB = constrain((int)(b * scale), 0, 255);

    // Применение логарифмической гамма-коррекции CIE1931 для натурального свечения
    uint8_t finalR = applyCie1931(scaledR);
    uint8_t finalG = applyCie1931(scaledG);
    uint8_t finalB = applyCie1931(scaledB);

    #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
        ledcWrite(PIN_LED_R, finalR);
        ledcWrite(PIN_LED_G, finalG);
        ledcWrite(PIN_LED_B, finalB);
    #else
        ledcWrite(0, finalR);
        ledcWrite(1, finalG);
        ledcWrite(2, finalB);
    #endif
}

void updateLedClimate(float t) {
    if (isnan(t)) { setLedColor(0, 0, 0); return; }

    int r = 0, g = 0, b = 0;

    if (t <= ALERT_TEMP_LOW) {
        b = 255;
    } else if (t < tempMin) {
        float denom = tempMin - ALERT_TEMP_LOW;
        float ratio = (denom > 0.01f) ? constrain((t - ALERT_TEMP_LOW) / denom, 0.0f, 1.0f) : 0.0f;
        g = ratio * 255;
        b = (1.0f - ratio) * 255;
    } else if (t <= tempMax) {
        g = 255;
    } else if (t < ALERT_TEMP_HIGH) {
        float denom = ALERT_TEMP_HIGH - tempMax;
        float ratio = (denom > 0.01f) ? constrain((t - tempMax) / denom, 0.0f, 1.0f) : 0.0f;
        r = ratio * 255;
        g = (1.0f - ratio) * 255;
    } else {
        r = 255;
    }

    setLedColor(r, g, b);
}

// ========== СТАТИСТИКА & ИСТОРИЯ ==========

void updateStats(float t, float h) {
    if (isnan(t) || isnan(h)) return;

    if (t < minTemp) minTemp = t;
    if (t > maxTemp) maxTemp = t;
    if (h < minHum)  minHum  = h;
    if (h > maxHum)  maxHum  = h;

    if (millis() - statsResetTime > STATS_RESET_INTERVAL) {
        minTemp = t; maxTemp = t;
        minHum  = h; maxHum  = h;
        statsResetTime = millis();
    }

    if (millis() - lastHistorySave > HISTORY_INTERVAL || lastHistorySave == 0) {
        tempHistory[historyIndex] = t;
        humHistory[historyIndex]  = h;
        historyIndex++;
        if (historyIndex >= HISTORY_SIZE) {
            historyIndex = 0;
            historyFull = true;
        }
        lastHistorySave = millis();
        historyDirty = true;
        historyUpdatedForSse = true;
    }
}

// NVS: загрузка / сохранение порогов, яркости, калибровки и истории
void loadSettings(Preferences &prefs) {
    prefs.begin("climate", true);
    tempMin = prefs.getFloat("tMin", DEFAULT_TEMP_MIN);
    tempMax = prefs.getFloat("tMax", DEFAULT_TEMP_MAX);
    humMin  = prefs.getFloat("hMin", DEFAULT_HUM_MIN);
    humMax  = prefs.getFloat("hMax", DEFAULT_HUM_MAX);

    dayBrightness   = prefs.getInt("bDay", DEFAULT_DAY_BRIGHTNESS);
    nightBrightness = prefs.getInt("bNight", DEFAULT_NIGHT_BRIGHTNESS);
    nightStartHour  = prefs.getInt("nStart", DEFAULT_NIGHT_START_HOUR);
    nightEndHour    = prefs.getInt("nEnd", DEFAULT_NIGHT_END_HOUR);

    tempOffset      = prefs.getFloat("tOff", DEFAULT_TEMP_OFFSET);
    humOffset       = prefs.getFloat("hOff", DEFAULT_HUM_OFFSET);
    prefs.end();
}

void saveSettings(Preferences &prefs, float tMin, float tMax, float hMin, float hMax, 
                  int bDay, int bNight, int nStart, int nEnd,
                  float tOff = DEFAULT_TEMP_OFFSET, float hOff = DEFAULT_HUM_OFFSET) {
    prefs.begin("climate", false);
    prefs.putFloat("tMin", tMin);
    prefs.putFloat("tMax", tMax);
    prefs.putFloat("hMin", hMin);
    prefs.putFloat("hMax", hMax);

    prefs.putInt("bDay", bDay);
    prefs.putInt("bNight", bNight);
    prefs.putInt("nStart", nStart);
    prefs.putInt("nEnd", nEnd);

    prefs.putFloat("tOff", tOff);
    prefs.putFloat("hOff", hOff);
    prefs.end();

    tempMin = tMin; tempMax = tMax;
    humMin  = hMin; humMax  = hMax;
    dayBrightness   = bDay;
    nightBrightness = bNight;
    nightStartHour  = nStart;
    nightEndHour    = nEnd;
    tempOffset      = tOff;
    humOffset       = hOff;
}

void loadHistory(Preferences &prefs) {
    prefs.begin("hist", true);
    if (prefs.isKey("hIdx")) {
        prefs.getBytes("tH", tempHistory, sizeof(tempHistory));
        prefs.getBytes("hH", humHistory, sizeof(humHistory));
        historyIndex = prefs.getInt("hIdx", 0);
        historyFull  = prefs.getBool("hFull", false);
    }
    prefs.end();
}

void saveHistory(Preferences &prefs) {
    prefs.begin("hist", false);
    prefs.putBytes("tH", tempHistory, sizeof(tempHistory));
    prefs.putBytes("hH", humHistory, sizeof(humHistory));
    prefs.putInt("hIdx", historyIndex);
    prefs.putBool("hFull", historyFull);
    prefs.end();
}

void processSensorData() {
    if (!sensorOk()) return;
    updateStats(cachedTemp, cachedHum);
    updateLedClimate(cachedTemp);
}
