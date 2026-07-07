from pathlib import Path
import re

root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

# ---------------------------------------------------------------------------
# 1) PlatformIO: use LewisXhe TinyGSM fork and A76XXSSL model.
# ---------------------------------------------------------------------------
pio = root / "firmware/lilygo-t-a7670/platformio.ini"
s = read(pio)

# Remove previous TinyGSM dependencies.
s = re.sub(r"\n\s*vshymanskyy/TinyGSM[^\n]*", "", s)
s = re.sub(r"\n\s*TinyGSM\s*@[^\n]*", "", s)
s = re.sub(r"\n\s*https://github\.com/lewisxhe/TinyGSM[^\n]*", "", s)

if "lib_deps" in s:
    # Add to first lib_deps block in this firmware environment.
    s = re.sub(
        r"(lib_deps\s*=\s*[^\n]*(?:\n\s+[^\n]+)*)",
        lambda m: m.group(1) + "\n  https://github.com/lewisxhe/TinyGSM",
        s,
        count=1
    )
else:
    s += "\nlib_deps =\n  https://github.com/lewisxhe/TinyGSM\n"

# Remove wrong/old TinyGSM model flags.
s = re.sub(r"\n\s*-D\s*TINY_GSM_MODEM_A7670[^\n]*", "", s)
s = re.sub(r"\n\s*-D\s*TINY_GSM_MODEM_A76XXSSL[^\n]*", "", s)
s = re.sub(r"\n\s*-D\s*TINY_GSM_USE_GPRS[^\n]*", "", s)

# Add flags into build_flags block, after MOT_BOARD if possible.
if "-D TINY_GSM_MODEM_A76XXSSL" not in s:
    if "build_flags" in s:
        s = re.sub(
            r"(build_flags\s*=\s*[^\n]*(?:\n\s+-D[^\n]*)*)",
            lambda m: m.group(1) + "\n  -D TINY_GSM_MODEM_A76XXSSL\n  -D TINY_GSM_USE_GPRS=true",
            s,
            count=1
        )
    else:
        s += "\nbuild_flags =\n  -D TINY_GSM_MODEM_A76XXSSL\n  -D TINY_GSM_USE_GPRS=true\n"

write(pio, s)

# ---------------------------------------------------------------------------
# 2) Replace LTE Client with LewisXhe TinyGSM-backed client.
# ---------------------------------------------------------------------------
lte_h = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.h"
write(lte_h, '#pragma once\n\n#include <Arduino.h>\n#include <Client.h>\n\nclass LilygoLteClient : public Client\n{\npublic:\n    int connect(IPAddress ip, uint16_t port) override;\n    int connect(const char *host, uint16_t port) override;\n\n    size_t write(uint8_t b) override;\n    size_t write(const uint8_t *buf, size_t size) override;\n\n    int available() override;\n    int read() override;\n    int read(uint8_t *buf, size_t size) override;\n    int peek() override;\n    void flush() override;\n    void stop() override;\n    uint8_t connected() override;\n    operator bool() override;\n\nprivate:\n    bool connectedFlag = false;\n};\n\nString lilygoLteClientTraceJson();\nvoid lilygoLteClientTraceClear();\nString lilygoTinyGsmStatusJson();\n')

lte_cpp = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.cpp"
write(lte_cpp, '#include "lte/lilygo_lte_client.h"\n\n#include <Arduino.h>\n\n#ifndef TINY_GSM_MODEM_A76XXSSL\n#define TINY_GSM_MODEM_A76XXSSL\n#endif\n\n#ifndef TINY_GSM_USE_GPRS\n#define TINY_GSM_USE_GPRS true\n#endif\n\n#include <TinyGsmClient.h>\n\n#include "board_config.h"\n#include "config/lilygo_config.h"\n#include "modem/lilygo_modem.h"\n\n#ifndef SerialAT\n#define SerialAT Serial1\n#endif\n\nstatic TinyGsm tinyModem(SerialAT);\nstatic TinyGsmClient tinyClient(tinyModem);\n\nstatic bool tinyModemReady = false;\nstatic bool tinyGprsReady = false;\nstatic uint32_t lastGprsCheckMs = 0;\n\nstatic String lteClientTrace;\nstatic uint32_t lteClientConnectCalls = 0;\nstatic uint32_t lteClientWriteCalls = 0;\nstatic uint32_t lteClientReadCalls = 0;\nstatic uint32_t lteClientAvailableCalls = 0;\nstatic uint32_t lteClientStopCalls = 0;\nstatic int lteClientLastAvailable = 0;\nstatic int lteClientLastRead = 0;\nstatic int lteClientLastWritten = 0;\nstatic bool lteClientLastConnected = false;\nstatic String tinyLastMessage;\n\nstatic void traceAppend(const String& line)\n{\n    lteClientTrace += String(millis()) + "ms " + line + "\\n";\n\n    if (lteClientTrace.length() > 5000) {\n        lteClientTrace.remove(0, lteClientTrace.length() - 5000);\n    }\n}\n\nstatic String traceEsc(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteClientTraceJson()\n{\n    String json = "{";\n    json += "\\"backend\\":\\"LewisXhe TinyGSM A76XXSSL\\",";\n    json += "\\"tinyModemReady\\":" + String(tinyModemReady ? "true" : "false") + ",";\n    json += "\\"tinyGprsReady\\":" + String(tinyGprsReady ? "true" : "false") + ",";\n    json += "\\"connectCalls\\":" + String(lteClientConnectCalls) + ",";\n    json += "\\"writeCalls\\":" + String(lteClientWriteCalls) + ",";\n    json += "\\"readCalls\\":" + String(lteClientReadCalls) + ",";\n    json += "\\"availableCalls\\":" + String(lteClientAvailableCalls) + ",";\n    json += "\\"stopCalls\\":" + String(lteClientStopCalls) + ",";\n    json += "\\"lastAvailable\\":" + String(lteClientLastAvailable) + ",";\n    json += "\\"lastRead\\":" + String(lteClientLastRead) + ",";\n    json += "\\"lastWritten\\":" + String(lteClientLastWritten) + ",";\n    json += "\\"lastConnected\\":" + String(lteClientLastConnected ? "true" : "false") + ",";\n    json += "\\"localIp\\":\\"" + traceEsc(tinyModemReady ? tinyModem.getLocalIP() : String("")) + "\\",";\n    json += "\\"message\\":\\"" + traceEsc(tinyLastMessage) + "\\",";\n    json += "\\"trace\\":\\"" + traceEsc(lteClientTrace) + "\\"";\n    json += "}";\n    return json;\n}\n\nString lilygoTinyGsmStatusJson()\n{\n    return lilygoLteClientTraceJson();\n}\n\nvoid lilygoLteClientTraceClear()\n{\n    lteClientTrace = "";\n    lteClientConnectCalls = 0;\n    lteClientWriteCalls = 0;\n    lteClientReadCalls = 0;\n    lteClientAvailableCalls = 0;\n    lteClientStopCalls = 0;\n    lteClientLastAvailable = 0;\n    lteClientLastRead = 0;\n    lteClientLastWritten = 0;\n    lteClientLastConnected = false;\n}\n\nstatic bool ensureTinyModemReady()\n{\n    if (tinyModemReady) {\n        return true;\n    }\n\n    traceAppend("TinyGSM init");\n\n    // The MOT modem setup already powers the A7670 and starts SerialAT.\n    // TinyGSM init is still required so the A76xx socket backend initializes.\n    tinyModemReady = tinyModem.init();\n\n    if (!tinyModemReady) {\n        tinyLastMessage = "TinyGSM modem init failed";\n        traceAppend(tinyLastMessage);\n        return false;\n    }\n\n    String modemName = tinyModem.getModemName();\n    String modemInfo = tinyModem.getModemInfo();\n\n    traceAppend("TinyGSM modem name=" + modemName);\n    traceAppend("TinyGSM modem info=" + modemInfo);\n\n    tinyLastMessage = "TinyGSM modem ready";\n    return true;\n}\n\nstatic bool ensureTinyGprsReady()\n{\n    if (!ensureTinyModemReady()) {\n        return false;\n    }\n\n    // Avoid hammering the modem. TinyGSM checks are AT commands and share the\n    // UART with the live socket.\n    if (tinyGprsReady && millis() - lastGprsCheckMs < 30000) {\n        return true;\n    }\n\n    lastGprsCheckMs = millis();\n\n    if (tinyModem.isGprsConnected()) {\n        tinyGprsReady = true;\n        tinyLastMessage = "TinyGSM GPRS already connected";\n        return true;\n    }\n\n    traceAppend("TinyGSM waitForNetwork");\n    if (!tinyModem.waitForNetwork(60000L)) {\n        tinyGprsReady = false;\n        tinyLastMessage = "TinyGSM network wait failed";\n        traceAppend(tinyLastMessage);\n        return false;\n    }\n\n    traceAppend("TinyGSM gprsConnect apn=" + config.lteApn);\n\n    tinyGprsReady = tinyModem.gprsConnect(\n        config.lteApn.c_str(),\n        config.lteUser.c_str(),\n        config.ltePass.c_str()\n    );\n\n    if (!tinyGprsReady) {\n        tinyLastMessage = "TinyGSM gprsConnect failed";\n        traceAppend(tinyLastMessage);\n        return false;\n    }\n\n    tinyLastMessage = "TinyGSM GPRS connected ip=" + tinyModem.getLocalIP();\n    traceAppend(tinyLastMessage);\n    return true;\n}\n\nint LilygoLteClient::connect(const char *host, uint16_t port)\n{\n    lteClientConnectCalls++;\n    traceAppend("connect host=" + String(host) + " port=" + String(port));\n\n    if (!ensureTinyGprsReady()) {\n        connectedFlag = false;\n        lteClientLastConnected = false;\n        traceAppend("connect failed before TCP");\n        return 0;\n    }\n\n    connectedFlag = tinyClient.connect(host, port);\n    lteClientLastConnected = connectedFlag;\n\n    traceAppend(String("TinyGSM TCP connect result=") + (connectedFlag ? "1" : "0"));\n\n    if (!connectedFlag) {\n        tinyLastMessage = "TinyGSM TCP connect failed";\n    }\n\n    return connectedFlag ? 1 : 0;\n}\n\nint LilygoLteClient::connect(IPAddress ip, uint16_t port)\n{\n    char host[24];\n    snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);\n    return connect(host, port);\n}\n\nsize_t LilygoLteClient::write(uint8_t b)\n{\n    return write(&b, 1);\n}\n\nsize_t LilygoLteClient::write(const uint8_t *buf, size_t size)\n{\n    lteClientWriteCalls++;\n\n    if (!connectedFlag) {\n        lteClientLastWritten = 0;\n        traceAppend("write skipped not-connected size=" + String(size));\n        return 0;\n    }\n\n    size_t written = tinyClient.write(buf, size);\n    lteClientLastWritten = (int)written;\n\n    traceAppend("write size=" + String(size) + " result=" + String((int)written));\n\n    if (written == 0) {\n        connectedFlag = tinyClient.connected();\n        lteClientLastConnected = connectedFlag;\n    }\n\n    return written;\n}\n\nint LilygoLteClient::available()\n{\n    lteClientAvailableCalls++;\n\n    int available = connectedFlag ? tinyClient.available() : 0;\n    lteClientLastAvailable = available;\n\n    static uint32_t lastTraceMs = 0;\n    if (available > 0 || millis() - lastTraceMs > 1000) {\n        traceAppend("available result=" + String(available));\n        lastTraceMs = millis();\n    }\n\n    return available;\n}\n\nint LilygoLteClient::read()\n{\n    uint8_t b = 0;\n    int n = read(&b, 1);\n    return n == 1 ? b : -1;\n}\n\nint LilygoLteClient::read(uint8_t *buf, size_t size)\n{\n    lteClientReadCalls++;\n\n    if (!connectedFlag) {\n        lteClientLastRead = 0;\n        traceAppend("read skipped not-connected size=" + String(size));\n        return 0;\n    }\n\n    int n = tinyClient.read(buf, size);\n    lteClientLastRead = n;\n\n    traceAppend("read size=" + String(size) + " result=" + String(n));\n\n    if (n < 0) {\n        connectedFlag = tinyClient.connected();\n        lteClientLastConnected = connectedFlag;\n        return 0;\n    }\n\n    return n;\n}\n\nint LilygoLteClient::peek()\n{\n    return tinyClient.peek();\n}\n\nvoid LilygoLteClient::flush()\n{\n    tinyClient.flush();\n}\n\nvoid LilygoLteClient::stop()\n{\n    lteClientStopCalls++;\n    traceAppend("stop");\n\n    tinyClient.stop();\n    connectedFlag = false;\n    lteClientLastConnected = false;\n}\n\nuint8_t LilygoLteClient::connected()\n{\n    connectedFlag = tinyClient.connected();\n    lteClientLastConnected = connectedFlag;\n    return connectedFlag ? 1 : 0;\n}\n\nLilygoLteClient::operator bool()\n{\n    return connected();\n}\n')

# ---------------------------------------------------------------------------
# 3) Web UI trace endpoint, optional but useful.
# ---------------------------------------------------------------------------
web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    w = read(web)

    if '#include "lte/lilygo_lte_client.h"' not in w:
        lines = w.splitlines()
        insert_at = 0
        for i, line in enumerate(lines):
            if line.startswith("#include"):
                insert_at = i + 1
        lines.insert(insert_at, '#include "lte/lilygo_lte_client.h"')
        w = "\n".join(lines) + "\n"

    if "handleLteMqttTrace" not in w:
        handler = """
static void handleLteMqttTrace()
{
    server.send(200, "application/json", lilygoLteClientTraceJson());
}

static void handleLteMqttTraceClear()
{
    lilygoLteClientTraceClear();
    server.send(200, "application/json", lilygoLteClientTraceJson());
}

"""
        if "static void handleMqtt()" in w:
            w = w.replace("static void handleMqtt()", handler + "static void handleMqtt()", 1)
        else:
            w += "\n" + handler

    if 'server.on("/api/lilygo/lte/mqtt-trace"' not in w:
        routes = (
            'server.on("/api/lilygo/lte/mqtt-trace", HTTP_GET, handleLteMqttTrace);\n'
            '    server.on("/api/lilygo/lte/mqtt-trace/clear", HTTP_POST, handleLteMqttTraceClear);'
        )
        if 'server.on("/api/lilygo/mqtt"' in w:
            idx = w.find('server.on("/api/lilygo/mqtt"')
            line_end = w.find("\n", idx)
            w = w[:line_end+1] + "    " + routes + "\n" + w[line_end+1:]
        elif "server.begin();" in w:
            w = w.replace("server.begin();", routes + "\n    server.begin();", 1)

    write(web, w)

print("Applied MOT LilyGO LewisXhe TinyGSM A76XX transport.")
