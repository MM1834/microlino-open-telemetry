from pathlib import Path
import re

root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

# Must be run from repo root.
fw = root / "firmware/lilygo-t-a7670"
if not fw.exists():
    raise SystemExit("Run from repository root, expected firmware/lilygo-t-a7670")

pio = fw / "platformio.ini"
s = read(pio)

# Remove old TinyGSM deps and flags.
s = re.sub(r"\n\s*vshymanskyy/TinyGSM[^\n]*", "", s)
s = re.sub(r"\n\s*TinyGSM\s*@[^\n]*", "", s)
s = re.sub(r"\n\s*https://github\.com/lewisxhe/TinyGSM[^\n]*", "", s)
s = re.sub(r"\n\s*-D\s*TINY_GSM_MODEM_A7670[^\n]*", "", s)
s = re.sub(r"\n\s*-D\s*TINY_GSM_MODEM_A76XXSSL[^\n]*", "", s)
s = re.sub(r"\n\s*-D\s*TINY_GSM_USE_GPRS[^\n]*", "", s)

if "lib_deps" in s:
    s = re.sub(
        r"(lib_deps\s*=\s*[^\n]*(?:\n\s+[^\n]+)*)",
        lambda m: m.group(1).rstrip() + "\n  https://github.com/lewisxhe/TinyGSM",
        s,
        count=1
    )
else:
    s += "\nlib_deps =\n  https://github.com/lewisxhe/TinyGSM\n"

if "build_flags" in s:
    s = re.sub(
        r"(build_flags\s*=\s*[^\n]*(?:\n\s+[^\n]+)*)",
        lambda m: m.group(1).rstrip() + "\n  -D TINY_GSM_MODEM_A76XXSSL\n  -D TINY_GSM_USE_GPRS=true",
        s,
        count=1
    )
else:
    s += "\nbuild_flags =\n  -D TINY_GSM_MODEM_A76XXSSL\n  -D TINY_GSM_USE_GPRS=true\n"

write(pio, s)

# Replace the modem and LTE client with one shared LewisXhe TinyGSM stack.
write(fw / "src/modem/lilygo_modem.h", '#pragma once\n\n#include <Arduino.h>\n#include <Client.h>\n\nvoid setupLilygoModem();\n\nbool lilygoGprsConnected();\nbool lilygoEnsureGprsConnected();\n\nString lilygoLteIp();\nString lilygoModemStatusJson();\nString lilygoLteDebugJson();\n\nbool lilygoLteTcpOpen(const String& host, uint16_t port);\nint lilygoLteTcpWrite(const uint8_t* data, size_t len);\nint lilygoLteTcpAvailable();\nint lilygoLteTcpRead(uint8_t* buffer, size_t len);\nbool lilygoLteTcpConnected();\nvoid lilygoLteTcpClose();\n\nString lilygoLteTcpTestJson(const String& host, uint16_t port);\nString lilygoLteRxDebugJson();\n\nClient* lilygoTinyGsmClient();\nString lilygoTinyGsmTraceJson();\nvoid lilygoTinyGsmTraceClear();\n')
write(fw / "src/modem/lilygo_modem.cpp", '#include "lilygo_modem.h"\n\n#include <Arduino.h>\n#include <Client.h>\n\n#ifndef TINY_GSM_MODEM_A76XXSSL\n#define TINY_GSM_MODEM_A76XXSSL\n#endif\n\n#ifndef TINY_GSM_USE_GPRS\n#define TINY_GSM_USE_GPRS true\n#endif\n\n#include <TinyGsmClient.h>\n\n#include "board_config.h"\n#include "config/lilygo_config.h"\n\n#ifndef SerialAT\n#define SerialAT Serial1\n#endif\n\n#ifndef MODEM_BAUD\n#define MODEM_BAUD 115200\n#endif\n\nstatic TinyGsm modem(SerialAT);\nstatic TinyGsmClient lteClient(modem);\n\nstatic bool modemReadyFlag = false;\nstatic bool networkReadyFlag = false;\nstatic bool gprsReadyFlag = false;\nstatic bool tcpOpenFlag = false;\n\nstatic String lastMessage = "";\nstatic String lastTrace = "";\nstatic uint32_t lastGprsEnsureMs = 0;\nstatic uint32_t connectAttempts = 0;\nstatic uint32_t tcpOpenCount = 0;\nstatic uint32_t tcpFailCount = 0;\nstatic uint32_t bytesWritten = 0;\nstatic uint32_t bytesRead = 0;\n\nstatic void traceAppend(const String& line)\n{\n    lastTrace += String(millis()) + "ms " + line + "\\n";\n\n    if (lastTrace.length() > 5000) {\n        lastTrace.remove(0, lastTrace.length() - 5000);\n    }\n}\n\nstatic String esc(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nstatic void setupPins()\n{\n#ifdef BOARD_POWER_ON_PIN\n    pinMode(BOARD_POWER_ON_PIN, OUTPUT);\n    digitalWrite(BOARD_POWER_ON_PIN, HIGH);\n#endif\n\n#ifdef MODEM_PWR_PIN\n    pinMode(MODEM_PWR_PIN, OUTPUT);\n#endif\n\n#ifdef MODEM_PWRKEY_PIN\n    pinMode(MODEM_PWRKEY_PIN, OUTPUT);\n#endif\n\n#ifdef MODEM_RST_PIN\n    pinMode(MODEM_RST_PIN, OUTPUT);\n    digitalWrite(MODEM_RST_PIN, HIGH);\n    delay(100);\n    digitalWrite(MODEM_RST_PIN, LOW);\n    delay(2600);\n    digitalWrite(MODEM_RST_PIN, HIGH);\n#endif\n\n#ifdef MODEM_DTR_PIN\n    pinMode(MODEM_DTR_PIN, OUTPUT);\n    digitalWrite(MODEM_DTR_PIN, LOW);\n#endif\n}\n\nstatic void powerKeyPulse()\n{\n#ifdef MODEM_PWR_PIN\n    digitalWrite(MODEM_PWR_PIN, LOW);\n    delay(100);\n    digitalWrite(MODEM_PWR_PIN, HIGH);\n    delay(1000);\n    digitalWrite(MODEM_PWR_PIN, LOW);\n#elif defined(MODEM_PWRKEY_PIN)\n    digitalWrite(MODEM_PWRKEY_PIN, LOW);\n    delay(100);\n    digitalWrite(MODEM_PWRKEY_PIN, HIGH);\n    delay(1000);\n    digitalWrite(MODEM_PWRKEY_PIN, LOW);\n#endif\n}\n\nstatic bool initTinyGsmModem()\n{\n    Serial.println("Initializing modem with LewisXhe TinyGSM A76XXSSL...");\n    traceAppend("modem.init");\n\n    modemReadyFlag = modem.init();\n\n    if (!modemReadyFlag) {\n        lastMessage = "TinyGSM modem init failed";\n        traceAppend(lastMessage);\n        return false;\n    }\n\n    String name = modem.getModemName();\n    String info = modem.getModemInfo();\n\n    Serial.printf("Modem Name: %s\\n", name.c_str());\n    Serial.printf("Modem Info: %s\\n", info.c_str());\n\n    traceAppend("modem name=" + name);\n    traceAppend("modem info=" + info);\n\n    lastMessage = "TinyGSM modem ready";\n    return true;\n}\n\nstatic bool connectNetworkAndGprs(uint32_t timeoutMs = 60000)\n{\n    if (!modemReadyFlag && !initTinyGsmModem()) {\n        return false;\n    }\n\n    connectAttempts++;\n\n#if defined(TINY_GSM_MODEM_HAS_NETWORK_MODE)\n    // Not all TinyGSM versions expose this symbol; keep disabled by default.\n#endif\n\n    Serial.print("Waiting for network...");\n    traceAppend("waitForNetwork");\n\n    networkReadyFlag = modem.waitForNetwork(timeoutMs);\n\n    if (!networkReadyFlag) {\n        Serial.println("fail");\n        lastMessage = "TinyGSM network wait failed";\n        traceAppend(lastMessage);\n        return false;\n    }\n\n    Serial.println("success");\n    traceAppend("network connected");\n\n    if (config.lteApn.isEmpty()) {\n        lastMessage = "No LTE APN configured";\n        traceAppend(lastMessage);\n        return false;\n    }\n\n    Serial.printf("Connecting to %s...", config.lteApn.c_str());\n    traceAppend("gprsConnect apn=" + config.lteApn);\n\n    gprsReadyFlag = modem.gprsConnect(\n        config.lteApn.c_str(),\n        config.lteUser.c_str(),\n        config.ltePass.c_str()\n    );\n\n    if (!gprsReadyFlag) {\n        Serial.println("fail");\n        lastMessage = "TinyGSM GPRS connect failed";\n        traceAppend(lastMessage);\n        return false;\n    }\n\n    Serial.println("success");\n    Serial.printf("IP Address: %s\\n", modem.getLocalIP().c_str());\n\n    lastGprsEnsureMs = millis();\n    lastMessage = "TinyGSM GPRS connected ip=" + modem.getLocalIP();\n    traceAppend(lastMessage);\n\n    return true;\n}\n\nvoid setupLilygoModem()\n{\n    Serial.println("LilyGO T-A7670G: LTE modem setup");\n    Serial.printf(\n        "Modem pins RX=%d TX=%d PWR=%d POWER_ON=%d RST=%d DTR=%d RI=%d\\n",\n        MODEM_RX_PIN,\n        MODEM_TX_PIN,\n#ifdef MODEM_PWR_PIN\n        MODEM_PWR_PIN,\n#else\n        -1,\n#endif\n#ifdef BOARD_POWER_ON_PIN\n        BOARD_POWER_ON_PIN,\n#else\n        -1,\n#endif\n#ifdef MODEM_RST_PIN\n        MODEM_RST_PIN,\n#else\n        -1,\n#endif\n#ifdef MODEM_DTR_PIN\n        MODEM_DTR_PIN,\n#else\n        -1,\n#endif\n#ifdef MODEM_RI_PIN\n        MODEM_RI_PIN\n#else\n        -1\n#endif\n    );\n\n    setupPins();\n\n    SerialAT.end();\n    delay(200);\n    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);\n    delay(300);\n\n    powerKeyPulse();\n    delay(3000);\n\n    if (!initTinyGsmModem()) {\n        Serial.println("TinyGSM init failed, retry power pulse...");\n        powerKeyPulse();\n        delay(5000);\n        initTinyGsmModem();\n    }\n\n    if (modemReadyFlag) {\n        connectNetworkAndGprs(60000);\n    }\n}\n\nbool lilygoEnsureGprsConnected()\n{\n    if (!modemReadyFlag && !initTinyGsmModem()) {\n        return false;\n    }\n\n    if (gprsReadyFlag && modem.isGprsConnected()) {\n        lastGprsEnsureMs = millis();\n        return true;\n    }\n\n    // Do not hammer reconnects; PubSubClient may retry often.\n    if (millis() - lastGprsEnsureMs < 5000 && gprsReadyFlag) {\n        return true;\n    }\n\n    return connectNetworkAndGprs(60000);\n}\n\nbool lilygoGprsConnected()\n{\n    if (!modemReadyFlag) return false;\n\n    bool ok = modem.isGprsConnected();\n\n    if (!ok) {\n        gprsReadyFlag = false;\n    }\n\n    return ok;\n}\n\nString lilygoLteIp()\n{\n    if (!modemReadyFlag) return "";\n    if (!gprsReadyFlag && !modem.isGprsConnected()) return "";\n    return modem.getLocalIP();\n}\n\nClient* lilygoTinyGsmClient()\n{\n    return &lteClient;\n}\n\nbool lilygoLteTcpOpen(const String& host, uint16_t port)\n{\n    if (!lilygoEnsureGprsConnected()) {\n        tcpOpenFlag = false;\n        tcpFailCount++;\n        lastMessage = "TCP open failed: GPRS unavailable";\n        traceAppend(lastMessage);\n        return false;\n    }\n\n    tcpOpenFlag = false;\n    lteClient.stop();\n\n    traceAppend("TCP connect host=" + host + " port=" + String(port));\n\n    uint32_t start = millis();\n    tcpOpenFlag = lteClient.connect(host.c_str(), port);\n    uint32_t elapsed = millis() - start;\n\n    if (tcpOpenFlag) {\n        tcpOpenCount++;\n        lastMessage = "TCP connected in " + String(elapsed) + "ms";\n    } else {\n        tcpFailCount++;\n        lastMessage = "TCP connect failed in " + String(elapsed) + "ms";\n    }\n\n    traceAppend(lastMessage);\n    return tcpOpenFlag;\n}\n\nint lilygoLteTcpWrite(const uint8_t* data, size_t len)\n{\n    if (!tcpOpenFlag || !data || len == 0) return 0;\n\n    size_t n = lteClient.write(data, len);\n    bytesWritten += n;\n\n    traceAppend("TCP write len=" + String(len) + " result=" + String((int)n));\n\n    if (n == 0) {\n        tcpOpenFlag = lteClient.connected();\n    }\n\n    return (int)n;\n}\n\nint lilygoLteTcpAvailable()\n{\n    if (!tcpOpenFlag) return 0;\n    return lteClient.available();\n}\n\nint lilygoLteTcpRead(uint8_t* buffer, size_t len)\n{\n    if (!tcpOpenFlag || !buffer || len == 0) return 0;\n\n    int n = lteClient.read(buffer, len);\n\n    if (n > 0) {\n        bytesRead += n;\n        traceAppend("TCP read result=" + String(n));\n    } else if (n < 0) {\n        tcpOpenFlag = lteClient.connected();\n        n = 0;\n    }\n\n    return n;\n}\n\nbool lilygoLteTcpConnected()\n{\n    tcpOpenFlag = lteClient.connected();\n    return tcpOpenFlag;\n}\n\nvoid lilygoLteTcpClose()\n{\n    lteClient.stop();\n    tcpOpenFlag = false;\n    traceAppend("TCP close");\n}\n\nString lilygoLteTcpTestJson(const String& host, uint16_t port)\n{\n    uint32_t start = millis();\n    bool ok = lilygoLteTcpOpen(host, port);\n    uint32_t elapsed = millis() - start;\n    lilygoLteTcpClose();\n\n    String json = "{";\n    json += "\\"backend\\":\\"LewisXhe TinyGSM A76XXSSL\\",";\n    json += "\\"host\\":\\"" + esc(host) + "\\",";\n    json += "\\"port\\":" + String(port) + ",";\n    json += "\\"tcpOpen\\":" + String(ok ? "true" : "false") + ",";\n    json += "\\"elapsedMs\\":" + String(elapsed) + ",";\n    json += "\\"modemReady\\":" + String(modemReadyFlag ? "true" : "false") + ",";\n    json += "\\"networkReady\\":" + String(networkReadyFlag ? "true" : "false") + ",";\n    json += "\\"gprsConnected\\":" + String(lilygoGprsConnected() ? "true" : "false") + ",";\n    json += "\\"lteIp\\":\\"" + esc(lilygoLteIp()) + "\\",";\n    json += "\\"message\\":\\"" + esc(lastMessage) + "\\"";\n    json += "}";\n    return json;\n}\n\nString lilygoLteRxDebugJson()\n{\n    String json = "{";\n    json += "\\"backend\\":\\"LewisXhe TinyGSM A76XXSSL\\",";\n    json += "\\"tcpOpenFlag\\":" + String(tcpOpenFlag ? "true" : "false") + ",";\n    json += "\\"available\\":" + String(lteClient.available()) + ",";\n    json += "\\"connected\\":" + String(lteClient.connected() ? "true" : "false") + ",";\n    json += "\\"bytesWritten\\":" + String(bytesWritten) + ",";\n    json += "\\"bytesRead\\":" + String(bytesRead) + ",";\n    json += "\\"message\\":\\"" + esc(lastMessage) + "\\",";\n    json += "\\"trace\\":\\"" + esc(lastTrace) + "\\"";\n    json += "}";\n    return json;\n}\n\nString lilygoTinyGsmTraceJson()\n{\n    return lilygoLteRxDebugJson();\n}\n\nvoid lilygoTinyGsmTraceClear()\n{\n    lastTrace = "";\n    bytesWritten = 0;\n    bytesRead = 0;\n}\n\nString lilygoLteDebugJson()\n{\n    return lilygoModemStatusJson();\n}\n\nString lilygoModemStatusJson()\n{\n    int signal = modemReadyFlag ? modem.getSignalQuality() : 99;\n\n    String json = "{";\n    json += "\\"backend\\":\\"LewisXhe TinyGSM A76XXSSL\\",";\n    json += "\\"modemReady\\":" + String(modemReadyFlag ? "true" : "false") + ",";\n    json += "\\"networkReady\\":" + String(networkReadyFlag ? "true" : "false") + ",";\n    json += "\\"gprsAttached\\":" + String(lilygoGprsConnected() ? "true" : "false") + ",";\n    json += "\\"pdpConfigured\\":" + String(gprsReadyFlag ? "true" : "false") + ",";\n    json += "\\"lteIp\\":\\"" + esc(lilygoLteIp()) + "\\",";\n    json += "\\"signalQuality\\":" + String(signal) + ",";\n    json += "\\"tcpOpen\\":" + String(tcpOpenFlag ? "true" : "false") + ",";\n    json += "\\"tcpOpenCount\\":" + String(tcpOpenCount) + ",";\n    json += "\\"tcpFailCount\\":" + String(tcpFailCount) + ",";\n    json += "\\"bytesWritten\\":" + String(bytesWritten) + ",";\n    json += "\\"bytesRead\\":" + String(bytesRead) + ",";\n    json += "\\"message\\":\\"" + esc(lastMessage) + "\\"";\n\n    if (modemReadyFlag) {\n        json += ",\\"modemName\\":\\"" + esc(modem.getModemName()) + "\\"";\n        json += ",\\"modemInfo\\":\\"" + esc(modem.getModemInfo()) + "\\"";\n    }\n\n    json += "}";\n    return json;\n}\n')
write(fw / "src/lte/lilygo_lte_client.h", '#pragma once\n\n#include <Arduino.h>\n#include <Client.h>\n\nclass LilygoLteClient : public Client\n{\npublic:\n    int connect(IPAddress ip, uint16_t port) override;\n    int connect(const char *host, uint16_t port) override;\n\n    size_t write(uint8_t b) override;\n    size_t write(const uint8_t *buf, size_t size) override;\n\n    int available() override;\n    int read() override;\n    int read(uint8_t *buf, size_t size) override;\n    int peek() override;\n    void flush() override;\n    void stop() override;\n    uint8_t connected() override;\n    operator bool() override;\n\nprivate:\n    bool connectedFlag = false;\n};\n\nString lilygoLteClientTraceJson();\nvoid lilygoLteClientTraceClear();\n')
write(fw / "src/lte/lilygo_lte_client.cpp", '#include "lilygo_lte_client.h"\n\n#include "modem/lilygo_modem.h"\n\nstatic uint32_t connectCalls = 0;\nstatic uint32_t writeCalls = 0;\nstatic uint32_t readCalls = 0;\nstatic uint32_t availableCalls = 0;\nstatic uint32_t stopCalls = 0;\nstatic int lastAvailable = 0;\nstatic int lastRead = 0;\nstatic int lastWritten = 0;\nstatic String trace;\n\nstatic void traceAppend(const String& line)\n{\n    trace += String(millis()) + "ms " + line + "\\n";\n\n    if (trace.length() > 5000) {\n        trace.remove(0, trace.length() - 5000);\n    }\n}\n\nstatic String esc(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteClientTraceJson()\n{\n    String json = "{";\n    json += "\\"backend\\":\\"LewisXhe TinyGSM A76XXSSL wrapper\\",";\n    json += "\\"connectCalls\\":" + String(connectCalls) + ",";\n    json += "\\"writeCalls\\":" + String(writeCalls) + ",";\n    json += "\\"readCalls\\":" + String(readCalls) + ",";\n    json += "\\"availableCalls\\":" + String(availableCalls) + ",";\n    json += "\\"stopCalls\\":" + String(stopCalls) + ",";\n    json += "\\"lastAvailable\\":" + String(lastAvailable) + ",";\n    json += "\\"lastRead\\":" + String(lastRead) + ",";\n    json += "\\"lastWritten\\":" + String(lastWritten) + ",";\n    json += "\\"connected\\":" + String(lilygoLteTcpConnected() ? "true" : "false") + ",";\n    json += "\\"trace\\":\\"" + esc(trace) + "\\"";\n    json += "}";\n    return json;\n}\n\nvoid lilygoLteClientTraceClear()\n{\n    trace = "";\n    connectCalls = 0;\n    writeCalls = 0;\n    readCalls = 0;\n    availableCalls = 0;\n    stopCalls = 0;\n    lastAvailable = 0;\n    lastRead = 0;\n    lastWritten = 0;\n}\n\nint LilygoLteClient::connect(const char *host, uint16_t port)\n{\n    connectCalls++;\n    traceAppend("connect host=" + String(host) + " port=" + String(port));\n\n    connectedFlag = lilygoLteTcpOpen(String(host), port);\n\n    traceAppend(String("connect result=") + (connectedFlag ? "1" : "0"));\n    return connectedFlag ? 1 : 0;\n}\n\nint LilygoLteClient::connect(IPAddress ip, uint16_t port)\n{\n    char host[24];\n    snprintf(host, sizeof(host), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);\n    return connect(host, port);\n}\n\nsize_t LilygoLteClient::write(uint8_t b)\n{\n    return write(&b, 1);\n}\n\nsize_t LilygoLteClient::write(const uint8_t *buf, size_t size)\n{\n    writeCalls++;\n\n    if (!connectedFlag) {\n        lastWritten = 0;\n        return 0;\n    }\n\n    int n = lilygoLteTcpWrite(buf, size);\n    lastWritten = n;\n\n    if (n <= 0) {\n        connectedFlag = lilygoLteTcpConnected();\n    }\n\n    traceAppend("write size=" + String(size) + " result=" + String(n));\n    return n > 0 ? (size_t)n : 0;\n}\n\nint LilygoLteClient::available()\n{\n    availableCalls++;\n\n    if (!connectedFlag) {\n        lastAvailable = 0;\n        return 0;\n    }\n\n    lastAvailable = lilygoLteTcpAvailable();\n    return lastAvailable;\n}\n\nint LilygoLteClient::read()\n{\n    uint8_t b = 0;\n    int n = read(&b, 1);\n    return n == 1 ? b : -1;\n}\n\nint LilygoLteClient::read(uint8_t *buf, size_t size)\n{\n    readCalls++;\n\n    if (!connectedFlag) {\n        lastRead = 0;\n        return 0;\n    }\n\n    int n = lilygoLteTcpRead(buf, size);\n    lastRead = n;\n\n    if (n < 0) {\n        connectedFlag = lilygoLteTcpConnected();\n        return 0;\n    }\n\n    traceAppend("read size=" + String(size) + " result=" + String(n));\n    return n;\n}\n\nint LilygoLteClient::peek()\n{\n    return -1;\n}\n\nvoid LilygoLteClient::flush()\n{\n}\n\nvoid LilygoLteClient::stop()\n{\n    stopCalls++;\n    lilygoLteTcpClose();\n    connectedFlag = false;\n    traceAppend("stop");\n}\n\nuint8_t LilygoLteClient::connected()\n{\n    connectedFlag = lilygoLteTcpConnected();\n    return connectedFlag ? 1 : 0;\n}\n\nLilygoLteClient::operator bool()\n{\n    return connected();\n}\n')

# Ensure Web trace route exists.
web = fw / "src/web/lilygo_web.cpp"
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

print("Applied full LewisXhe TinyGSM A76XX LTE transport for MOT LilyGO.")
