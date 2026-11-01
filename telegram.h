#pragma once
#include <UniversalTelegramBot.h>
#include "config.h"

// ========== ПОТОКОБЕЗОПАСНОСТЬ FREERTOS & МЬЮТЕКС ==========
extern SemaphoreHandle_t botMutex;

inline bool acquireBotLock(TickType_t waitTicks = pdMS_TO_TICKS(5000)) {
    if (!botMutex) return true;
    return (xSemaphoreTakeRecursive(botMutex, waitTicks) == pdTRUE);
}

inline void releaseBotLock() {
    if (botMutex) {
        xSemaphoreGiveRecursive(botMutex);
    }
}

// ========== РАЗДЕЛЬНЫЕ БУФЕРЫ СООБЩЕНИЙ ==========
// Устранение race condition между задачей Telegram и основным циклом loop()
static char msgBuf[BIG_BUF_SIZE];
static char alertBuf[BIG_BUF_SIZE];

// ========== КЛАВИАТУРА ==========

String getKeyboardJson() {
    return "[[\"📊 Всё сразу\"], [\"🌡 Температура\", \"💧 Влажность\"], [\"📈 Статистика\", \"ℹ️ Инфо\"]]";
}

void sendWithKeyboard(UniversalTelegramBot &bot, const char* chat_id, const char* text) {
    if (acquireBotLock()) {
        String kb = getKeyboardJson();
        bot.sendMessageWithReplyKeyboard(chat_id, text, "Markdown", kb, true);
        releaseBotLock();
    }
}

// ========== ФОРМИРОВАНИЕ ОТЧЁТОВ ==========

const char* buildTempAnalysis(float t) {
    char extra[64] = "";

    if (t < tempMin) {
        snprintf(extra, sizeof(extra), "%.1f", tempMin - t);
        snprintf(msgBuf, MSG_BUF_SIZE,
            "🌡 Температура: *%.1f °C*\n"
            "🎯 _Норма: %.0f–%.0f °C_\n"
            "❄️ Статус: *Ниже нормы на %s °C* (прохладно)\n",
            t, tempMin, tempMax, extra);
    } else if (t > tempMax) {
        snprintf(extra, sizeof(extra), "%.1f", t - tempMax);
        snprintf(msgBuf, MSG_BUF_SIZE,
            "🌡 Температура: *%.1f °C*\n"
            "🎯 _Норма: %.0f–%.0f °C_\n"
            "🔥 Статус: *Выше нормы на %s °C* (жарко)\n",
            t, tempMin, tempMax, extra);
    } else {
        snprintf(msgBuf, MSG_BUF_SIZE,
            "🌡 Температура: *%.1f °C*\n"
            "🎯 _Норма: %.0f–%.0f °C_\n"
            "✅ Статус: *В пределах нормы*\n",
            t, tempMin, tempMax);
    }
    return msgBuf;
}

// Гибридная оценка влажности с учётом точки росы
const char* buildHumAnalysis(float h, float t) {
    float dp = calculateDewPoint(t, h);

    if (h < humMin) {
        float diff = humMin - h;
        if (!isnan(dp) && dp < 5.0f) {
            snprintf(msgBuf, MSG_BUF_SIZE,
                "💧 Влажность: *%.1f %%*\n"
                "🎯 _Норма: %.0f–%.0f %%_\n"
                "🌵 Статус: *Ниже нормы на %.1f %%* (Тр = %.1f °C, сильная сушь, включи увлажнитель)\n",
                h, humMin, humMax, diff, dp);
        } else {
            snprintf(msgBuf, MSG_BUF_SIZE,
                "💧 Влажность: *%.1f %%*\n"
                "🎯 _Норма: %.0f–%.0f %%_\n"
                "🌵 Статус: *Ниже нормы на %.1f %%* (включи увлажнитель)\n",
                h, humMin, humMax, diff);
        }
    } else if (h > humMax) {
        float diff = h - humMax;
        if (!isnan(dp) && dp >= 18.0f) {
            snprintf(msgBuf, MSG_BUF_SIZE,
                "💧 Влажность: *%.1f %%*\n"
                "🎯 _Норма: %.0f–%.0f %%_\n"
                "🔥 Статус: *Выше нормы на %.1f %%* (Тр = %.1f °C, очень душно, эффект сауны!)\n",
                h, humMin, humMax, diff, dp);
        } else {
            snprintf(msgBuf, MSG_BUF_SIZE,
                "💧 Влажность: *%.1f %%*\n"
                "🎯 _Норма: %.0f–%.0f %%_\n"
                "💦 Статус: *Выше нормы на %.1f %%* (сыро, проветри)\n",
                h, humMin, humMax, diff);
        }
    } else {
        // Влажность в % формально в норме, но проверяем точку росы на духоту
        if (!isnan(dp) && dp >= 16.0f) {
            snprintf(msgBuf, MSG_BUF_SIZE,
                "💧 Влажность: *%.1f %%*\n"
                "🎯 _Норма: %.0f–%.0f %%_\n"
                "💦 Статус: *В норме, но душновато* (Тр = %.1f °C, влажное тепло, можно проветрить)\n",
                h, humMin, humMax, dp);
        } else {
            snprintf(msgBuf, MSG_BUF_SIZE,
                "💧 Влажность: *%.1f %%*\n"
                "🎯 _Норма: %.0f–%.0f %%_\n"
                "✅ Статус: *Идеальный баланс* (Тр = %.1f °C, %s)\n",
                h, humMin, humMax, dp, getDewPointDesc(dp));
        }
    }
    return msgBuf;
}

const char* buildFullReport(float t, float h) {
    if (isnan(t) || isnan(h)) {
        return "⚠️ *Ошибка чтения с датчика DHT!* Проверь контакты.";
    }

    char timeStr[16];
    getFormattedTime(timeStr, sizeof(timeStr));

    float dp = calculateDewPoint(t, h);
    float hi = calculateHeatIndex(t, h);
    float ah = calculateAbsoluteHumidity(t, h);
    const char* mold = getMoldRiskLevel(t, h, dp);

    float tDelta = 0, hDelta = 0;
    const char* tTrend = "";
    const char* hTrend = "";
    calculateTrends(tDelta, hDelta, tTrend, hTrend);

    char tempPart[MSG_BUF_SIZE];
    char humPart[MSG_BUF_SIZE];

    buildTempAnalysis(t);
    strncpy(tempPart, msgBuf, MSG_BUF_SIZE);

    buildHumAnalysis(h, t);
    strncpy(humPart, msgBuf, MSG_BUF_SIZE);

    snprintf(msgBuf, BIG_BUF_SIZE,
        "📊 *Анализ микроклимата (%s):*\n\n"
        "%s\n"
        "%s\n"
        "✨ *Ощущение и экология:*\n"
        "• Ощущается как: *%.1f °C*\n"
        "• Точка росы: *%.1f °C* (%s)\n"
        "• Абс. влажность: *%.1f г/м³*\n"
        "• Риск плесени: *%s*\n"
        "• Тренд за час: Т *%s*, Вл *%s*\n"
        "• Режим: *%s* (LED %d%%)%s",
        timeStr, tempPart, humPart,
        hi, dp, getDewPointDesc(dp),
        isnan(ah) ? 0.0f : ah,
        mold,
        tTrend, hTrend,
        isNightTime() ? "🌙 Ночь" : "☀️ День",
        getCurrentBrightness(),
        (millis() < alertMuteUntil) ? "\n🔕 *Алерты временно заглушены*" : "");

    return msgBuf;
}

const char* buildStatsReport() {
    if (minTemp > 900.0f) {
        return "📈 *Статистика:* ещё нет накопленных данных.";
    }

    unsigned long hours = (millis() - statsResetTime) / 3600000UL;
    char timeStr[16];
    getFormattedTime(timeStr, sizeof(timeStr));

    snprintf(msgBuf, BIG_BUF_SIZE,
        "📈 *Экстремумы за %lu ч. (на %s):*\n\n"
        "🌡 *Температура:*\n"
        "• Минимум: *%.1f °C*\n"
        "• Максимум: *%.1f °C*\n\n"
        "💧 *Влажность:*\n"
        "• Минимум: *%.1f %%*\n"
        "• Максимум: *%.1f %%*",
        hours, timeStr, minTemp, maxTemp, minHum, maxHum);

    return msgBuf;
}

// ========== АЛЕРТЫ ==========

bool alertTempHighSent = false;
bool alertTempLowSent  = false;
bool alertHumLowSent   = false;
bool alertHumHighSent  = false;

void checkCriticalAlerts(UniversalTelegramBot &bot, float t, float h) {
    if (isnan(t) || isnan(h)) return;
    if (millis() < alertMuteUntil) return; // Режим Snooze: алерты временно заглушены пользователем

    if (t >= ALERT_TEMP_HIGH && !alertTempHighSent) {
        if (acquireBotLock()) {
            snprintf(alertBuf, MSG_BUF_SIZE,
                "🚨 *ВНИМАНИЕ! Высокая температура!*\n\n"
                "Текущая: *%.1f °C* (выше порога %.0f °C).", t, ALERT_TEMP_HIGH);
            bot.sendMessage(CHAT_ID, alertBuf, "Markdown");
            releaseBotLock();
            alertTempHighSent = true;
        }
    } else if (t < (ALERT_TEMP_HIGH - 1.0f) && alertTempHighSent) {
        alertTempHighSent = false;
    }

    if (t <= ALERT_TEMP_LOW && !alertTempLowSent) {
        if (acquireBotLock()) {
            snprintf(alertBuf, MSG_BUF_SIZE,
                "🥶 *ВНИМАНИЕ! Сильное похолодание!*\n\n"
                "Текущая: *%.1f °C* (ниже порога %.0f °C).", t, ALERT_TEMP_LOW);
            bot.sendMessage(CHAT_ID, alertBuf, "Markdown");
            releaseBotLock();
            alertTempLowSent = true;
        }
    } else if (t > (ALERT_TEMP_LOW + 1.0f) && alertTempLowSent) {
        alertTempLowSent = false;
    }

    if (h <= ALERT_HUM_LOW && !alertHumLowSent) {
        if (acquireBotLock()) {
            snprintf(alertBuf, MSG_BUF_SIZE,
                "🌵 *ВНИМАНИЕ! Критически сухой воздух!*\n\n"
                "Влажность упала до *%.1f %%*. Рекомендуется включить увлажнитель.", h);
            bot.sendMessage(CHAT_ID, alertBuf, "Markdown");
            releaseBotLock();
            alertHumLowSent = true;
        }
    } else if (h > (ALERT_HUM_LOW + 3.0f) && alertHumLowSent) {
        alertHumLowSent = false;
    }

    if (h >= ALERT_HUM_HIGH && !alertHumHighSent) {
        if (acquireBotLock()) {
            snprintf(alertBuf, MSG_BUF_SIZE,
                "💦 *ВНИМАНИЕ! Чрезмерная сырость!*\n\n"
                "Влажность поднялась до *%.1f %%*. Проветрите помещение.", h);
            bot.sendMessage(CHAT_ID, alertBuf, "Markdown");
            releaseBotLock();
            alertHumHighSent = true;
        }
    } else if (h < (ALERT_HUM_HIGH - 3.0f) && alertHumHighSent) {
        alertHumHighSent = false;
    }
}

// ========== ОБРАБОТКА КОМАНД ==========

void handleNewMessages(UniversalTelegramBot &bot, int numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
        const char* chat_id = bot.messages[i].chat_id.c_str();
        String text = bot.messages[i].text;

        if (strcmp(chat_id, CHAT_ID) != 0) {
            bot.sendMessage(chat_id, "Доступ запрещён ⛔", "");
            continue;
        }

        if (text == "/start") {
            snprintf(msgBuf, BIG_BUF_SIZE,
                "👋 *Привет!*\nЯ бот микроклимата на ESP32-C3 SuperMini.\n"
                "🌐 Веб-панель: http://climat.local (или http://%s)\n\n"
                "Используй кнопки внизу для запроса показателей:",
                WiFi.localIP().toString().c_str());
            sendWithKeyboard(bot, chat_id, msgBuf);
        }
        else if (text == "📊 Всё сразу" || text == "/status") {
            sendWithKeyboard(bot, chat_id, buildFullReport(cachedTemp, cachedHum));
        }
        else if (text == "🌡 Температура") {
            if (isnan(cachedTemp)) {
                sendWithKeyboard(bot, chat_id, "⚠️ Ошибка датчика температуры!");
            } else {
                sendWithKeyboard(bot, chat_id, buildTempAnalysis(cachedTemp));
            }
        }
        else if (text == "💧 Влажность") {
            if (isnan(cachedHum)) {
                sendWithKeyboard(bot, chat_id, "⚠️ Ошибка датчика влажности!");
            } else {
                sendWithKeyboard(bot, chat_id, buildHumAnalysis(cachedHum, cachedTemp));
            }
        }
        else if (text == "📈 Статистика") {
            sendWithKeyboard(bot, chat_id, buildStatsReport());
        }
        else if (text == "📉 Тренды" || text == "/trends") {
            float tDelta = 0, hDelta = 0;
            const char* tTrend = "";
            const char* hTrend = "";
            calculateTrends(tDelta, hDelta, tTrend, hTrend);

            snprintf(msgBuf, MSG_BUF_SIZE,
                "📊 *Динамика изменения климата (за 1 час):*\n\n"
                "🌡 Температура: *%s* (скорость: %+.2f °C/ч)\n"
                "💧 Влажность: *%s* (скорость: %+.1f %%/ч)\n\n"
                "💡 _Показывает скорость нагрева или остывания комнаты._",
                tTrend, tDelta, hTrend, hDelta);
            sendWithKeyboard(bot, chat_id, msgBuf);
        }
        else if (text == "/mute" || text == "/mute 1h") {
            alertMuteUntil = millis() + 3600000UL;
            sendWithKeyboard(bot, chat_id, "🔕 *Алерты заглушены на 1 час.*\nИспользуй /unmute для досрочного включения.");
        }
        else if (text == "/mute 2h") {
            alertMuteUntil = millis() + 7200000UL;
            sendWithKeyboard(bot, chat_id, "🔕 *Алерты заглушены на 2 часа.*\nИспользуй /unmute для досрочного включения.");
        }
        else if (text == "/unmute") {
            alertMuteUntil = 0;
            sendWithKeyboard(bot, chat_id, "🔔 *Алерты снова активны!* Мониторинг в штатном режиме.");
        }
        else if (text == "ℹ️ Инфо") {
            char timeStr[16];
            getFormattedTime(timeStr, sizeof(timeStr));

            char muteStr[32] = "🔔 Активны";
            if (millis() < alertMuteUntil) {
                unsigned long leftMins = (alertMuteUntil - millis()) / 60000UL;
                snprintf(muteStr, sizeof(muteStr), "🔕 Заглушены (%lu мин)", leftMins);
            }

            snprintf(msgBuf, MSG_BUF_SIZE,
                "ℹ️ *Информация об устройстве:*\n\n"
                "• Время (МСК): *%s*\n"
                "• Датчик: *%s*\n"
                "• Калибровка: *T: %+.1f°C*, *H: %+.0f%%*\n"
                "• Алерты: *%s*\n"
                "• Режим: *%s* (LED %d%%)\n"
                "• Локальный адрес: http://climat.local\n"
                "• IP адрес: `%s`\n"
                "• Wi-Fi сигнал: *%ld dBm*\n"
                "• Аптайм: *%lu мин.*\n"
                "• Свободная память: *%u байт*",
                timeStr,
                getSensorModelName(),
                tempOffset, humOffset,
                muteStr,
                isNightTime() ? "🌙 Ночной" : "☀️ Дневной",
                getCurrentBrightness(),
                WiFi.localIP().toString().c_str(),
                WiFi.RSSI(),
                millis() / 60000UL,
                ESP.getFreeHeap());
            sendWithKeyboard(bot, chat_id, msgBuf);
        }
        else {
            sendWithKeyboard(bot, chat_id, "Неизвестная команда. Нажми одну из кнопок меню 👇");
        }
    }
}
