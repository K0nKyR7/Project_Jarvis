#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config.h"

// ========== WI-FI CAPTIVE PORTAL (Jarvis-Setup) ==========
// Если ESP32 не может подключиться к сохранённой Wi-Fi сети,
// автоматически поднимается точка доступа "Jarvis-Setup".
// Все DNS-запросы перенаправляются на 192.168.4.1,
// где пользователь может ввести SSID и пароль новой сети.

#define CP_AP_NAME      "Jarvis-Setup"
#define CP_AP_PASS      ""                // Открытая точка (без пароля)
#define CP_DNS_PORT     53
#define CP_TIMEOUT_MS   60000UL           // 60 секунд попыток подключения к Wi-Fi

static DNSServer cpDnsServer;
static WebServer cpServer(80);
static bool portalActive = false;

// NVS ключи для сохранённых Wi-Fi credentials
static const char* NVS_WIFI_NS   = "wifi_cfg";
static const char* NVS_KEY_SSID  = "ssid";
static const char* NVS_KEY_PASS  = "pass";

// Загрузка сохранённых Wi-Fi credentials из NVS
// Возвращает true, если SSID найден
bool loadSavedWiFi(Preferences &prefs, String &savedSsid, String &savedPass) {
    prefs.begin(NVS_WIFI_NS, true);
    savedSsid = prefs.getString(NVS_KEY_SSID, "");
    savedPass = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();
    return savedSsid.length() > 0;
}

// Сохранение Wi-Fi credentials в NVS
void saveWiFiCredentials(Preferences &prefs, const String &ssid, const String &pass) {
    prefs.begin(NVS_WIFI_NS, false);
    prefs.putString(NVS_KEY_SSID, ssid);
    prefs.putString(NVS_KEY_PASS, pass);
    prefs.end();
    Serial.printf("[CaptivePortal] Saved WiFi: SSID='%s'\n", ssid.c_str());
}

// HTML страница Captive Portal (минимальная, работает без внешних ресурсов)
static const char CP_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Jarvis — Настройка Wi-Fi</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:linear-gradient(135deg,#0a0e17 0%,#1a1f35 50%,#0d1220 100%);
color:#e0e0e0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:rgba(20,24,40,0.85);backdrop-filter:blur(20px);
border:1px solid rgba(255,255,255,0.08);border-radius:24px;
padding:40px 32px;max-width:420px;width:100%;
box-shadow:0 20px 60px rgba(0,0,0,0.5)}
.logo{text-align:center;margin-bottom:24px}
.logo svg{width:64px;height:64px}
h1{font-size:22px;text-align:center;margin-bottom:6px;
background:linear-gradient(135deg,#60a5fa,#a78bfa);
-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{text-align:center;font-size:13px;color:#888;margin-bottom:28px}
label{display:block;font-size:13px;color:#aaa;margin-bottom:6px;margin-top:16px}
input[type="text"],input[type="password"]{width:100%;padding:12px 16px;
background:rgba(255,255,255,0.06);border:1px solid rgba(255,255,255,0.12);
border-radius:12px;color:#fff;font-size:15px;outline:none;
transition:border-color 0.3s}
input:focus{border-color:#60a5fa}
select{width:100%;padding:12px 16px;background:rgba(255,255,255,0.06);
border:1px solid rgba(255,255,255,0.12);border-radius:12px;color:#fff;
font-size:15px;outline:none;appearance:none}
select option{background:#1a1f35;color:#fff}
.btn{width:100%;padding:14px;margin-top:24px;border:none;
border-radius:14px;font-size:16px;font-weight:600;cursor:pointer;
background:linear-gradient(135deg,#3b82f6,#8b5cf6);
color:#fff;transition:transform 0.2s,opacity 0.2s}
.btn:hover{transform:scale(1.02)}
.btn:active{transform:scale(0.98)}
.scan-btn{background:rgba(255,255,255,0.08);margin-top:12px;font-size:14px;
padding:10px;border-radius:10px;color:#aaa}
.scan-btn:hover{background:rgba(255,255,255,0.12);color:#fff}
.status{text-align:center;margin-top:16px;font-size:13px;color:#888}
.spinner{display:none;text-align:center;margin-top:16px}
.spinner::after{content:'';display:inline-block;width:24px;height:24px;
border:3px solid rgba(255,255,255,0.1);border-top-color:#60a5fa;
border-radius:50%;animation:spin 0.8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.networks{max-height:200px;overflow-y:auto;margin-top:8px}
.net-item{padding:10px 14px;margin:4px 0;border-radius:10px;cursor:pointer;
background:rgba(255,255,255,0.04);border:1px solid rgba(255,255,255,0.06);
display:flex;justify-content:space-between;align-items:center;
transition:background 0.2s}
.net-item:hover{background:rgba(255,255,255,0.1)}
.net-name{font-size:14px}
.net-rssi{font-size:12px;color:#888}
.success{color:#4ade80;text-align:center;font-size:15px;margin-top:16px}
.error{color:#f87171;text-align:center;font-size:14px;margin-top:12px}
</style>
</head>
<body>
<div class="card">
<div class="logo">
<svg viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
<circle cx="32" cy="32" r="30" stroke="url(#g)" stroke-width="2" fill="rgba(96,165,250,0.08)"/>
<path d="M22 38 L32 20 L42 38 Z" fill="none" stroke="url(#g)" stroke-width="2" stroke-linejoin="round"/>
<circle cx="32" cy="32" r="4" fill="url(#g)"/>
<defs><linearGradient id="g" x1="0" y1="0" x2="64" y2="64">
<stop stop-color="#60a5fa"/><stop offset="1" stop-color="#a78bfa"/></linearGradient></defs>
</svg>
</div>
<h1>Jarvis — Настройка Wi-Fi</h1>
<p class="sub">Устройство не смогло подключиться к сохранённой сети.<br>Выберите сеть или введите данные вручную.</p>

<div id="scanSection">
<button class="btn scan-btn" onclick="scanNetworks()">🔍 Сканировать сети</button>
<div id="networks" class="networks"></div>
</div>

<form id="wifiForm" action="/save" method="POST">
<label for="ssid">📶 Имя сети (SSID)</label>
<input type="text" id="ssid" name="ssid" placeholder="Имя Wi-Fi сети" required autocomplete="off">

<label for="pass">🔑 Пароль</label>
<input type="password" id="pass" name="pass" placeholder="Пароль Wi-Fi" autocomplete="off">

<button type="submit" class="btn">💾 Подключиться</button>
</form>

<div id="spinner" class="spinner"></div>
<div id="status" class="status"></div>
</div>

<script>
function scanNetworks(){
  document.getElementById('status').textContent='Сканирование...';
  fetch('/scan').then(r=>r.json()).then(nets=>{
    let html='';
    nets.forEach(n=>{
      let icon=n.rssi>-50?'📶':n.rssi>-70?'📶':'📡';
      let lock=n.enc?'🔒':'🔓';
      html+=`<div class="net-item" onclick="selectNet('${n.ssid.replace(/'/g,"\\'")}')">
        <span class="net-name">${icon} ${lock} ${n.ssid}</span>
        <span class="net-rssi">${n.rssi} dBm</span>
      </div>`;
    });
    document.getElementById('networks').innerHTML=html||'<p style="color:#888;padding:8px">Сети не найдены</p>';
    document.getElementById('status').textContent='';
  }).catch(()=>{
    document.getElementById('status').textContent='Ошибка сканирования';
  });
}

function selectNet(ssid){
  document.getElementById('ssid').value=ssid;
  document.getElementById('pass').focus();
}

document.getElementById('wifiForm').addEventListener('submit',function(e){
  e.preventDefault();
  let ssid=document.getElementById('ssid').value;
  let pass=document.getElementById('pass').value;
  document.getElementById('spinner').style.display='block';
  document.getElementById('status').innerHTML='Подключение к <b>'+ssid+'</b>...';

  fetch('/save',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)
  }).then(r=>r.json()).then(d=>{
    document.getElementById('spinner').style.display='none';
    if(d.success){
      document.getElementById('status').innerHTML=
        '<p class="success">✅ Подключено! IP: <b>'+d.ip+'</b></p>'+
        '<p style="margin-top:8px;color:#888">Устройство перезагрузится через 3 секунды...</p>';
    }else{
      document.getElementById('status').innerHTML=
        '<p class="error">❌ Не удалось подключиться. Проверьте пароль.</p>';
    }
  }).catch(()=>{
    document.getElementById('spinner').style.display='none';
    document.getElementById('status').innerHTML='<p class="error">Ошибка связи</p>';
  });
});
</script>
</body>
</html>)rawliteral";

// Сканирование Wi-Fi сетей (JSON)
void handleCpScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
                ",\"enc\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    cpServer.send(200, "application/json", json);
}

// Сохранение введённых credentials и попытка подключения
void handleCpSave(Preferences &prefs) {
    String newSsid = cpServer.arg("ssid");
    String newPass = cpServer.arg("pass");

    if (newSsid.length() == 0) {
        cpServer.send(400, "application/json", "{\"success\":false,\"error\":\"Empty SSID\"}");
        return;
    }

    Serial.printf("[CaptivePortal] Trying to connect to: %s\n", newSsid.c_str());

    // Пробуем подключиться
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(newSsid.c_str(), newPass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Успех! Сохраняем в NVS
        saveWiFiCredentials(prefs, newSsid, newPass);

        String ip = WiFi.localIP().toString();
        cpServer.send(200, "application/json", "{\"success\":true,\"ip\":\"" + ip + "\"}");
        Serial.printf("\n[CaptivePortal] ✅ Connected! IP: %s\n", ip.c_str());

        // Через 3 секунды перезагружаемся в нормальный режим
        delay(3000);
        ESP.restart();
    } else {
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(CP_AP_NAME, CP_AP_PASS);
        cpServer.send(200, "application/json", "{\"success\":false}");
        Serial.println("\n[CaptivePortal] ❌ Connection failed");
    }
}

// Запуск Captive Portal (точка доступа + DNS + веб-сервер)
void startCaptivePortal(Preferences &prefs) {
    portalActive = true;
    Serial.println("\n[CaptivePortal] Starting AP: " CP_AP_NAME);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(CP_AP_NAME, CP_AP_PASS);
    delay(500);

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[CaptivePortal] AP IP: %s\n", apIP.toString().c_str());

    // DNS сервер перенаправляет ВСЕ запросы на наш IP
    cpDnsServer.start(CP_DNS_PORT, "*", apIP);

    // Маршруты портала
    cpServer.on("/", []() {
        cpServer.send_P(200, "text/html", CP_HTML);
    });
    cpServer.on("/scan", handleCpScan);
    cpServer.on("/save", HTTP_POST, [&prefs]() {
        handleCpSave(prefs);
    });
    // Все неизвестные пути → главная страница (Captive Portal detection)
    cpServer.onNotFound([]() {
        cpServer.sendHeader("Location", "http://192.168.4.1/");
        cpServer.send(302, "text/plain", "");
    });
    cpServer.begin();
    Serial.println("[CaptivePortal] Web server started on 192.168.4.1");
}

// Цикл обработки Captive Portal (вызывается из loop() если портал активен)
void handleCaptivePortal() {
    if (!portalActive) return;
    cpDnsServer.processNextRequest();
    cpServer.handleClient();
}

// Попытка подключения к Wi-Fi с учётом NVS и Captive Portal fallback
// Возвращает true если подключение установлено, false если активен портал
bool connectWiFiWithPortal(Preferences &prefs) {
    String savedSsid, savedPass;
    bool hasNvs = loadSavedWiFi(prefs, savedSsid, savedPass);

    // 1. Сначала пробуем NVS credentials (если есть)
    if (hasNvs) {
        Serial.printf("[WiFi] Trying saved NVS WiFi: %s\n", savedSsid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSsid.c_str(), savedPass.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < CP_TIMEOUT_MS / 2) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] ✅ Connected via NVS! IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.println("\n[WiFi] NVS credentials failed");
        WiFi.disconnect();
    }

    // 2. Пробуем credentials из config.h
    Serial.printf("[WiFi] Trying config.h WiFi: %s\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < CP_TIMEOUT_MS / 2) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] ✅ Connected via config.h! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("\n[WiFi] ❌ All WiFi connections failed");
    WiFi.disconnect();

    // 3. Запускаем Captive Portal
    startCaptivePortal(prefs);
    return false;
}
