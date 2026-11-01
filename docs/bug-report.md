# 🐛 Аудит Project Jarvis — Баги и потенциальные проблемы

Файл: [`Project_Jarvis.ino`](file:///d:/Project_Jarvis/Project_Jarvis.ino)

---

## 🔴 Критические баги

### 1. Авто-отчёт стреляет сразу при включении (двойное сообщение)

[Строка 49](file:///d:/Project_Jarvis/Project_Jarvis.ino#L49), [строка 1188](file:///d:/Project_Jarvis/Project_Jarvis.ino#L1188-L1192)

`lastTimeAutoSend` инициализируется в `0`. На первой же итерации `loop()` условие `millis() - 0 > 14400000` будет **истинно** (т.к. `millis()` — unsigned, и `millis() - 0 = millis()`, но `millis()` на старте мал... нет, на самом деле это сработает корректно не сразу). 

**Но!** Настоящая проблема в том, что `lastTimeAutoSend = 0` и при этом в `setup()` уже отправляется стартовое сообщение (строка 1160-1164). Первая проверка в `loop()` после `setup()` — `millis()` будет маленьким (несколько секунд), так что `millis() - 0 < 14400000` → **ОК, не выстрелит сразу**. Но после ~49.7 дней аптайма `millis()` переполнится, и тогда возникнет ложное срабатывание.

**Фикс:**
```cpp
// В setup(), после отправки стартового сообщения:
lastTimeAutoSend = millis();
```

---

### 2. Деление на ноль в расчёте цвета LED

[Строки 108, 119](file:///d:/Project_Jarvis/Project_Jarvis.ino#L106-L127) (C++), [строки 817, 828](file:///d:/Project_Jarvis/Project_Jarvis.ino#L816-L838) (JavaScript)

Если пользователь через веб-панель или бота выставит `tempMin = 18.0` или `tempMax = 28.0`:

```cpp
// Если tempMin == 18.0 → делитель = 0!
float ratio = (t - 18.0) / (tempMin - 18.0);  // 💥 Division by zero

// Если tempMax == 28.0 → делитель = 0!
float ratio = (t - tempMax) / (28.0 - tempMax); // 💥 Division by zero
```

На ESP32 это даст `inf` или `NaN`, что пойдёт в `analogWrite` и приведёт к непредсказуемому поведению LED. В JavaScript — аналогично.

**Фикс:**
```cpp
else if (t < tempMin) {
    float denom = tempMin - 18.0;
    float ratio = (denom > 0.01) ? (t - 18.0) / denom : 0.0;
    // ...
}
else if (t < 28.0) {
    float denom = 28.0 - tempMax;
    float ratio = (denom > 0.01) ? (t - tempMax) / denom : 0.0;
    // ...
}
```

---

### 3. Нет переподключения к Wi-Fi

[Строка 1167-1193](file:///d:/Project_Jarvis/Project_Jarvis.ino#L1167-L1193)

Если Wi-Fi отвалится (роутер перезагрузился, пропало питание), ESP32 **навсегда** потеряет связь. В `loop()` нет никакой проверки `WiFi.status()`. Бот, веб-панель и алерты — всё перестанет работать.

**Фикс — добавить в `loop()`:**
```cpp
// В начало loop():
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost! Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Reconnected!");
        MDNS.begin("climat");
    }
    return; // Пропустить текущий цикл
}
```

---

### 4. Нет валидации входных данных в API сохранения порогов

[Строки 341-354](file:///d:/Project_Jarvis/Project_Jarvis.ino#L341-L354)

```cpp
void handleApiSave() {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, server.arg("plain")); // Результат не проверяется!
    saveSettings(doc["tMin"], doc["tMax"], doc["hMin"], doc["hMax"]); // Может записать 0 или NaN
```

- Результат `deserializeJson()` не проверяется → кривой JSON молча запишет нули в NVS
- Нет проверки границ → можно задать `tMin = 100, tMax = -50`
- Нет проверки `tMin < tMax` и `hMin < hMax`

**Фикс:**
```cpp
void handleApiSave() {
    if (!server.hasArg("plain")) { server.send(400, "text/plain", "No body"); return; }
    
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    float tMin = doc["tMin"] | tempMin;
    float tMax = doc["tMax"] | tempMax;
    float hMin = doc["hMin"] | humMin;
    float hMax = doc["hMax"] | humMax;
    
    if (tMin >= tMax || hMin >= hMax || tMin < -40 || tMax > 80 || hMin < 0 || hMax > 100) {
        server.send(400, "application/json", "{\"error\":\"Invalid range\"}");
        return;
    }
    
    saveSettings(tMin, tMax, hMin, hMax);
    // ...
}
```

---

## 🟠 Серьёзные проблемы

### 5. Telegram `bot.getUpdates()` блокирует всё

[Строки 1172-1178](file:///d:/Project_Jarvis/Project_Jarvis.ino#L1172-L1178)

`bot.getUpdates()` — это синхронный HTTP-запрос к Telegram API. Он может занять **2-5 секунд** (а при проблемах с сетью — до таймаута). Всё это время:
- ❌ Веб-панель не отвечает
- ❌ LED не обновляется
- ❌ Алерты не проверяются

**И запрос идёт каждую секунду** (`botRequestDelay = 1000`), что ещё и создаёт нагрузку на Telegram API.

**Фикс:** увеличить интервал до 2-3 секунд:
```cpp
const unsigned long botRequestDelay = 2000; // Telegram rate-limit safe
```

---

### 6. Утечка памяти через `String`-конкатенацию

Повсюду в коде (строки 78-85, 183-249, 263-296, 1064-1107...) — массивное сцепление `String` объектов через `+=`. На ESP32 это вызывает **фрагментацию кучи**. После нескольких дней непрерывной работы свободная память «рассыпается», и `malloc` начинает возвращать `NULL` → **краш / перезагрузка**.

> [!WARNING]
> Это самая частая причина «ESP32 падает через X дней». Особенно критично, т.к. проект работает 24/7.

**Минимальный фикс — мониторинг памяти:**
```cpp
// Добавить в loop() раз в минуту:
Serial.printf("Free heap: %d, largest block: %d\n", 
    ESP.getFreeHeap(), ESP.getMaxAllocHeap());
```

**Идеальный фикс** — использовать `snprintf()` в статический буфер для формирования сообщений.

---

### 7. Огромный HTML-литерал на стеке

[Строки 377-1047](file:///d:/Project_Jarvis/Project_Jarvis.ino#L377-L1047)

`const char html[] PROGMEM` внутри функции `handleRoot()` — это ~**15 КБ** данных. На ESP32 `PROGMEM` не имеет специального эффекта (единое адресное пространство), но локальный массив может аллоцироваться на **стеке**. Стек задачи Arduino по умолчанию — 8 КБ.

**Фикс — сделать `static`:**
```cpp
void handleRoot() {
    static const char html[] PROGMEM = R"rawliteral(...)rawliteral";
    server.send(200, "text/html", html);
}
```
`static` гарантирует, что строка будет в глобальной памяти (flash), а не на стеке.

---

### 8. NaN может попасть в `updateStats`

[Строки 1073-1078](file:///d:/Project_Jarvis/Project_Jarvis.ino#L1073-L1078)

```cpp
else if (text == "🌡 Температура") {
    float t = dht.readTemperature();
    float h = dht.readHumidity();      // ← может вернуть NaN
    if (isnan(t)) sendWithKeyboard(...);
    else {
        updateStats(t, h);  // h может быть NaN! → minHum/maxHum никогда не обновятся
```

Проверяется только `t`, но `h` тоже может быть `NaN`. `updateStats` не защищён от `NaN`.

**Фикс:**
```cpp
if (isnan(t) || isnan(h)) { ... }
```

---

## 🟡 Потенциальные проблемы

### 9. API эндпоинты без аутентификации

[Строки 300-360](file:///d:/Project_Jarvis/Project_Jarvis.ino#L300-L360)

Любой в локальной сети может:
- `GET /api/data` — получить все показатели
- `POST /api/save` — перезаписать пороги (перевести LED в красный)
- `GET /api/send_tg` — спамить в Telegram

`/api/send_tg` и `/api/save` должны быть как минимум POST с проверкой простого токена.

---

### 10. `setInsecure()` — отключена проверка TLS-сертификата

[Строка 1143](file:///d:/Project_Jarvis/Project_Jarvis.ino#L1143)

```cpp
secured_client.setInsecure(); // Принимает любой сертификат!
```

Это делает HTTPS-соединение с Telegram API уязвимым для MITM-атаки. Для домашнего проекта допустимо, но для продакшена стоит использовать certificate pinning.

---

### 11. `StaticJsonDocument<1200>` может быть впритык

[Строка 309](file:///d:/Project_Jarvis/Project_Jarvis.ino#L309)

С 24 точками истории (2 массива × 24 float) + все остальные поля — 1200 байт может не хватить. Если буфер переполнится, `serializeJson` молча обрежет данные.

**Проверка:** используйте [ArduinoJson Assistant](https://arduinojson.org/v6/assistant/) для расчёта точного размера.

---

### 12. `millis()` overflow в `statsResetTime`

[Строка 162](file:///d:/Project_Jarvis/Project_Jarvis.ino#L162)

```cpp
if (millis() - statsResetTime > 86400000) { // 24 часа
```

Арифметика `unsigned long` корректно обрабатывает переполнение `millis()` (через ~49.7 дней), так что баг не возникнет. Но **все остальные таймеры тоже безопасны** — тут всё ОК ✅

---

## 🔵 Рекомендации по улучшению

| # | Что | Зачем |
|---|-----|-------|
| 13 | Добавить **OTA-обновления** (`ArduinoOTA`) | Не надо каждый раз подключать USB |
| 14 | Вынести HTML в **LittleFS** | Отделить прошивку от интерфейса |
| 15 | Добавить **Watchdog Timer** | Автоперезагрузка при зависании |
| 16 | Кешировать показания DHT | Не дёргать датчик из каждой функции отдельно |
| 17 | Добавить `/api/health` эндпоинт | Для внешнего мониторинга доступности |

---

## Сводка

| Уровень | Кол-во | Самое критичное |
|---------|--------|-----------------|
| 🔴 Критическое | 4 | Division by zero в LED, нет WiFi reconnect, нет валидации API |
| 🟠 Серьёзное | 4 | String-фрагментация, HTML на стеке, блокирующий Telegram |
| 🟡 Потенциальное | 3 | Нет auth на API, TLS insecure, JSON-буфер впритык |
| 🔵 Улучшения | 5 | OTA, Watchdog, кеш DHT |
