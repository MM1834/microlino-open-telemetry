from pathlib import Path
import re

root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

# ---------------------------------------------------------------------------
# 1) Add TinyGSM dependency.
# ---------------------------------------------------------------------------
pio = root / "firmware/lilygo-t-a7670/platformio.ini"
if pio.exists():
    s = read(pio)
    if "vshymanskyy/TinyGSM" not in s:
        if "lib_deps" in s:
            s = re.sub(
                r"(lib_deps\s*=\s*[^\n]*(?:\n\s+[^\n]+)*)",
                lambda m: m.group(1) + "\n    vshymanskyy/TinyGSM@^0.12.0",
                s,
                count=1
            )
        else:
            s += "\nlib_deps =\n    vshymanskyy/TinyGSM@^0.12.0\n"
    write(pio, s)

# ---------------------------------------------------------------------------
# 2) Replace LTE Client implementation with TinyGSM-backed client.
# ---------------------------------------------------------------------------
lte_cpp = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.cpp"
write(lte_cpp, '#include "lilygo_lte_client.h"\n\n#include <Arduino.h>\n\n#ifndef TINY_GSM_MODEM_A7670\n#define TINY_GSM_MODEM_A7670\n#endif\n#define TINY_GSM_USE_GPRS true\n\n#include <TinyGsmClient.h>\n\n#include "board_config.h"\n#include "config/lilygo_config.h"\n#include "modem/lilygo_modem.h"\n\nstatic TinyGsm tinyModem(SerialAT);\nstatic TinyGsmClient tinyClient(tinyModem);\n\nstatic bool tinyModemReady = false;\nstatic String lteClientTrace;\nstatic uint32_t lteClientConnectCalls = 0;\nstatic uint32_t lteClientWriteCalls = 0;\nstatic uint32_t lteClientReadCalls = 0;\nstatic uint32_t lteClientAvailableCalls = 0;\nstatic uint32_t lteClientStopCalls = 0;\nstatic int lteClientLastAvailable = 0;\nstatic int lteClientLastRead = 0;\nstatic int lteClientLastWritten = 0;\nstatic bool lteClientLastConnected = false;\n\nstatic void traceAppend(const String& line)\n{\n    lteClientTrace += String(millis()) + "ms " + line + "\\n";\n\n    if (lteClientTrace.length() > 3500) {\n        lteClientTrace.remove(0, lteClientTrace.length() - 3500);\n    }\n}\n\nstatic String traceEsc(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteClientTraceJson()\n{\n    String json = "{";\n    json += "\\"backend\\":\\"TinyGSM\\",";\n    json += "\\"tinyModemReady\\":" + String(tinyModemReady ? "true" : "false") + ",";\n    json += "\\"connectCalls\\":" + String(lteClientConnectCalls) + ",";\n    json += "\\"writeCalls\\":" + String(lteClientWriteCalls) + ",";\n    json += "\\"readCalls\\":" + String(lteClientReadCalls) + ",";\n    json += "\\"availableCalls\\":" + String(lteClientAvailableCalls) + ",";\n    json += "\\"stopCalls\\":" + String(lteClientStopCalls) + ",";\n    json += "\\"lastAvailable\\":" + String(lteClientLastAvailable) + ",";\n    json += "\\"lastRead\\":" + String(lteClientLastRead) + ",";\n    json += "\\"lastWritten\\":" + String(lteClientLastWritten) + ",";\n    json += "\\"lastConnected\\":" + String(lteClientLastConnected ? "true" : "false") + ",";\n    json += "\\"trace\\":\\"" + traceEsc(lteClientTrace) + "\\"";\n    json += "}";\n    return json;\n}\n\nvoid lilygoLteClientTraceClear()\n{\n    lteClientTrace = "";\n    lteClientConnectCalls = 0;\n    lteClientWriteCalls = 0;\n    lteClientReadCalls = 0;\n    lteClientAvailableCalls = 0;\n    lteClientStopCalls = 0;\n    lteClientLastAvailable = 0;\n    lteClientLastRead = 0;\n    lteClientLastWritten = 0;\n    lteClientLastConnected = false;\n}\n\nstatic bool ensureTinyGsmReady()\n{\n    if (tinyModemReady) return true;\n\n    traceAppend("TinyGSM init");\n\n    tinyModemReady = tinyModem.init();\n\n    if (!tinyModemReady) {\n        traceAppend("TinyGSM init failed");\n        return false;\n    }\n\n    traceAppend("TinyGSM init OK");\n\n    return true;\n}\n\nstatic bool ensureTinyGsmGprs()\n{\n    if (!ensureTinyGsmReady()) return false;\n\n    if (!lilygoGprsConnected()) {\n        traceAppend("MOT modem reports GPRS not attached");\n    }\n\n    // TinyGSM keeps its own internal socket/GPRS state. Even if the MOT AT\n    // layer already attached PDP, TinyGSM must still run its gprsConnect path\n    // once so its client backend is initialized consistently.\n    traceAppend("TinyGSM gprsConnect apn=" + config.lteApn);\n\n    bool ok = tinyModem.gprsConnect(\n        config.lteApn.c_str(),\n        config.lteUser.c_str(),\n        config.ltePass.c_str()\n    );\n\n    traceAppend(String("TinyGSM gprsConnect result=") + (ok ? "1" : "0"));\n\n    return ok;\n}\n\nint LilygoLteClient::connect(const char *host, uint16_t port)\n{\n    lteClientConnectCalls++;\n    traceAppend("connect host=" + String(host) + " port=" + String(port));\n\n    if (!ensureTinyGsmGprs()) {\n        connectedFlag = false;\n        lteClientLastConnected = false;\n        traceAppend("connect failed before TCP");\n        return 0;\n    }\n\n    connectedFlag = tinyClient.connect(host, port);\n    lteClientLastConnected = connectedFlag;\n\n    traceAppend(String("TinyGSM TCP connect result=") + (connectedFlag ? "1" : "0"));\n\n    return connectedFlag ? 1 : 0;\n}\n\nint LilygoLteClient::connect(IPAddress ip, uint16_t port)\n{\n    char host[24];\n    snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);\n    return connect(host, port);\n}\n\nsize_t LilygoLteClient::write(uint8_t b)\n{\n    return write(&b, 1);\n}\n\nsize_t LilygoLteClient::write(const uint8_t *buf, size_t size)\n{\n    lteClientWriteCalls++;\n\n    if (!connectedFlag) {\n        lteClientLastWritten = 0;\n        traceAppend("write skipped not-connected size=" + String(size));\n        return 0;\n    }\n\n    traceAppend("write size=" + String(size));\n\n    size_t written = tinyClient.write(buf, size);\n    lteClientLastWritten = (int)written;\n\n    traceAppend("write result=" + String((int)written));\n\n    if (written == 0) {\n        connectedFlag = tinyClient.connected();\n        lteClientLastConnected = connectedFlag;\n        traceAppend(String("write connected-state=") + (connectedFlag ? "1" : "0"));\n    }\n\n    return written;\n}\n\nint LilygoLteClient::available()\n{\n    lteClientAvailableCalls++;\n\n    int available = connectedFlag ? tinyClient.available() : 0;\n    lteClientLastAvailable = available;\n\n    // Avoid flooding the ring buffer. PubSubClient polls this very often.\n    static uint32_t lastAvailTraceMs = 0;\n    if (available > 0 || millis() - lastAvailTraceMs > 1000) {\n        traceAppend("available result=" + String(available));\n        lastAvailTraceMs = millis();\n    }\n\n    return available;\n}\n\nint LilygoLteClient::read()\n{\n    uint8_t b;\n    int n = read(&b, 1);\n    return n == 1 ? b : -1;\n}\n\nint LilygoLteClient::read(uint8_t *buf, size_t size)\n{\n    lteClientReadCalls++;\n\n    if (!connectedFlag) {\n        lteClientLastRead = 0;\n        traceAppend("read skipped not-connected size=" + String(size));\n        return 0;\n    }\n\n    int n = tinyClient.read(buf, size);\n    lteClientLastRead = n;\n\n    traceAppend("read size=" + String(size) + " result=" + String(n));\n\n    if (n < 0) {\n        connectedFlag = tinyClient.connected();\n        lteClientLastConnected = connectedFlag;\n    }\n\n    return n < 0 ? 0 : n;\n}\n\nint LilygoLteClient::peek()\n{\n    return -1;\n}\n\nvoid LilygoLteClient::flush()\n{\n    tinyClient.flush();\n}\n\nvoid LilygoLteClient::stop()\n{\n    lteClientStopCalls++;\n    traceAppend("stop");\n\n    tinyClient.stop();\n    connectedFlag = false;\n    lteClientLastConnected = false;\n}\n\nuint8_t LilygoLteClient::connected()\n{\n    connectedFlag = tinyClient.connected();\n    lteClientLastConnected = connectedFlag;\n    return connectedFlag ? 1 : 0;\n}\n\nLilygoLteClient::operator bool()\n{\n    return connected();\n}\n')

# ---------------------------------------------------------------------------
# 3) Ensure trace declarations exist in header.
# ---------------------------------------------------------------------------
lte_h = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.h"
if lte_h.exists():
    s = read(lte_h)
    if "lilygoLteClientTraceJson" not in s:
        s += "\nString lilygoLteClientTraceJson();\nvoid lilygoLteClientTraceClear();\n"
    write(lte_h, s)

# ---------------------------------------------------------------------------
# 4) Disable legacy AT LTE TCP status from confusing MQTT trace. The old
#    endpoint can stay; TinyGSM is now the source of truth for MQTT socket IO.
# ---------------------------------------------------------------------------
print("Applied TinyGSM-backed LilyGO LTE client.")
