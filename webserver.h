#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "web_html_gz.h"
#include "themes/cozy_jpg.h"
#include "themes/fog_jpg.h"
#include "themes/oasis_jpg.h"
#include "themes/nordic_jpg.h"
#include "themes/dining_jpg.h"
#include "themes/viperr_jpg.h"
#include "themes/y2k_jpg.h"
#include "themes/drain_jpg.h"

// ========== REALTIME MULTI-CLIENT SSE POOL ==========
WiFiClient sseClients[MAX_SSE_CLIENTS];
unsigned long lastSsePush = 0;
const unsigned long sseInterval = 2000UL; // Отправка событий каждые 2 секунды

int getActiveSseCount() {
    int count = 0;
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (sseClients[i] && sseClients[i].connected()) count++;
    }
    return count;
}

void broadcastSse(const String &payload) {
    String msg = "data: " + payload + "\n\n";
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (sseClients[i]) {
            if (sseClients[i].connected()) {
                sseClients[i].print(msg);
            } else {
                sseClients[i].stop();
            }
        }
    }
}

// ========== HTML (gzip) & ASSETS ==========

void handleRoot(WebServer &server) {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html", (const char*)index_html_gz, index_html_gz_len);
}

void handleCozyImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)cozy_jpg, cozy_jpg_len);
}

void handleFogImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)fog_jpg, fog_jpg_len);
}

void handleOasisImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)oasis_jpg, oasis_jpg_len);
}

void handleNordicImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)nordic_jpg, nordic_jpg_len);
}

void handleDiningImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)dining_jpg, dining_jpg_len);
}

void handleViperrImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)viperr_jpg, viperr_jpg_len);
}

void handleY2kImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)y2k_jpg, y2k_jpg_len);
}

void handleDrainImage(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, "image/jpeg", (const char*)drain_jpg, drain_jpg_len);
}

// ========== PWA MANIFEST ==========

void handleManifest(WebServer &server) {
    static const char manifest[] PROGMEM = R"rawliteral({
  "name": "Климат Квартира",
  "short_name": "Климат",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#090a0d",
  "theme_color": "#121318"
})rawliteral";
    server.send(200, "application/json", manifest);
}

// ========== СБОРКА JSON ДАННЫХ (ОБЩАЯ) ==========

void buildDataJson(String &output) {
    StaticJsonDocument<2048> doc;
    doc["t"] = isnan(cachedTemp) ? 0.0f : cachedTemp;
    doc["h"] = isnan(cachedHum)  ? 0.0f : cachedHum;
    doc["minT"] = minTemp > 900  ? cachedTemp : minTemp;
    doc["maxT"] = maxTemp < -900 ? cachedTemp : maxTemp;
    doc["minH"] = minHum  > 900  ? cachedHum  : minHum;
    doc["maxH"] = maxHum  < -900 ? cachedHum  : maxHum;
    doc["rssi"]   = WiFi.RSSI();
    doc["uptime"]  = millis() / 60000UL;

    char timeStr[16];
    getFormattedTime(timeStr, sizeof(timeStr));
    doc["time"] = timeStr;

    float dp = calculateDewPoint(cachedTemp, cachedHum);
    float hi = calculateHeatIndex(cachedTemp, cachedHum);
    float ah = calculateAbsoluteHumidity(cachedTemp, cachedHum);
    const char* mold = getMoldRiskLevel(cachedTemp, cachedHum, dp);

    float tDelta = 0, hDelta = 0;
    const char* tTrend = "";
    const char* hTrend = "";
    calculateTrends(tDelta, hDelta, tTrend, hTrend);

    doc["dewPoint"]  = isnan(dp) ? 0.0f : dp;
    doc["heatIndex"] = isnan(hi) ? 0.0f : hi;
    doc["dewDesc"]   = getDewPointDesc(dp);
    doc["absHum"]    = isnan(ah) ? 0.0f : (round(ah * 10.0f) / 10.0f);
    doc["moldRisk"]  = mold;
    doc["tTrend"]    = tTrend;
    doc["hTrend"]    = hTrend;
    doc["tDelta"]    = round(tDelta * 10.0f) / 10.0f;
    doc["hDelta"]    = round(hDelta * 10.0f) / 10.0f;

    doc["tOff"]      = tempOffset;
    doc["hOff"]      = humOffset;
    doc["isMuted"]   = (millis() < alertMuteUntil);

    doc["cfgTMin"] = tempMin;
    doc["cfgTMax"] = tempMax;
    doc["cfgHMin"] = humMin;
    doc["cfgHMax"] = humMax;

    doc["bDay"]    = dayBrightness;
    doc["bNight"]  = nightBrightness;
    doc["nStart"]  = nightStartHour;
    doc["nEnd"]    = nightEndHour;
    doc["isNight"] = isNightTime();
    doc["curB"]    = getCurrentBrightness();

    doc["sensor"]  = getSensorModelName();
    doc["sseCount"] = getActiveSseCount();

    JsonArray tArr = doc.createNestedArray("tHist");
    JsonArray hArr = doc.createNestedArray("hHist");

    // Интеллектуальное прореживание для графика 24ч (держит JSON легким и быстрым)
    int count = historyFull ? HISTORY_SIZE : historyIndex;
    int start = historyFull ? historyIndex : 0;
    int step = (count > 96) ? (count / 96) : 1;
    for (int i = 0; i < count; i += step) {
        int idx = (start + i) % HISTORY_SIZE;
        tArr.add(round(tempHistory[idx] * 10.0f) / 10.0f);
        hArr.add(round(humHistory[idx] * 10.0f) / 10.0f);
    }

    output.reserve(measureJson(doc) + 1);
    serializeJson(doc, output);
}

// ========== API: DATA (REST FALLBACK) ==========

void handleApiData(WebServer &server) {
    String json;
    buildDataJson(json);
    server.send(200, "application/json", json);
}

// ========== API: EVENTS (SERVER-SENT EVENTS STREAM) ==========

void handleApiEvents(WebServer &server) {
    WiFiClient client = server.client();

    // Заголовки постоянного потока SSE
    server.sendContent("HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/event-stream\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Connection: keep-alive\r\n"
                       "Access-Control-Allow-Origin: *\r\n\r\n");

    // Отправляем первое мгновенное событие
    String initialJson;
    buildDataJson(initialJson);
    server.sendContent("data: " + initialJson + "\n\n");

    // Регистрация в пуле SSE
    int targetSlot = -1;
    for (int i = 0; i < MAX_SSE_CLIENTS; i++) {
        if (!sseClients[i] || !sseClients[i].connected()) {
            targetSlot = i;
            break;
        }
    }

    // Если все слоты заняты, освобождаем самый старый
    if (targetSlot == -1) {
        if (sseClients[0]) sseClients[0].stop();
        targetSlot = 0;
    }

    sseClients[targetSlot] = client;
    lastSsePush = millis();
}

// Фоновая отправка SSE-событий во все активные соединения пула
void handleSseLoop() {
    if (millis() - lastSsePush < sseInterval) return;

    if (getActiveSseCount() == 0) return;

    String json;
    buildDataJson(json);
    broadcastSse(json);
    lastSsePush = millis();
}

// ========== API: SAVE SETTINGS ==========

void handleApiSave(WebServer &server, Preferences &prefs) {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    StaticJsonDocument<384> doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    float tMin = doc["tMin"] | tempMin;
    float tMax = doc["tMax"] | tempMax;
    float hMin = doc["hMin"] | humMin;
    float hMax = doc["hMax"] | humMax;

    int bDay   = doc["bDay"]   | dayBrightness;
    int bNight = doc["bNight"] | nightBrightness;
    int nStart = doc["nStart"] | nightStartHour;
    int nEnd   = doc["nEnd"]   | nightEndHour;

    bDay   = constrain(bDay, 0, 100);
    bNight = constrain(bNight, 0, 100);
    nStart = constrain(nStart, 0, 23);
    nEnd   = constrain(nEnd, 0, 23);

    if (tMin >= tMax || hMin >= hMax || tMin < -40.0f || tMax > 80.0f || hMin < 0.0f || hMax > 100.0f) {
        server.send(400, "application/json", "{\"error\":\"Invalid ranges\"}");
        return;
    }

    float tOff = doc["tOff"] | tempOffset;
    float hOff = doc["hOff"] | humOffset;
    tOff = constrain(tOff, -5.0f, 5.0f);
    hOff = constrain(hOff, -15.0f, 15.0f);

    saveSettings(prefs, tMin, tMax, hMin, hMax, bDay, bNight, nStart, nEnd, tOff, hOff);
    if (!isnan(cachedTemp)) updateLedClimate(cachedTemp);

    server.send(200, "application/json", "{\"status\":\"ok\"}");

    // Мгновенно пушим новые настройки всем клиентам SSE
    String json;
    buildDataJson(json);
    broadcastSse(json);
}

// ========== API: EXPORT CSV (24H CLIMATE HISTORY) ==========

void handleApiExportCsv(WebServer &server) {
    server.sendHeader("Content-Type", "text/csv; charset=utf-8");
    server.sendHeader("Content-Disposition", "attachment; filename=\"jarvis_climate_24h.csv\"");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/csv", "");

    server.sendContent("Index,TimeAgoMins,Temperature_C,Humidity_Pct,DewPoint_C,HeatIndex_C,AbsHumidity_gm3\r\n");

    int count = historyFull ? HISTORY_SIZE : historyIndex;
    int start = historyFull ? historyIndex : 0;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % HISTORY_SIZE;
        float t = tempHistory[idx];
        float h = humHistory[idx];
        float dp = calculateDewPoint(t, h);
        float hi = calculateHeatIndex(t, h);
        float ah = calculateAbsoluteHumidity(t, h);

        int minsAgo = (count - 1 - i) * 5;
        char row[128];
        snprintf(row, sizeof(row), "%d,-%d min,%.2f,%.1f,%.2f,%.2f,%.2f\r\n",
                 i + 1, minsAgo, t, h, dp, hi, ah);
        server.sendContent(row);
    }
}

// ========== API: SEND TO TELEGRAM ==========

void handleApiSendTelegram(WebServer &server, UniversalTelegramBot &bot) {
    if (acquireBotLock()) {
        const char* report = buildFullReport(cachedTemp, cachedHum);
        snprintf(alertBuf, BIG_BUF_SIZE, "📲 *Запрос отчёта из веб-панели:*\n\n%s", report);
        bot.sendMessage(CHAT_ID, alertBuf, "Markdown");
        releaseBotLock();
        server.send(200, "application/json", "{\"status\":\"sent\"}");
    } else {
        server.send(503, "application/json", "{\"status\":\"busy\"}");
    }
}

// ========== API: HEALTH ==========

void handleApiHealth(WebServer &server) {
    StaticJsonDocument<384> doc;
    doc["freeHeap"]    = ESP.getFreeHeap();
    doc["maxBlock"]    = ESP.getMaxAllocHeap();
    doc["uptimeSec"]   = millis() / 1000UL;
    doc["wifiRSSI"]    = WiFi.RSSI();
    doc["sensorOk"]    = sensorOk();
    doc["sensorModel"] = getSensorModelName();
    doc["isNight"]     = isNightTime();
    doc["brightness"]  = getCurrentBrightness();
    doc["sseOnline"]   = (getActiveSseCount() > 0);
    doc["sseClients"]  = getActiveSseCount();
    doc["historyPts"]  = historyFull ? HISTORY_SIZE : historyIndex;
    doc["resetReason"] = (int)esp_reset_reason();

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}
