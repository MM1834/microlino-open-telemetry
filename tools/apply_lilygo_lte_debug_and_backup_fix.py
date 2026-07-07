from pathlib import Path
root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

mqtt = root / "firmware/lilygo-t-a7670/src/mqtt/lilygo_mqtt.cpp"
if mqtt.exists():
    s = read(mqtt)
    if "MQTT_LTE_RECONNECT_INTERVAL_MS" not in s:
        s = s.replace(
            "static const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;",
            "static const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;\nstatic const unsigned long MQTT_LTE_RECONNECT_INTERVAL_MS = 60000;"
        )
    if "reconnectIntervalMs" not in s:
        s = s.replace(
            "if(lastReconnectAttemptMs && millis()-lastReconnectAttemptMs<MQTT_RECONNECT_INTERVAL_MS) return;",
            "unsigned long reconnectIntervalMs = activeTransport == \"LTE\" ? MQTT_LTE_RECONNECT_INTERVAL_MS : MQTT_RECONNECT_INTERVAL_MS;\n    if(lastReconnectAttemptMs && millis()-lastReconnectAttemptMs<reconnectIntervalMs) return;"
        )
        s = s.replace(
            "if (lastReconnectAttemptMs && millis() - lastReconnectAttemptMs < MQTT_RECONNECT_INTERVAL_MS) return;",
            "unsigned long reconnectIntervalMs = activeTransport == \"LTE\" ? MQTT_LTE_RECONNECT_INTERVAL_MS : MQTT_RECONNECT_INTERVAL_MS;\n    if (lastReconnectAttemptMs && millis() - lastReconnectAttemptMs < reconnectIntervalMs) return;"
        )
    write(mqtt, s)

modem_h = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.h"
if modem_h.exists():
    s = read(modem_h)
    if "lilygoLteDebugJson" not in s:
        s += "\nString lilygoLteDebugJson();\n"
    write(modem_h, s)

modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
if modem_cpp.exists():
    s = read(modem_cpp)
    if "jsonEscLteDebug" not in s:
        s += "\n" + '\nstatic String jsonEscLteDebug(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteDebugJson()\n{\n    String csq = atCommand("AT+CSQ", 3000);\n    String cereg = atCommand("AT+CEREG?", 3000);\n    String cgatt = atCommand("AT+CGATT?", 3000);\n    String cgpaddr = atCommand("AT+CGPADDR=1", 3000);\n    String netopen = atCommand("AT+NETOPEN?", 3000);\n    String cipstatus = atCommand("AT+CIPSTATUS", 5000);\n\n    String json = "{";\n    json += "\\"modemReady\\":" + String(modemReadyFlag ? "true" : "false") + ",";\n    json += "\\"simReady\\":" + String(simReadyFlag ? "true" : "false") + ",";\n    json += "\\"networkRegistered\\":" + String(networkRegisteredFlag ? "true" : "false") + ",";\n    json += "\\"gprsAttached\\":" + String(gprsConnectedFlag ? "true" : "false") + ",";\n    json += "\\"pdpConfigured\\":" + String(pdpConfiguredFlag ? "true" : "false") + ",";\n    json += "\\"lteIp\\":\\"" + jsonEscLteDebug(lilygoLteIp()) + "\\",";\n    json += "\\"lteTcpConnected\\":" + String(lilygoLteTcpConnected() ? "true" : "false") + ",";\n    json += "\\"signal\\":\\"" + jsonEscLteDebug(csq) + "\\",";\n    json += "\\"registration\\":\\"" + jsonEscLteDebug(cereg) + "\\",";\n    json += "\\"gprs\\":\\"" + jsonEscLteDebug(cgatt) + "\\",";\n    json += "\\"ipInfo\\":\\"" + jsonEscLteDebug(cgpaddr) + "\\",";\n    json += "\\"netopen\\":\\"" + jsonEscLteDebug(netopen) + "\\",";\n    json += "\\"cipstatus\\":\\"" + jsonEscLteDebug(cipstatus) + "\\",";\n    json += "\\"lastAt\\":\\"" + jsonEscLteDebug(lastAt) + "\\",";\n    json += "\\"message\\":\\"" + jsonEscLteDebug(lastMessage) + "\\"";\n    json += "}";\n    return json;\n}\n' + "\n"
    write(modem_cpp, s)

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    s = read(web)
    if "handleLteDebug" not in s:
        handler = """
static void handleLteDebug()
{
    server.send(200, "application/json", lilygoLteDebugJson());
}

"""
        if "static void handleMqtt()" in s:
            s = s.replace("static void handleMqtt()", handler + "static void handleMqtt()", 1)
        else:
            s += "\n" + handler
    if 'server.on("/api/lilygo/lte/debug"' not in s:
        route = 'server.on("/api/lilygo/lte/debug", HTTP_GET, handleLteDebug);'
        if 'server.on("/api/lilygo/network"' in s:
            idx = s.find('server.on("/api/lilygo/network"')
            line_end = s.find("\n", idx)
            s = s[:line_end+1] + "    " + route + "\n" + s[line_end+1:]
        elif "server.begin();" in s:
            s = s.replace("server.begin();", route + "\n    server.begin();", 1)
    if "LTE Debug" not in s:
        s = s.replace(
            "<div class='card'><h2>LTE Modem</h2><button onclick='loadModem()'>Refresh</button><pre id='modem'>Loading...</pre></div>",
            "<div class='card'><h2>LTE Modem</h2><p><a href='/api/lilygo/lte/debug'>LTE Debug</a></p><button onclick='loadModem()'>Refresh</button><pre id='modem'>Loading...</pre></div>"
        )
    write(web, s)

cfg = root / "firmware/lilygo-t-a7670/src/config/lilygo_config.cpp"
if cfg.exists():
    s = read(cfg)
    if '"abrpApiKey"' not in s:
        s = s.replace(
            'json += ",\\"otaPassword\\":\\"" + esc(config.otaPassword) + "\\"";',
            'json += ",\\"otaPassword\\":\\"" + esc(config.otaPassword) + "\\"";\n        json += ",\\"abrpApiKey\\":\\"" + esc(config.abrpApiKey) + "\\"";\n        json += ",\\"abrpUserToken\\":\\"" + esc(config.abrpUserToken) + "\\"";'
        )
    if '"abrpEnabled"' not in s:
        s = s.replace(
            'json += "\\"otaEnabled\\":" + String(config.otaEnabled ? "true" : "false");',
            'json += "\\"otaEnabled\\":" + String(config.otaEnabled ? "true" : "false") + ",";\n    json += "\\"abrpEnabled\\":" + String(config.abrpEnabled ? "true" : "false");'
        )
    write(cfg, s)

print("MOT LilyGO LTE debug + backup ABRP fix applied.")
