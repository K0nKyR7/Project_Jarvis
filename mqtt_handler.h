#pragma once
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor.h"

// ========== ZERO-DEPENDENCY MQTT v3.1.1 CLIENT & HOME ASSISTANT DISCOVERY ==========
// Работает напрямую через WiFiClient без сторонних библиотек и ограничений буфера PubSubClient.

static WiFiClient mqttNetClient;
static bool mqttConnected = false;
static bool mqttDiscoveryPublished = false;
static unsigned long lastMqttAttempt = 0;
static unsigned long lastMqttPub = 0;
static unsigned long lastMqttPing = 0;

// Кодирование переменной длины MQTT (Variable Length Remaining Length)
static int mqttEncodeLength(uint8_t *buf, uint32_t len) {
    int idx = 0;
    do {
        uint8_t d = len % 128;
        len /= 128;
        if (len > 0) d |= 0x80;
        buf[idx++] = d;
    } while (len > 0);
    return idx;
}

// Отправка MQTT PUBLISH (QoS 0)
static bool mqttPublish(const char* topic, const char* payload, bool retain = false) {
    if (!mqttNetClient.connected()) return false;

    uint16_t topicLen = strlen(topic);
    uint32_t payloadLen = strlen(payload);
    uint32_t remLen = 2 + topicLen + payloadLen;

    uint8_t header[10];
    header[0] = 0x30 | (retain ? 0x01 : 0x00); // 0x30 = PUBLISH QoS 0
    int lenBytes = mqttEncodeLength(&header[1], remLen);

    // Заголовок пакета
    mqttNetClient.write(header, 1 + lenBytes);

    // Топик
    uint8_t tLenBuf[2] = { (uint8_t)(topicLen >> 8), (uint8_t)(topicLen & 0xFF) };
    mqttNetClient.write(tLenBuf, 2);
    mqttNetClient.write((const uint8_t*)topic, topicLen);

    // Данные
    mqttNetClient.write((const uint8_t*)payload, payloadLen);
    return true;
}

// Подписка на топик управления
static void mqttSubscribe(const char* topic) {
    if (!mqttNetClient.connected()) return;

    uint16_t topicLen = strlen(topic);
    uint32_t remLen = 2 + 2 + topicLen + 1; // Packet ID (2) + Topic Len (2) + Topic + QoS (1)

    uint8_t buf[128];
    buf[0] = 0x82; // SUBSCRIBE
    int lenBytes = mqttEncodeLength(&buf[1], remLen);
    int pos = 1 + lenBytes;

    buf[pos++] = 0x00; // Packet ID MSB
    buf[pos++] = 0x01; // Packet ID LSB

    buf[pos++] = (uint8_t)(topicLen >> 8);
    buf[pos++] = (uint8_t)(topicLen & 0xFF);
    memcpy(&buf[pos], topic, topicLen);
    pos += topicLen;

    buf[pos++] = 0x00; // Requested QoS 0

    mqttNetClient.write(buf, pos);
}

// Отправка пакета CONNECT
static bool mqttSendConnect() {
    uint8_t buf[256];
    const char* cid = MQTT_CLIENT_ID;
    uint16_t cidLen = strlen(cid);

    const char* user = MQTT_USER;
    const char* pass = MQTT_PASS;
    bool hasUser = (strlen(user) > 0);
    bool hasPass = (strlen(pass) > 0);

    uint8_t connectFlags = 0x02; // Clean Session
    if (hasUser) connectFlags |= 0x80;
    if (hasPass) connectFlags |= 0x40;

    uint32_t remLen = 10 + 2 + cidLen;
    if (hasUser) remLen += 2 + strlen(user);
    if (hasPass) remLen += 2 + strlen(pass);

    buf[0] = 0x10; // CONNECT
    int lenBytes = mqttEncodeLength(&buf[1], remLen);
    int pos = 1 + lenBytes;

    // Protocol Name "MQTT"
    buf[pos++] = 0x00; buf[pos++] = 0x04;
    buf[pos++] = 'M'; buf[pos++] = 'Q'; buf[pos++] = 'T'; buf[pos++] = 'T';
    buf[pos++] = 0x04; // Protocol Level 4 (v3.1.1)
    buf[pos++] = connectFlags;
    buf[pos++] = 0x00; buf[pos++] = 0x3C; // Keepalive: 60 сек

    // Client ID
    buf[pos++] = (uint8_t)(cidLen >> 8);
    buf[pos++] = (uint8_t)(cidLen & 0xFF);
    memcpy(&buf[pos], cid, cidLen);
    pos += cidLen;

    if (hasUser) {
        uint16_t uLen = strlen(user);
        buf[pos++] = (uint8_t)(uLen >> 8); buf[pos++] = (uint8_t)(uLen & 0xFF);
        memcpy(&buf[pos], user, uLen);
        pos += uLen;
    }
    if (hasPass) {
        uint16_t pLen = strlen(pass);
        buf[pos++] = (uint8_t)(pLen >> 8); buf[pos++] = (uint8_t)(pLen & 0xFF);
        memcpy(&buf[pos], pass, pLen);
        pos += pLen;
    }

    mqttNetClient.write(buf, pos);

    // Ждём CONNACK (4 байта: 0x20, 0x02, 0x00, 0x00)
    unsigned long waitStart = millis();
    while (mqttNetClient.available() < 4 && (millis() - waitStart < 2000)) {
        delay(10);
    }

    if (mqttNetClient.available() >= 4) {
        uint8_t c1 = mqttNetClient.read();
        uint8_t c2 = mqttNetClient.read();
        uint8_t c3 = mqttNetClient.read();
        uint8_t c4 = mqttNetClient.read();
        if (c1 == 0x20 && c4 == 0x00) {
            return true;
        }
    }
    return false;
}

// Публикация Home Assistant MQTT Auto-Discovery
static void mqttPublishDiscovery() {
    Serial.println("[MQTT] Регистрация сущностей в Home Assistant Auto-Discovery...");

    const char* devInfo = "{\"identifiers\":[\"jarvis_esp32c3\"],"
                          "\"name\":\"Jarvis Climate Monitor\","
                          "\"model\":\"ESP32-C3 SuperMini\","
                          "\"manufacturer\":\"Jarvis Project\","
                          "\"sw_version\":\"2.4\"}";

    // 1. Датчик температуры
    char discT[512];
    snprintf(discT, sizeof(discT),
        "{\"name\":\"Температура\","
        "\"unique_id\":\"jarvis_temperature\","
        "\"state_topic\":\"jarvis/state\","
        "\"unit_of_measurement\":\"°C\","
        "\"device_class\":\"temperature\","
        "\"state_class\":\"measurement\","
        "\"value_template\":\"{{ value_json.temperature }}\","
        "\"device\":%s}", devInfo);
    mqttPublish("homeassistant/sensor/jarvis_esp32/temperature/config", discT, true);

    // 2. Датчик влажности
    char discH[512];
    snprintf(discH, sizeof(discH),
        "{\"name\":\"Влажность\","
        "\"unique_id\":\"jarvis_humidity\","
        "\"state_topic\":\"jarvis/state\","
        "\"unit_of_measurement\":\"%%\","
        "\"device_class\":\"humidity\","
        "\"state_class\":\"measurement\","
        "\"value_template\":\"{{ value_json.humidity }}\","
        "\"device\":%s}", devInfo);
    mqttPublish("homeassistant/sensor/jarvis_esp32/humidity/config", discH, true);

    // 3. Точка росы
    char discDP[512];
    snprintf(discDP, sizeof(discDP),
        "{\"name\":\"Точка росы\","
        "\"unique_id\":\"jarvis_dew_point\","
        "\"state_topic\":\"jarvis/state\","
        "\"unit_of_measurement\":\"°C\","
        "\"device_class\":\"temperature\","
        "\"value_template\":\"{{ value_json.dew_point }}\","
        "\"device\":%s}", devInfo);
    mqttPublish("homeassistant/sensor/jarvis_esp32/dew_point/config", discDP, true);

    // 4. Ощущаемая температура (Heat Index)
    char discHI[512];
    snprintf(discHI, sizeof(discHI),
        "{\"name\":\"Ощущается как\","
        "\"unique_id\":\"jarvis_heat_index\","
        "\"state_topic\":\"jarvis/state\","
        "\"unit_of_measurement\":\"°C\","
        "\"device_class\":\"temperature\","
        "\"value_template\":\"{{ value_json.heat_index }}\","
        "\"device\":%s}", devInfo);
    mqttPublish("homeassistant/sensor/jarvis_esp32/heat_index/config", discHI, true);

    // 5. Абсолютная влажность (г/м³)
    char discAH[512];
    snprintf(discAH, sizeof(discAH),
        "{\"name\":\"Абсолютная влажность\","
        "\"unique_id\":\"jarvis_abs_humidity\","
        "\"state_topic\":\"jarvis/state\","
        "\"unit_of_measurement\":\"г/м³\","
        "\"state_class\":\"measurement\","
        "\"icon\":\"mdi:water-percent\","
        "\"value_template\":\"{{ value_json.abs_humidity }}\",\""
        "device\":%s}", devInfo);
    mqttPublish("homeassistant/sensor/jarvis_esp32/abs_humidity/config", discAH, true);

    // 6. Риск плесени
    char discMold[512];
    snprintf(discMold, sizeof(discMold),
        "{\"name\":\"Риск плесени\","
        "\"unique_id\":\"jarvis_mold_risk\","
        "\"state_topic\":\"jarvis/state\","
        "\"icon\":\"mdi:shield-alert-outline\","
        "\"value_template\":\"{{ value_json.mold_risk }}\",\""
        "device\":%s}", devInfo);
    mqttPublish("homeassistant/sensor/jarvis_esp32/mold_risk/config", discMold, true);

    // 7. RGB Светодиод / Ночной режим
    char discLED[512];
    snprintf(discLED, sizeof(discLED),
        "{\"name\":\"Индикатор климата\","
        "\"unique_id\":\"jarvis_climate_led\","
        "\"state_topic\":\"jarvis/led/state\","
        "\"command_topic\":\"jarvis/led/set\","
        "\"brightness_state_topic\":\"jarvis/led/brightness\","
        "\"brightness_command_topic\":\"jarvis/led/brightness/set\","
        "\"brightness_scale\":100,"
        "\"device\":%s}", devInfo);
    mqttPublish("homeassistant/light/jarvis_esp32/led/config", discLED, true);

    Serial.println("[MQTT] ✅ Home Assistant Auto-Discovery успешно отправлен!");
}

// Публикация текущих значений в Home Assistant
static void mqttPublishState() {
    if (!mqttNetClient.connected()) return;

    float dp = calculateDewPoint(cachedTemp, cachedHum);
    float hi = calculateHeatIndex(cachedTemp, cachedHum);
    float ah = calculateAbsoluteHumidity(cachedTemp, cachedHum);
    const char* mold = getMoldRiskLevel(cachedTemp, cachedHum, dp);

    char stateJson[512];
    snprintf(stateJson, sizeof(stateJson),
        "{\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"dew_point\":%.1f,"
        "\"heat_index\":%.1f,"
        "\"abs_humidity\":%.1f,"
        "\"mold_risk\":\"%s\","
        "\"brightness\":%d,"
        "\"is_night\":%s,"
        "\"sensor\":\"%s\","
        "\"rssi\":%ld,"
        "\"uptime\":%lu}",
        isnan(cachedTemp) ? 0.0f : cachedTemp,
        isnan(cachedHum) ? 0.0f : cachedHum,
        isnan(dp) ? 0.0f : dp,
        isnan(hi) ? 0.0f : hi,
        isnan(ah) ? 0.0f : ah,
        mold,
        isnan(cachedTemp) ? 0.0f : cachedTemp,
        isnan(cachedHum) ? 0.0f : cachedHum,
        isnan(dp) ? 0.0f : dp,
        isnan(hi) ? 0.0f : hi,
        getCurrentBrightness(),
        isNightTime() ? "true" : "false",
        getSensorModelName(),
        WiFi.RSSI(),
        millis() / 60000UL);

    mqttPublish("jarvis/state", stateJson, false);

    // Состояние светодиода
    mqttPublish("jarvis/led/state", (getCurrentBrightness() > 0) ? "ON" : "OFF", false);
    char bStr[8];
    snprintf(bStr, sizeof(bStr), "%d", getCurrentBrightness());
    mqttPublish("jarvis/led/brightness", bStr, false);
}

// Разбор входящих команд от Home Assistant
static void mqttHandleIncoming() {
    while (mqttNetClient.available() > 0) {
        uint8_t header = mqttNetClient.read();
        uint8_t msgType = (header >> 4) & 0x0F;

        // Декодируем Remaining Length
        uint32_t remLen = 0;
        uint32_t multiplier = 1;
        uint8_t encodedByte;
        do {
            if (!mqttNetClient.available()) break;
            encodedByte = mqttNetClient.read();
            remLen += (encodedByte & 127) * multiplier;
            multiplier *= 128;
        } while ((encodedByte & 128) != 0);

        if (msgType == 3) { // PUBLISH
            if (remLen < 2) continue;
            uint8_t t1 = mqttNetClient.read();
            uint8_t t2 = mqttNetClient.read();
            uint16_t topicLen = ((uint16_t)t1 << 8) | t2;

            char topic[64] = "";
            int readTopic = min((int)topicLen, (int)sizeof(topic) - 1);
            for (int i = 0; i < topicLen; i++) {
                char c = mqttNetClient.read();
                if (i < readTopic) topic[i] = c;
            }
            topic[readTopic] = '\0';

            int payloadLen = remLen - 2 - topicLen;
            char payload[64] = "";
            int readPayload = min(payloadLen, (int)sizeof(payload) - 1);
            for (int i = 0; i < payloadLen; i++) {
                char c = mqttNetClient.read();
                if (i < readPayload) payload[i] = c;
            }
            payload[readPayload] = '\0';

            // Обработка команд светодиода от Home Assistant
            if (strcmp(topic, "jarvis/led/set") == 0) {
                if (strcmp(payload, "OFF") == 0) {
                    setLedColor(0, 0, 0);
                } else if (strcmp(payload, "ON") == 0) {
                    if (!isnan(cachedTemp)) updateLedClimate(cachedTemp);
                }
            } else if (strcmp(topic, "jarvis/led/brightness/set") == 0) {
                int val = atoi(payload);
                if (val >= 0 && val <= 100) {
                    dayBrightness = val;
                    if (!isnan(cachedTemp)) updateLedClimate(cachedTemp);
                }
            }
        } else if (msgType == 13) { // PINGRESP
            // Живой пинг подтвержден
        } else {
            // Пропускаем оставшиеся байты пакета
            while (remLen > 0 && mqttNetClient.available()) {
                mqttNetClient.read();
                remLen--;
            }
        }
    }
}

// Главный неблокирующий цикл MQTT
void mqttLoop() {
    if (!MQTT_ENABLED) return;
    if (WiFi.status() != WL_CONNECTED) {
        mqttConnected = false;
        return;
    }

    // Если связь с брокером разорвана — неблокирующая попытка переподключения раз в 15 сек
    if (!mqttNetClient.connected()) {
        mqttConnected = false;
        mqttDiscoveryPublished = false;

        if (millis() - lastMqttAttempt >= 15000UL) {
            lastMqttAttempt = millis();
            Serial.printf("[MQTT] Подключение к брокеру %s:%d...\n", MQTT_BROKER, MQTT_PORT);
            if (mqttNetClient.connect(MQTT_BROKER, MQTT_PORT, 1500)) {
                if (mqttSendConnect()) {
                    mqttConnected = true;
                    Serial.println("[MQTT] ✅ Успешно подключено к брокеру!");

                    // Подписываемся на команды управления
                    mqttSubscribe("jarvis/led/set");
                    mqttSubscribe("jarvis/led/brightness/set");

                    // Анонс в Home Assistant
                    mqttPublishDiscovery();
                    mqttDiscoveryPublished = true;

                    // Мгновенная публикация первого состояния
                    mqttPublishState();
                    lastMqttPub = millis();
                    lastMqttPing = millis();
                } else {
                    mqttNetClient.stop();
                    Serial.println("[MQTT] ❌ Ошибка рукопожатия CONNECT");
                }
            } else {
                Serial.println("[MQTT] ℹ️ Брокер пока недоступен (повтор через 15с)");
            }
        }
        return;
    }

    // Обработка входящих команд
    mqttHandleIncoming();

    // Регулярная публикация телеметрии
    if (millis() - lastMqttPub >= MQTT_PUB_INTERVAL) {
        mqttPublishState();
        lastMqttPub = millis();
    }

    // Keepalive Ping каждые 30 секунд
    if (millis() - lastMqttPing >= 30000UL) {
        uint8_t pingPacket[2] = { 0xC0, 0x00 }; // PINGREQ
        mqttNetClient.write(pingPacket, 2);
        lastMqttPing = millis();
    }
}

bool isMqttConnected() {
    return mqttConnected && mqttNetClient.connected();
}
