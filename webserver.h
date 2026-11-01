#pragma once
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
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

extern bool historyUpdatedForSse;

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

// ========== PWA MANIFEST, SERVICE WORKER & ICONS ==========

void handleManifest(WebServer &server) {
    server.sendHeader("Cache-Control", "public, max-age=86400");
    static const char manifest[] PROGMEM = R"rawliteral({
  "name": "Project Jarvis — Климат",
  "short_name": "Jarvis",
  "description": "Мониторинг микроклимата и качества воздуха Jarvis",
  "start_url": "/",
  "scope": "/",
  "display": "standalone",
  "orientation": "portrait",
  "background_color": "#090a0d",
  "theme_color": "#0b0f19",
  "icons": [
    {
      "src": "/icon.svg",
      "sizes": "any",
      "type": "image/svg+xml",
      "purpose": "any maskable"
    }
  ]
})rawliteral";
    server.send(200, "application/json", manifest);
}

void handleServiceWorker(WebServer &server) {
    server.sendHeader("Content-Type", "application/javascript");
    server.sendHeader("Service-Worker-Allowed", "/");
    server.sendHeader("Cache-Control", "no-cache");
    static const char sw[] PROGMEM = R"rawliteral(
const CACHE_NAME = 'jarvis-pwa-v3';
const ASSETS = ['/', '/manifest.json', '/icon.svg'];

self.addEventListener('install', (e) => {
  e.waitUntil(
    caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
  );
  self.skipWaiting();
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE_NAME).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener('fetch', (e) => {
  if (e.request.url.includes('/api/')) {
    return;
  }
  e.respondWith(
    fetch(e.request).catch(() => caches.match(e.request))
  );
});
)rawliteral";
    server.send(200, "application/javascript", sw);
}

void handleAppIcon(WebServer &server) {
    server.sendHeader("Content-Type", "image/svg+xml");
    server.sendHeader("Cache-Control", "public, max-age=604800");
    static const char iconSvg[] PROGMEM = R"rawliteral(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
  <defs>
    <radialGradient id="bg" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#1e293b"/>
      <stop offset="100%" stop-color="#0b0f19"/>
    </radialGradient>
    <linearGradient id="neon" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#38bdf8"/>
      <stop offset="50%" stop-color="#818cf8"/>
      <stop offset="100%" stop-color="#c084fc"/>
    </linearGradient>
    <filter id="glow" x="-20%" y="-20%" width="140%" height="140%">
      <feGaussianBlur stdDeviation="8" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>
  </defs>
  <rect width="512" height="512" rx="128" fill="url(#bg)"/>
  <rect width="508" height="508" x="2" y="2" rx="126" fill="none" stroke="url(#neon)" stroke-width="4" stroke-opacity="0.4"/>
  <circle cx="256" cy="256" r="160" fill="none" stroke="url(#neon)" stroke-width="12" stroke-dasharray="16 8" filter="url(#glow)"/>
  <circle cx="256" cy="256" r="120" fill="rgba(56, 189, 248, 0.08)"/>
  <path d="M220 180 L292 180 L292 270 C292 315 256 345 220 345 C195 345 175 330 170 305" fill="none" stroke="url(#neon)" stroke-width="24" stroke-linecap="round" stroke-linejoin="round" filter="url(#glow)"/>
  <circle cx="256" cy="130" r="14" fill="#38bdf8" filter="url(#glow)"/>
  <circle cx="370" cy="256" r="10" fill="#818cf8"/>
  <circle cx="256" cy="382" r="10" fill="#c084fc"/>
  <circle cx="142" cy="256" r="10" fill="#38bdf8"/>
</svg>)rawliteral";
    server.send(200, "image/svg+xml", iconSvg);
}

// ========== WEB OTA FIRMWARE UPDATE ==========

void handleUpdateDone(WebServer &server) {
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        server.send(500, "application/json", "{\"status\":\"error\",\"msg\":\"Ошибка прошивки по воздуху\"}");
    } else {
        server.send(200, "application/json", "{\"status\":\"success\",\"msg\":\"Прошивка успешно установлена! Перезагрузка...\"}");
        delay(1000);
        ESP.restart();
    }
}

void handleUpdateUpload(WebServer &server) {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[WebOTA] Начало загрузки прошивки: %s\n", upload.filename.c_str());
        // UPDATE_SIZE_UNKNOWN сообщает ядру выделить весь доступный свободный раздел OTA
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[WebOTA] Успешно завершено! Записано: %u байт\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        Serial.println("[WebOTA] Обновление отменено");
    }
}

// ========== СБОРКА JSON ДАННЫХ (ОБЩАЯ С ПОДДЕРЖКОЙ DELTA PUSH) ==========

template<typename TDoc>
static void populateScalarMetrics(TDoc &doc) {
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
}

void buildDataJson(String &output, bool includeHistory = true) {
    if (includeHistory) {
        StaticJsonDocument<2048> doc;
        populateScalarMetrics(doc);

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
    } else {
        // Ультра-быстрый легкий JSON (~250 байт) для 2-секундных SSE тиков
        StaticJsonDocument<512> doc;
        populateScalarMetrics(doc);
        output.reserve(measureJson(doc) + 1);
        serializeJson(doc, output);
    }
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

    // Передаем полную историю только если добавилась новая точка (~раз в 5 мин)
    bool sendFullHistory = historyUpdatedForSse;
    String json;
    buildDataJson(json, sendFullHistory);
    broadcastSse(json);

    if (sendFullHistory) {
        historyUpdatedForSse = false;
    }
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
    buildDataJson(json, false);
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
