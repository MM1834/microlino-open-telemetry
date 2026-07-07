
from pathlib import Path
import re

root = Path.cwd()

def replace_func(s, ret, name, body):
    m = re.search(ret + r'\s+' + re.escape(name) + r'\s*\([^)]*\)\s*\{', s)
    if not m:
        raise SystemExit("Could not find " + name)
    start = m.start()
    brace = s.find("{", m.end() - 1)
    depth = 0
    for i in range(brace, len(s)):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return s[:start] + body.strip() + s[i+1:]
    raise SystemExit("Could not parse " + name)

rx_helpers = '''
static uint8_t lteRxBuffer[768];
static size_t lteRxBufferLen = 0;
static String lteUrcText;

static void lteRxPushByte(uint8_t b)
{
    if (lteRxBufferLen < sizeof(lteRxBuffer)) {
        lteRxBuffer[lteRxBufferLen++] = b;
        return;
    }
    memmove(lteRxBuffer, lteRxBuffer + 1, sizeof(lteRxBuffer) - 1);
    lteRxBuffer[sizeof(lteRxBuffer) - 1] = b;
}

static bool parseReceiveHeader(const String& text, int& headerStart, int& payloadStart, int& payloadLen)
{
    int marker = text.indexOf("+RECEIVE,");
    int markerLen = String("+RECEIVE,").length();

    if (marker < 0) {
        marker = text.indexOf("+IPD,");
        markerLen = String("+IPD,").length();
    }

    if (marker < 0) return false;

    int comma1 = text.indexOf(',', marker + markerLen);
    int colon = text.indexOf(':', marker + markerLen);
    if (colon < 0) return false;

    if (comma1 > 0 && comma1 < colon) {
        payloadLen = text.substring(comma1 + 1, colon).toInt();
    } else {
        payloadLen = text.substring(marker + markerLen, colon).toInt();
    }

    if (payloadLen <= 0) return false;

    headerStart = marker;
    payloadStart = colon + 1;

    if (payloadStart + 1 < text.length() && text[payloadStart] == '\\r' && text[payloadStart + 1] == '\\n') {
        payloadStart += 2;
    } else if (payloadStart < text.length() && (text[payloadStart] == '\\r' || text[payloadStart] == '\\n')) {
        payloadStart += 1;
    }

    return true;
}

static void ltePollIncoming(uint32_t budgetMs = 20)
{
    uint32_t start = millis();

    while (millis() - start < budgetMs) {
        bool hadData = false;

        while (SerialAT.available()) {
            hadData = true;
            char c = (char)SerialAT.read();
            lteUrcText += c;

            if (lteUrcText.length() > 1200) {
                lteUrcText.remove(0, lteUrcText.length() - 1200);
            }

            int headerStart = -1;
            int payloadStart = -1;
            int payloadLen = 0;

            while (parseReceiveHeader(lteUrcText, headerStart, payloadStart, payloadLen)) {
                if (lteUrcText.length() < payloadStart + payloadLen) break;

                for (int i = 0; i < payloadLen; i++) {
                    lteRxPushByte((uint8_t)lteUrcText[payloadStart + i]);
                }

                lteUrcText.remove(0, payloadStart + payloadLen);
            }

            if (lteUrcText.indexOf("CLOSED") >= 0 || lteUrcText.indexOf("+IPCLOSE") >= 0) {
                lteTcpOpenFlag = false;
            }
        }

        if (!hadData) break;
        delay(1);
    }
}
'''

available_func = '''
int lilygoLteTcpAvailable()
{
    if (!lteTcpOpenFlag && lteRxBufferLen == 0) return 0;
    ltePollIncoming(25);
    return (int)lteRxBufferLen;
}
'''

read_func = '''
int lilygoLteTcpRead(uint8_t* buffer, size_t len)
{
    if (!buffer || len == 0) return 0;

    ltePollIncoming(25);

    if (lteRxBufferLen == 0) return 0;

    size_t n = len;
    if (n > lteRxBufferLen) n = lteRxBufferLen;

    memcpy(buffer, lteRxBuffer, n);

    if (n < lteRxBufferLen) {
        memmove(lteRxBuffer, lteRxBuffer + n, lteRxBufferLen - n);
    }

    lteRxBufferLen -= n;
    return (int)n;
}
'''

rx_debug = '''
static String jsonEscUrcRxDebug(String value)
{
    value.replace("\\\\", "\\\\\\\\");
    value.replace("\\"", "\\\\\\"");
    value.replace("\\r", "\\\\r");
    value.replace("\\n", "\\\\n");
    return value;
}

String lilygoLteRxDebugJson()
{
    ltePollIncoming(100);

    uint8_t buf[64];
    size_t n = lteRxBufferLen;
    if (n > sizeof(buf)) n = sizeof(buf);
    memcpy(buf, lteRxBuffer, n);

    String hex;
    String ascii;

    for (size_t i = 0; i < n; i++) {
        if (buf[i] < 16) hex += "0";
        hex += String(buf[i], HEX);
        if (i + 1 < n) hex += " ";

        char c = (char)buf[i];
        ascii += (c >= 32 && c <= 126) ? c : '.';
    }

    String json = "{";
    json += "\\"tcpOpenFlag\\":" + String(lteTcpOpenFlag ? "true" : "false") + ",";
    json += "\\"rxBufferLen\\":" + String(lteRxBufferLen) + ",";
    json += "\\"peekLen\\":" + String(n) + ",";
    json += "\\"hex\\":\\"" + hex + "\\",";
    json += "\\"ascii\\":\\"" + jsonEscUrcRxDebug(ascii) + "\\",";
    json += "\\"urcText\\":\\"" + jsonEscUrcRxDebug(lteUrcText) + "\\",";
    json += "\\"lastAt\\":\\"" + jsonEscUrcRxDebug(lastAt) + "\\"";
    json += "}";
    return json;
}
'''

modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
s = modem_cpp.read_text(encoding="utf-8")

if "static uint8_t lteRxBuffer" not in s:
    marker = "int lilygoLteTcpWrite"
    if marker not in s:
        raise SystemExit("Could not find lilygoLteTcpWrite marker")
    s = s.replace(marker, rx_helpers + "\n\n" + marker, 1)

s = replace_func(s, "int", "lilygoLteTcpAvailable", available_func)
s = replace_func(s, "int", "lilygoLteTcpRead", read_func)

if "String lilygoLteRxDebugJson()" in s:
    s = replace_func(s, "String", "lilygoLteRxDebugJson", rx_debug)
else:
    s += "\n" + rx_debug + "\n"

modem_cpp.write_text(s, encoding="utf-8")

modem_h = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.h"
if modem_h.exists():
    h = modem_h.read_text(encoding="utf-8")
    if "lilygoLteRxDebugJson" not in h:
        h += "\nString lilygoLteRxDebugJson();\n"
    modem_h.write_text(h, encoding="utf-8")

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    w = web.read_text(encoding="utf-8")
    if "handleLteRxDebug" not in w:
        handler = '''
static void handleLteRxDebug()
{
    server.send(200, "application/json", lilygoLteRxDebugJson());
}

'''
        if "static void handleLteDebug()" in w:
            w = w.replace("static void handleLteDebug()", handler + "static void handleLteDebug()", 1)
        elif "static void handleMqtt()" in w:
            w = w.replace("static void handleMqtt()", handler + "static void handleMqtt()", 1)
        else:
            w += "\n" + handler

    if 'server.on("/api/lilygo/lte/rx-debug"' not in w:
        route = 'server.on("/api/lilygo/lte/rx-debug", HTTP_GET, handleLteRxDebug);'
        if 'server.on("/api/lilygo/lte/debug"' in w:
            idx = w.find('server.on("/api/lilygo/lte/debug"')
            line_end = w.find("\n", idx)
            w = w[:line_end+1] + "    " + route + "\n" + w[line_end+1:]
        elif "server.begin();" in w:
            w = w.replace("server.begin();", route + "\n    server.begin();", 1)

    if "LTE RX Debug" not in w and "<a href='/api/lilygo/lte/debug'>LTE Debug</a>" in w:
        w = w.replace(
            "<a href='/api/lilygo/lte/debug'>LTE Debug</a>",
            "<a href='/api/lilygo/lte/debug'>LTE Debug</a> · <a href='/api/lilygo/lte/rx-debug'>LTE RX Debug</a>"
        )
    web.write_text(w, encoding="utf-8")

print("Applied URC RX buffer receive path for LilyGO LTE.")
