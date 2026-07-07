
from pathlib import Path

root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

mqtt_h = root / "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.h"
if mqtt_h.exists():
    s = read(mqtt_h)
    if "lilygoMqttDebugJson" not in s:
        s = s.replace(
            "String lilygoMqttStatusJson();",
            "String lilygoMqttStatusJson();\nString lilygoMqttDebugJson();"
        )
    write(mqtt_h, s)

mqtt_cpp = root / "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp"
if mqtt_cpp.exists():
    s = read(mqtt_cpp)

    s = s.replace(
        'return WiFi.status() == WL_CONNECTED && lilygoNetworkModeName() == "WiFi";',
        'return WiFi.status() == WL_CONNECTED;'
    )

    if "String lilygoMqttDebugJson()" not in s:
        s += "\n" + '\nString lilygoMqttDebugJson()\n{\n    const unsigned long start = millis();\n\n    WiFiClient testClient;\n    bool tcpOk = false;\n\n    String host = config.mqttHost;\n    host.trim();\n\n    if (!host.isEmpty()) {\n        tcpOk = testClient.connect(host.c_str(), config.mqttPort);\n        testClient.stop();\n    }\n\n    const unsigned long durationMs = millis() - start;\n\n    String json = "{";\n    json += "\\"wifiStatus\\":" + String((int)WiFi.status()) + ",";\n    json += "\\"wifiConnected\\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";\n    json += "\\"ssid\\":\\"" + esc(WiFi.SSID()) + "\\",";\n    json += "\\"localIp\\":\\"" + esc(WiFi.localIP().toString()) + "\\",";\n    json += "\\"gateway\\":\\"" + esc(WiFi.gatewayIP().toString()) + "\\",";\n    json += "\\"subnet\\":\\"" + esc(WiFi.subnetMask().toString()) + "\\",";\n    json += "\\"dns0\\":\\"" + esc(WiFi.dnsIP(0).toString()) + "\\",";\n    json += "\\"dns1\\":\\"" + esc(WiFi.dnsIP(1).toString()) + "\\",";\n    json += "\\"rssi\\":" + String(WiFi.RSSI()) + ",";\n    json += "\\"networkMode\\":\\"" + esc(lilygoNetworkModeName()) + "\\",";\n    json += "\\"transportAvailable\\":" + String(wifiTransportAvailable() ? "true" : "false") + ",";\n    json += "\\"mqttHost\\":\\"" + esc(host) + "\\",";\n    json += "\\"mqttPort\\":" + String(config.mqttPort) + ",";\n    json += "\\"tcpConnectOk\\":" + String(tcpOk ? "true" : "false") + ",";\n    json += "\\"tcpConnectMs\\":" + String(durationMs) + ",";\n    json += "\\"mqttConnected\\":" + String(mqtt.connected() ? "true" : "false") + ",";\n    json += "\\"mqttState\\":" + String(mqtt.state()) + ",";\n    json += "\\"mqttStateText\\":\\"" + String(mqttStateText(mqtt.state())) + "\\",";\n    json += "\\"lastConnectState\\":" + String(lastConnectState) + ",";\n    json += "\\"lastConnectStateText\\":\\"" + String(mqttStateText(lastConnectState)) + "\\",";\n    json += "\\"connectAttempts\\":" + String(connectAttempts) + ",";\n    json += "\\"message\\":\\"" + esc(lastMessage) + "\\"";\n    json += "}";\n\n    return json;\n}\n' + "\n"

    write(mqtt_cpp, s)

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    s = read(web)

    if "handleMqttDebug" not in s:
        s = s.replace(
            'static void handleMqtt(){ server.send(200, "application/json", lilygoMqttStatusJson()); }',
            'static void handleMqtt(){ server.send(200, "application/json", lilygoMqttStatusJson()); }\nstatic void handleMqttDebug(){ server.send(200, "application/json", lilygoMqttDebugJson()); }'
        )
        s = s.replace(
            'static void handleMqtt()\n{\n    server.send(200, "application/json", lilygoMqttStatusJson());\n}',
            'static void handleMqtt()\n{\n    server.send(200, "application/json", lilygoMqttStatusJson());\n}\n\nstatic void handleMqttDebug()\n{\n    server.send(200, "application/json", lilygoMqttDebugJson());\n}'
        )

    if 'server.on("/api/lilygo/mqtt/debug"' not in s:
        s = s.replace(
            'server.on("/api/lilygo/mqtt",HTTP_GET,handleMqtt);',
            'server.on("/api/lilygo/mqtt",HTTP_GET,handleMqtt);server.on("/api/lilygo/mqtt/debug",HTTP_GET,handleMqttDebug);'
        )
        s = s.replace(
            'server.on("/api/lilygo/mqtt", HTTP_GET, handleMqtt);',
            'server.on("/api/lilygo/mqtt", HTTP_GET, handleMqtt); server.on("/api/lilygo/mqtt/debug", HTTP_GET, handleMqttDebug);'
        )

    if "MQTT Debug" not in s:
        s = s.replace(
            "<h2>MQTT WiFi</h2>",
            "<h2>MQTT WiFi</h2><p><a href='/api/lilygo/mqtt/debug'>MQTT Debug</a></p>"
        )

    write(web, s)

print("MOT v1.1.0 LilyGO MQTT debug patch applied.")
