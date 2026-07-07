from pathlib import Path
root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

h = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.h"
if h.exists():
    s = read(h)
    if "lilygoLteTcpTestJson" not in s:
        s += "\nString lilygoLteTcpTestJson(const String& host, uint16_t port);\n"
    write(h, s)

cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
if cpp.exists():
    s = read(cpp)
    if "jsonEscLteTcpTest" not in s:
        s += "\n" + '\nstatic String jsonEscLteTcpTest(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteTcpTestJson(const String& host, uint16_t port)\n{\n    unsigned long start = millis();\n\n    String closeResp = atCommand("AT+CIPCLOSE=0", 3000);\n    String netOpenResp = atCommand("AT+NETOPEN", 10000);\n    String netOpenQuery = atCommand("AT+NETOPEN?", 3000);\n\n    String cmd = "AT+CIPOPEN=0,\\"TCP\\",\\"" + host + "\\"," + String(port);\n    String openResp = atCommand(cmd, 25000);\n\n    unsigned long elapsed = millis() - start;\n\n    bool tcpOpen =\n        openResp.indexOf("+CIPOPEN: 0,0") >= 0 ||\n        openResp.indexOf("OK") >= 0 ||\n        openResp.indexOf("ALREADY") >= 0;\n\n    String cipStatus = atCommand("AT+CIPSTATUS", 5000);\n    String closeAfter = atCommand("AT+CIPCLOSE=0", 3000);\n    lteTcpOpenFlag = false;\n\n    String json = "{";\n    json += "\\"host\\":\\"" + jsonEscLteTcpTest(host) + "\\",";\n    json += "\\"port\\":" + String(port) + ",";\n    json += "\\"tcpOpen\\":" + String(tcpOpen ? "true" : "false") + ",";\n    json += "\\"elapsedMs\\":" + String(elapsed) + ",";\n    json += "\\"modemReady\\":" + String(modemReadyFlag ? "true" : "false") + ",";\n    json += "\\"simReady\\":" + String(simReadyFlag ? "true" : "false") + ",";\n    json += "\\"networkRegistered\\":" + String(networkRegisteredFlag ? "true" : "false") + ",";\n    json += "\\"gprsAttached\\":" + String(gprsAttachedFlag ? "true" : "false") + ",";\n    json += "\\"pdpConfigured\\":" + String(pdpConfiguredFlag ? "true" : "false") + ",";\n    json += "\\"lteIp\\":\\"" + jsonEscLteTcpTest(lilygoLteIp()) + "\\",";\n    json += "\\"closeBefore\\":\\"" + jsonEscLteTcpTest(closeResp) + "\\",";\n    json += "\\"netOpen\\":\\"" + jsonEscLteTcpTest(netOpenResp) + "\\",";\n    json += "\\"netOpenQuery\\":\\"" + jsonEscLteTcpTest(netOpenQuery) + "\\",";\n    json += "\\"cipOpen\\":\\"" + jsonEscLteTcpTest(openResp) + "\\",";\n    json += "\\"cipStatus\\":\\"" + jsonEscLteTcpTest(cipStatus) + "\\",";\n    json += "\\"closeAfter\\":\\"" + jsonEscLteTcpTest(closeAfter) + "\\",";\n    json += "\\"lastAt\\":\\"" + jsonEscLteTcpTest(lastAt) + "\\"";\n    json += "}";\n\n    return json;\n}\n' + "\n"
    write(cpp, s)

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    s = read(web)

    if "handleLteTcpTest" not in s:
        handler = """
static void handleLteTcpTest()
{
    String host = server.hasArg("host") ? server.arg("host") : config.mqttHost;
    uint16_t port = server.hasArg("port") ? (uint16_t)server.arg("port").toInt() : config.mqttPort;

    host.trim();

    if (host.isEmpty() || port == 0) {
        server.send(400, "application/json", "{\"error\":\"missing host or port\"}");
        return;
    }

    server.send(200, "application/json", lilygoLteTcpTestJson(host, port));
}

"""
        if "static void handleLteDebug()" in s:
            s = s.replace("static void handleLteDebug()", handler + "static void handleLteDebug()", 1)
        elif "static void handleMqtt()" in s:
            s = s.replace("static void handleMqtt()", handler + "static void handleMqtt()", 1)
        else:
            s += "\n" + handler

    if 'server.on("/api/lilygo/lte/tcp-test"' not in s:
        route1 = 'server.on("/api/lilygo/lte/tcp-test", HTTP_GET, handleLteTcpTest);'
        route2 = 'server.on("/api/lilygo/lte/tcp-test", HTTP_POST, handleLteTcpTest);'
        if 'server.on("/api/lilygo/lte/debug"' in s:
            idx = s.find('server.on("/api/lilygo/lte/debug"')
            line_end = s.find("\n", idx)
            s = s[:line_end+1] + "    " + route1 + "\n    " + route2 + "\n" + s[line_end+1:]
        elif "server.begin();" in s:
            s = s.replace("server.begin();", route1 + "\n    " + route2 + "\n    server.begin();", 1)

    if "LTE TCP Test" not in s:
        s = s.replace(
            "<p><a href='/api/lilygo/lte/debug'>LTE Debug</a></p>",
            "<p><a href='/api/lilygo/lte/debug'>LTE Debug</a> · <a href='/api/lilygo/lte/tcp-test'>LTE TCP Test</a></p>"
        )

    write(web, s)

print("MOT LilyGO LTE TCP test endpoint applied.")
