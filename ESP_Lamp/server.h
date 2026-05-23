#ifndef SERVER_H
#define SERVER_H

#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

volatile bool credsSubmitted = false;
String pendingSSID = "";
String pendingPASS = "";

const char HTML_FORM[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WiFi Lamp Setup</title>
  <style>
    body { font-family: sans-serif; background:#1a1a2e; color:#eaeaea;
           display:flex; justify-content:center; align-items:center;
           min-height:100vh; margin:0; padding:16px; box-sizing:border-box; }
    .card { background:#16213e; padding:28px 32px; border-radius:12px;
            box-shadow:0 8px 24px rgba(0,0,0,0.4); width:100%; max-width:360px; }
    h1 { margin:0 0 8px 0; font-size:22px; color:#e94560; }
    p  { margin:0 0 20px 0; font-size:13px; color:#a0a0b0; }
    label { display:block; margin:14px 0 6px 0; font-size:13px; color:#c0c0d0; }
    input { width:100%; padding:10px 12px; border-radius:6px; border:1px solid #2a2a4e;
            background:#0f1530; color:#eaeaea; box-sizing:border-box; font-size:14px; }
    button { margin-top:20px; width:100%; padding:12px; border:none; border-radius:6px;
             background:#e94560; color:white; font-size:15px; font-weight:600; cursor:pointer; }
    button:hover { background:#d63a52; }
    .device-id { font-size:11px; color:#666; margin-top:14px; text-align:center; }
  </style>
</head>
<body>
  <div class="card">
    <h1>WiFi Lamp Setup</h1>
    <p>Введите данные домашней WiFi сети, к которой подключится лампочка.</p>
    <form action="/setwifi" method="POST">
      <label for="ssid">Имя сети (SSID)</label>
      <input type="text" id="ssid" name="ssid" required maxlength="32" autocomplete="off">

      <label for="pass">Пароль</label>
      <input type="password" id="pass" name="pass" maxlength="64">

      <button type="submit">Подключиться</button>
    </form>
    <div class="device-id">Device ID: %DEVICE_ID%</div>
  </div>
</body>
</html>
)rawliteral";

const char HTML_OK[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="UTF-8">
  <title>Connecting...</title>
  <style>
    body { font-family: sans-serif; background:#1a1a2e; color:#eaeaea;
           display:flex; justify-content:center; align-items:center;
           min-height:100vh; margin:0; }
    .card { background:#16213e; padding:32px 40px; border-radius:12px; text-align:center; }
    h1 { color:#4ade80; margin:0 0 12px 0; }
  </style>
</head>
<body>
  <div class="card">
    <h1>OK</h1>
    <p>Данные сохранены. Устройство пытается подключиться к WiFi.<br>
       Если подключение успешно, эта точка доступа отключится.</p>
  </div>
</body>
</html>
)rawliteral";

void handleRoot() {
  String html = FPSTR(HTML_FORM);
  html.replace("%DEVICE_ID%", id());
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSetWifi() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "Missing ssid");
    return;
  }
  pendingSSID = server.arg("ssid");
  pendingPASS = server.hasArg("pass") ? server.arg("pass") : "";
  credsSubmitted = true;

  Serial.println("Received new credentials via web:");
  Serial.println("  SSID: " + pendingSSID);
  Serial.println("  PASS: " + pendingPASS);

  server.send(200, "text/html; charset=utf-8", FPSTR(HTML_OK));
}

void handleStatus() {
  String s = "{\"mode\":\"" + String(ap_mode ? "AP" : "STA") + "\",\"ip\":\"" + ip + "\",\"id\":\"" + id() + "\"}";
  server.send(200, "application/json", s);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

void server_init() {
  server.on("/",         HTTP_GET,  handleRoot);
  server.on("/setwifi",  HTTP_POST, handleSetWifi);
  server.on("/status",   HTTP_GET,  handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

#endif