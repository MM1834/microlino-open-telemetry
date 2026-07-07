
from pathlib import Path

root = Path.cwd()

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
s = read(modem_cpp)
s = s.replace("'\\\\r'", "'\\r'")
s = s.replace("'\\\\n'", "'\\n'")
if "lilygoLteRxDebugJson" not in s:
    s += "\n" + '\nstatic String jsonEscRxDebug(String value)\n{\n    value.replace("\\\\", "\\\\\\\\");\n    value.replace("\\"", "\\\\\\"");\n    value.replace("\\r", "\\\\r");\n    value.replace("\\n", "\\\\n");\n    return value;\n}\n\nString lilygoLteRxDebugJson()\n{\n    String query = atCommand("AT+CIPRXGET=4,0", 3000);\n\n    uint8_t buf[64];\n    int n = lilygoLteTcpRead(buf, sizeof(buf));\n\n    String hex;\n    String ascii;\n\n    for (int i = 0; i < n; i++) {\n        if (buf[i] < 16) hex += "0";\n        hex += String(buf[i], HEX);\n        if (i + 1 < n) hex += " ";\n\n        char c = (char)buf[i];\n        ascii += (c >= 32 && c <= 126) ? c : \'.\';\n    }\n\n    String after = atCommand("AT+CIPRXGET=4,0", 3000);\n\n    String json = "{";\n    json += "\\"tcpOpenFlag\\":" + String(lteTcpOpenFlag ? "true" : "false") + ",";\n    json += "\\"availableQuery\\":\\"" + jsonEscRxDebug(query) + "\\",";\n    json += "\\"read\\":" + String(n) + ",";\n    json += "\\"hex\\":\\"" + hex + "\\",";\n    json += "\\"ascii\\":\\"" + jsonEscRxDebug(ascii) + "\\",";\n    json += "\\"availableAfter\\":\\"" + jsonEscRxDebug(after) + "\\",";\n    json += "\\"lastAt\\":\\"" + jsonEscRxDebug(lastAt) + "\\"";\n    json += "}";\n    return json;\n}\n' + "\n"
write(modem_cpp, s)

modem_h = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.h"
s = read(modem_h)
if "lilygoLteRxDebugJson" not in s:
    s += "\nString lilygoLteRxDebugJson();\n"
write(modem_h, s)

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
s = read(web)

if "handleLteRxDebug" not in s:
    handler = """
static void handleLteRxDebug()
{
    server.send(200, "application/json", lilygoLteRxDebugJson());
}

"""
    if "static void handleLteDebug()" in s:
        s = s.replace("static void handleLteDebug()", handler + "static void handleLteDebug()", 1)
    elif "static void handleMqtt()" in s:
        s = s.replace("static void handleMqtt()", handler + "static void handleMqtt()", 1)
    else:
        s += "\n" + handler

if 'server.on("/api/lilygo/lte/rx-debug"' not in s:
    route = 'server.on("/api/lilygo/lte/rx-debug", HTTP_GET, handleLteRxDebug);'
    if 'server.on("/api/lilygo/lte/debug"' in s:
        idx = s.find('server.on("/api/lilygo/lte/debug"')
        line_end = s.find("\n", idx)
        s = s[:line_end+1] + "    " + route + "\n" + s[line_end+1:]
    elif "server.begin();" in s:
        s = s.replace("server.begin();", route + "\n    server.begin();", 1)

if "LTE RX Debug" not in s:
    s = s.replace(
        "<a href='/api/lilygo/lte/debug'>LTE Debug</a>",
        "<a href='/api/lilygo/lte/debug'>LTE Debug</a> · <a href='/api/lilygo/lte/rx-debug'>LTE RX Debug</a>"
    )

write(web, s)

print("Applied LilyGO LTE RX debug endpoint and CR/LF warning fix.")
