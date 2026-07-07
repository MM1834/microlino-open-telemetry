from pathlib import Path
root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

h = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.h"
if h.exists():
    s = read(h)
    if "lilygoLteClientTraceJson" not in s:
        s += "\nString lilygoLteClientTraceJson();\nvoid lilygoLteClientTraceClear();\n"
    write(h, s)

cpp = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.cpp"
if cpp.exists():
    s = read(cpp)

    if "static String lteClientTrace" not in s:
        s = s.replace(
            '#include "modem/lilygo_modem.h"',
            '#include "modem/lilygo_modem.h"\n\n' + 'static String lteClientTrace;\nstatic uint32_t lteClientConnectCalls = 0;\nstatic uint32_t lteClientWriteCalls = 0;\nstatic uint32_t lteClientReadCalls = 0;\nstatic uint32_t lteClientAvailableCalls = 0;\nstatic uint32_t lteClientStopCalls = 0;\nstatic int lteClientLastAvailable = 0;\nstatic int lteClientLastRead = 0;\nstatic int lteClientLastWritten = 0;\nstatic bool lteClientLastConnected = false;\n\nstatic void traceAppend(const String& line)\n{\n    lteClientTrace += String(millis()) + "ms " + line + "\\n";\n    if (lteClientTrace.length() > 3500) {\n        lteClientTrace.remove(0, lteClientTrace.length() - 3500);\n    }\n}\n\nstatic String traceEsc(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteClientTraceJson()\n{\n    String json = "{";\n    json += "\\"connectCalls\\":" + String(lteClientConnectCalls) + ",";\n    json += "\\"writeCalls\\":" + String(lteClientWriteCalls) + ",";\n    json += "\\"readCalls\\":" + String(lteClientReadCalls) + ",";\n    json += "\\"availableCalls\\":" + String(lteClientAvailableCalls) + ",";\n    json += "\\"stopCalls\\":" + String(lteClientStopCalls) + ",";\n    json += "\\"lastAvailable\\":" + String(lteClientLastAvailable) + ",";\n    json += "\\"lastRead\\":" + String(lteClientLastRead) + ",";\n    json += "\\"lastWritten\\":" + String(lteClientLastWritten) + ",";\n    json += "\\"lastConnected\\":" + String(lteClientLastConnected ? "true" : "false") + ",";\n    json += "\\"modemTcpConnected\\":" + String(lilygoLteTcpConnected() ? "true" : "false") + ",";\n    json += "\\"trace\\":\\"" + traceEsc(lteClientTrace) + "\\"";\n    json += "}";\n    return json;\n}\n\nvoid lilygoLteClientTraceClear()\n{\n    lteClientTrace = "";\n    lteClientConnectCalls = 0;\n    lteClientWriteCalls = 0;\n    lteClientReadCalls = 0;\n    lteClientAvailableCalls = 0;\n    lteClientStopCalls = 0;\n    lteClientLastAvailable = 0;\n    lteClientLastRead = 0;\n    lteClientLastWritten = 0;\n    lteClientLastConnected = false;\n}\n'
        )

    if 'traceAppend("connect host="' not in s:
        s = s.replace(
            'connectedFlag = lilygoLteTcpOpen(String(host), port);\n    return connectedFlag ? 1 : 0;',
            'lteClientConnectCalls++;\n'
            '    traceAppend("connect host=" + String(host) + " port=" + String(port));\n'
            '    connectedFlag = lilygoLteTcpOpen(String(host), port);\n'
            '    lteClientLastConnected = connectedFlag;\n'
            '    traceAppend(String("connect result=") + (connectedFlag ? "1" : "0"));\n'
            '    return connectedFlag ? 1 : 0;'
        )

    if 'traceAppend("write skipped' not in s:
        s = s.replace(
            'if (!connectedFlag) return 0;\n    int written = lilygoLteTcpWrite(buf, size);',
            'lteClientWriteCalls++;\n'
            '    if (!connectedFlag) {\n'
            '        lteClientLastWritten = 0;\n'
            '        traceAppend("write skipped not-connected size=" + String(size));\n'
            '        return 0;\n'
            '    }\n'
            '    traceAppend("write size=" + String(size));\n'
            '    int written = lilygoLteTcpWrite(buf, size);'
        )
        s = s.replace(
            'if (written <= 0) {\n        connectedFlag = lilygoLteTcpConnected();\n        return 0;\n    }\n    return (size_t)written;',
            'lteClientLastWritten = written;\n'
            '    traceAppend("write result=" + String(written));\n'
            '    if (written <= 0) {\n'
            '        connectedFlag = lilygoLteTcpConnected();\n'
            '        lteClientLastConnected = connectedFlag;\n'
            '        traceAppend(String("write reconnect-state=") + (connectedFlag ? "1" : "0"));\n'
            '        return 0;\n'
            '    }\n'
            '    return (size_t)written;'
        )

    if 'lteClientAvailableCalls++' not in s:
        s = s.replace(
            'return connectedFlag ? lilygoLteTcpAvailable() : 0;',
            'lteClientAvailableCalls++;\n'
            '    int available = connectedFlag ? lilygoLteTcpAvailable() : 0;\n'
            '    lteClientLastAvailable = available;\n'
            '    traceAppend("available result=" + String(available));\n'
            '    return available;'
        )

    if 'lteClientReadCalls++' not in s:
        s = s.replace(
            'if (!connectedFlag) return 0;\n    int n = lilygoLteTcpRead(buf, size);',
            'lteClientReadCalls++;\n'
            '    if (!connectedFlag) {\n'
            '        lteClientLastRead = 0;\n'
            '        traceAppend("read skipped not-connected size=" + String(size));\n'
            '        return 0;\n'
            '    }\n'
            '    traceAppend("read size=" + String(size));\n'
            '    int n = lilygoLteTcpRead(buf, size);'
        )
        s = s.replace(
            'if (n < 0) {\n        connectedFlag = lilygoLteTcpConnected();\n        return 0;\n    }\n    return n;',
            'lteClientLastRead = n;\n'
            '    traceAppend("read result=" + String(n));\n'
            '    if (n < 0) {\n'
            '        connectedFlag = lilygoLteTcpConnected();\n'
            '        lteClientLastConnected = connectedFlag;\n'
            '        traceAppend(String("read reconnect-state=") + (connectedFlag ? "1" : "0"));\n'
            '        return 0;\n'
            '    }\n'
            '    return n;'
        )

    if 'lteClientStopCalls++' not in s:
        s = s.replace(
            'void LilygoLteClient::stop() { lilygoLteTcpClose(); connectedFlag = false; }',
            'void LilygoLteClient::stop() { lteClientStopCalls++; traceAppend("stop"); lilygoLteTcpClose(); connectedFlag = false; lteClientLastConnected = false; }'
        )

    if 'traceAppend(String("connected result=' not in s:
        s = s.replace(
            'uint8_t LilygoLteClient::connected() { connectedFlag = lilygoLteTcpConnected(); return connectedFlag ? 1 : 0; }',
            'uint8_t LilygoLteClient::connected() { connectedFlag = lilygoLteTcpConnected(); lteClientLastConnected = connectedFlag; traceAppend(String("connected result=") + (connectedFlag ? "1" : "0")); return connectedFlag ? 1 : 0; }'
        )

    write(cpp, s)

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    s = read(web)

    if '#include "lte/lilygo_lte_client.h"' not in s:
        lines = s.splitlines()
        insert_at = 0
        for i, line in enumerate(lines):
            if line.startswith("#include"):
                insert_at = i + 1
        lines.insert(insert_at, '#include "lte/lilygo_lte_client.h"')
        s = "\n".join(lines) + "\n"

    if "handleLteMqttTrace" not in s:
        handler = '\nstatic void handleLteMqttTrace()\n{\n    server.send(200, "application/json", lilygoLteClientTraceJson());\n}\n\nstatic void handleLteMqttTraceClear()\n{\n    lilygoLteClientTraceClear();\n    server.send(200, "application/json", lilygoLteClientTraceJson());\n}\n\n'
        if "static void handleMqtt()" in s:
            s = s.replace("static void handleMqtt()", handler + "static void handleMqtt()", 1)
        else:
            s += "\n" + handler

    if 'server.on("/api/lilygo/lte/mqtt-trace"' not in s:
        routes = (
            'server.on("/api/lilygo/lte/mqtt-trace", HTTP_GET, handleLteMqttTrace);\n'
            '    server.on("/api/lilygo/lte/mqtt-trace/clear", HTTP_POST, handleLteMqttTraceClear);'
        )
        if 'server.on("/api/lilygo/mqtt"' in s:
            idx = s.find('server.on("/api/lilygo/mqtt"')
            line_end = s.find("\n", idx)
            s = s[:line_end+1] + "    " + routes + "\n" + s[line_end+1:]
        elif "server.begin();" in s:
            s = s.replace("server.begin();", routes + "\n    server.begin();", 1)

    if "LTE MQTT Trace" not in s:
        s = s.replace(
            "<p><a href='/api/lilygo/mqtt/debug'>MQTT Debug</a></p>",
            "<p><a href='/api/lilygo/mqtt/debug'>MQTT Debug</a> · <a href='/api/lilygo/lte/mqtt-trace'>LTE MQTT Trace</a></p>"
        )

    write(web, s)

print("MOT LilyGO LTE MQTT trace applied.")
