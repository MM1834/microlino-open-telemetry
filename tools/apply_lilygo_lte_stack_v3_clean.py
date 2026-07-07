from pathlib import Path
import re

root = Path.cwd()

def read(p):
    return p.read_text(encoding='utf-8')

def write(p, s):
    p.write_text(s, encoding='utf-8')

def find_func_span(s, ret, name):
    pattern = ret + r'\s+' + re.escape(name) + r'\s*\([^)]*\)\s*\{'
    m = re.search(pattern, s)
    if not m:
        return None
    start = m.start()
    brace = s.find('{', m.end() - 1)
    depth = 0
    for i in range(brace, len(s)):
        if s[i] == '{':
            depth += 1
        elif s[i] == '}':
            depth -= 1
            if depth == 0:
                return start, i + 1
    return None

def replace_func(s, ret, name, body):
    span = find_func_span(s, ret, name)
    if not span:
        raise SystemExit(f'Could not find function {name}')
    start, end = span
    return s[:start] + body.strip() + '\n' + s[end:]

def remove_all_func(s, ret, name):
    while True:
        span = find_func_span(s, ret, name)
        if not span:
            return s
        start, end = span
        s = s[:start] + '\n' + s[end:]

modem_cpp = root / 'firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp'
modem_h = root / 'firmware/lilygo-t-a7670/src/modem/lilygo_modem.h'
lte_cpp = root / 'firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.cpp'
web_cpp = root / 'firmware/lilygo-t-a7670/src/web/lilygo_web.cpp'

s = read(modem_cpp)
if '#include <string.h>' not in s and '#include <cstring>' not in s:
    lines = s.splitlines()
    insert_at = 0
    for i, line in enumerate(lines):
        if line.startswith('#include'):
            insert_at = i + 1
    lines.insert(insert_at, '#include <string.h>')
    s = '\n'.join(lines) + '\n'

while 'static uint8_t lteRxBuffer' in s:
    start = s.find('static uint8_t lteRxBuffer')
    candidates = [s.find(x, start + 1) for x in ['bool lilygoLteTcpOpen','int lilygoLteTcpWrite','int lilygoLteTcpAvailable','String lilygoLteRxDebugJson']]
    candidates = [c for c in candidates if c != -1]
    end = min(candidates) if candidates else start + 6000
    s = s[:start] + '\n' + s[end:]

for fname in ['jsonEscUrcRxDebug', 'jsonEscRxDebug', 'jsonEscLteStackV3']:
    s = remove_all_func(s, 'static String', fname)
s = remove_all_func(s, 'String', 'lilygoLteRxDebugJson')

stack_helpers = r'''
// -----------------------------------------------------------------------------
// LilyGO A7670 LTE AT socket stack v3
// Does not use AT+CIPSTATUS or AT+CIPRXGET on this A7670G firmware.
// Incoming socket data is collected from +RECEIVE / +IPD UART URCs.
// -----------------------------------------------------------------------------

static uint8_t lteRxBuffer[768];
static size_t lteRxBufferLen = 0;
static String lteUrcText;

static void lteRxReset()
{
    lteRxBufferLen = 0;
    lteUrcText = "";
}

static void lteRxPushByte(uint8_t b)
{
    if (lteRxBufferLen < sizeof(lteRxBuffer)) {
        lteRxBuffer[lteRxBufferLen++] = b;
        return;
    }
    memmove(lteRxBuffer, lteRxBuffer + 1, sizeof(lteRxBuffer) - 1);
    lteRxBuffer[sizeof(lteRxBuffer) - 1] = b;
}

static bool parseReceiveHeader(const String& text, int& payloadStart, int& payloadLen)
{
    int marker = text.indexOf("+RECEIVE,");
    int markerLen = String("+RECEIVE,").length();
    if (marker < 0) {
        marker = text.indexOf("+IPD,");
        markerLen = String("+IPD,").length();
    }
    if (marker < 0) return false;
    int colon = text.indexOf(':', marker + markerLen);
    if (colon < 0) return false;
    int comma1 = text.indexOf(',', marker + markerLen);
    if (comma1 > 0 && comma1 < colon) payloadLen = text.substring(comma1 + 1, colon).toInt();
    else payloadLen = text.substring(marker + markerLen, colon).toInt();
    if (payloadLen <= 0) return false;
    payloadStart = colon + 1;
    if (payloadStart + 1 < text.length() && text[payloadStart] == '\r' && text[payloadStart + 1] == '\n') payloadStart += 2;
    else if (payloadStart < text.length() && (text[payloadStart] == '\r' || text[payloadStart] == '\n')) payloadStart += 1;
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
            if (lteUrcText.length() > 1400) lteUrcText.remove(0, lteUrcText.length() - 1400);
            int payloadStart = -1;
            int payloadLen = 0;
            while (parseReceiveHeader(lteUrcText, payloadStart, payloadLen)) {
                if (lteUrcText.length() < payloadStart + payloadLen) break;
                for (int i = 0; i < payloadLen; i++) lteRxPushByte((uint8_t)lteUrcText[payloadStart + i]);
                lteUrcText.remove(0, payloadStart + payloadLen);
            }
            if (lteUrcText.indexOf("CLOSED") >= 0 || lteUrcText.indexOf("+IPCLOSE") >= 0) lteTcpOpenFlag = false;
        }
        if (!hadData) break;
        delay(1);
    }
}

static String jsonEscLteStackV3(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}
'''

open_func = r'''
bool lilygoLteTcpOpen(const String& host, uint16_t port)
{
    if (!modemReadyFlag) { lastMessage = "LTE TCP open failed: modem not ready"; return false; }
    if (!lilygoEnsureGprsConnected()) { lastMessage = "LTE TCP open failed: GPRS not connected"; return false; }
    lteTcpOpenFlag = false;
    lteRxReset();
    atCommand("AT+CIPCLOSE=0", 2000);
    String netState = atCommand("AT+NETOPEN?", 3000);
    if (netState.indexOf("+NETOPEN: 1") < 0) {
        atCommand("AT+NETOPEN", 10000);
        netState = atCommand("AT+NETOPEN?", 3000);
    }
    while (SerialAT.available()) SerialAT.read();
    String cmd = "AT+CIPOPEN=0,\"TCP\",\"" + host + "\"," + String(port);
    SerialAT.print(cmd);
    SerialAT.print("\r\n");
    String response;
    bool sawOk = false;
    uint32_t start = millis();
    while (millis() - start < 12000) {
        while (SerialAT.available()) response += (char)SerialAT.read();
        if (response.indexOf("+CIPOPEN: 0,0") >= 0) {
            lteTcpOpenFlag = true; lastAt = response; lastMessage = "LTE TCP open confirmed " + host + ":" + String(port); return true;
        }
        int pos = response.indexOf("+CIPOPEN: 0,");
        if (pos >= 0 && response.indexOf("+CIPOPEN: 0,0") < 0) {
            lteTcpOpenFlag = false; lastAt = response; lastMessage = "LTE TCP open failed: " + response; return false;
        }
        if (response.indexOf("\r\nOK") >= 0 || response.endsWith("OK")) sawOk = true;
        if (sawOk && millis() - start > 1200) {
            lteTcpOpenFlag = true; lastAt = response; lastMessage = "LTE TCP open assumed after OK " + host + ":" + String(port); return true;
        }
        delay(10);
    }
    lteTcpOpenFlag = false; lastAt = response; lastMessage = "LTE TCP open timeout: " + response; return false;
}
'''

write_func = r'''
int lilygoLteTcpWrite(const uint8_t* data, size_t len)
{
    if (!lteTcpOpenFlag || !data || len == 0) return 0;
    ltePollIncoming(20);
    String cmd = "AT+CIPSEND=0," + String(len);
    SerialAT.print(cmd);
    SerialAT.print("\r\n");
    String prompt;
    bool gotPrompt = false;
    uint32_t promptStart = millis();
    while (millis() - promptStart < 8000) {
        while (SerialAT.available()) {
            char c = (char)SerialAT.read();
            prompt += c;
            if (c == '>') { gotPrompt = true; break; }
            lteUrcText += c;
            if (lteUrcText.length() > 1400) lteUrcText.remove(0, lteUrcText.length() - 1400);
        }
        if (gotPrompt) break;
        if (prompt.indexOf("ERROR") >= 0 || prompt.indexOf("FAIL") >= 0) { lteTcpOpenFlag = false; lastAt = prompt; lastMessage = "LTE CIPSEND prompt failed"; return 0; }
        delay(5);
    }
    if (!gotPrompt) { lastAt = prompt; lastMessage = "LTE CIPSEND prompt timeout"; return 0; }
    SerialAT.write(data, len);
    SerialAT.flush();
    String response;
    uint32_t sendStart = millis();
    while (millis() - sendStart < 12000) {
        while (SerialAT.available()) {
            char c = (char)SerialAT.read();
            response += c;
            lteUrcText += c;
            if (lteUrcText.length() > 1400) lteUrcText.remove(0, lteUrcText.length() - 1400);
        }
        int payloadStart = -1;
        int payloadLen = 0;
        while (parseReceiveHeader(lteUrcText, payloadStart, payloadLen)) {
            if (lteUrcText.length() < payloadStart + payloadLen) break;
            for (int i = 0; i < payloadLen; i++) lteRxPushByte((uint8_t)lteUrcText[payloadStart + i]);
            lteUrcText.remove(0, payloadStart + payloadLen);
        }
        if (response.indexOf("SEND OK") >= 0 || response.indexOf("DATA ACCEPT") >= 0) {
            lastAt = prompt + response; lastMessage = "LTE TCP write OK"; ltePollIncoming(100); return (int)len;
        }
        if (response.indexOf("SEND FAIL") >= 0 || response.indexOf("ERROR") >= 0) {
            lteTcpOpenFlag = false; lastAt = prompt + response; lastMessage = "LTE TCP write failed"; return 0;
        }
        delay(5);
    }
    lastAt = prompt + response; lastMessage = "LTE TCP write timeout"; return 0;
}
'''

available_func = r'''
int lilygoLteTcpAvailable()
{
    if (!lteTcpOpenFlag && lteRxBufferLen == 0) return 0;
    ltePollIncoming(25);
    return (int)lteRxBufferLen;
}
'''

read_func = r'''
int lilygoLteTcpRead(uint8_t* buffer, size_t len)
{
    if (!buffer || len == 0) return 0;
    ltePollIncoming(25);
    if (lteRxBufferLen == 0) return 0;
    size_t n = len;
    if (n > lteRxBufferLen) n = lteRxBufferLen;
    memcpy(buffer, lteRxBuffer, n);
    if (n < lteRxBufferLen) memmove(lteRxBuffer, lteRxBuffer + n, lteRxBufferLen - n);
    lteRxBufferLen -= n;
    return (int)n;
}
'''

connected_func = r'''
bool lilygoLteTcpConnected()
{
    ltePollIncoming(5);
    return lteTcpOpenFlag;
}
'''

close_func = r'''
void lilygoLteTcpClose()
{
    atCommand("AT+CIPCLOSE=0", 2000);
    lteTcpOpenFlag = false;
    lteRxReset();
}
'''

rx_debug_func = r'''
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
    json += "\"stack\":\"v3\",";
    json += "\"tcpOpenFlag\":" + String(lteTcpOpenFlag ? "true" : "false") + ",";
    json += "\"rxBufferLen\":" + String(lteRxBufferLen) + ",";
    json += "\"peekLen\":" + String(n) + ",";
    json += "\"hex\":\"" + hex + "\",";
    json += "\"ascii\":\"" + jsonEscLteStackV3(ascii) + "\",";
    json += "\"urcText\":\"" + jsonEscLteStackV3(lteUrcText) + "\",";
    json += "\"lastAt\":\"" + jsonEscLteStackV3(lastAt) + "\",";
    json += "\"message\":\"" + jsonEscLteStackV3(lastMessage) + "\"";
    json += "}";
    return json;
}
'''

open_pos = s.find('bool lilygoLteTcpOpen')
if open_pos < 0:
    raise SystemExit('Could not find lilygoLteTcpOpen')
s = s[:open_pos] + stack_helpers + '\n\n' + s[open_pos:]
s = replace_func(s, 'bool', 'lilygoLteTcpOpen', open_func)
s = replace_func(s, 'int', 'lilygoLteTcpWrite', write_func)
s = replace_func(s, 'int', 'lilygoLteTcpAvailable', available_func)
s = replace_func(s, 'int', 'lilygoLteTcpRead', read_func)
s = replace_func(s, 'bool', 'lilygoLteTcpConnected', connected_func)
s = replace_func(s, 'void', 'lilygoLteTcpClose', close_func)
s += '\n' + rx_debug_func + '\n'
write(modem_cpp, s)

if modem_h.exists():
    h = read(modem_h)
    if 'lilygoLteRxDebugJson' not in h:
        h += '\nString lilygoLteRxDebugJson();\n'
    write(modem_h, h)

if lte_cpp.exists():
    t = read(lte_cpp)
    connected = r'''
uint8_t LilygoLteClient::connected()
{
    connectedFlag = lilygoLteTcpConnected();
    return connectedFlag ? 1 : 0;
}
'''
    if find_func_span(t, 'uint8_t', 'LilygoLteClient::connected'):
        t = replace_func(t, 'uint8_t', 'LilygoLteClient::connected', connected)
    write(lte_cpp, t)

if web_cpp.exists():
    w = read(web_cpp)
    if 'handleLteRxDebug' not in w:
        handler = r'''
static void handleLteRxDebug()
{
    server.send(200, "application/json", lilygoLteRxDebugJson());
}

'''
        if 'static void handleLteDebug()' in w:
            w = w.replace('static void handleLteDebug()', handler + 'static void handleLteDebug()', 1)
        elif 'static void handleMqtt()' in w:
            w = w.replace('static void handleMqtt()', handler + 'static void handleMqtt()', 1)
        else:
            w += '\n' + handler
    if 'server.on("/api/lilygo/lte/rx-debug"' not in w:
        route = 'server.on("/api/lilygo/lte/rx-debug", HTTP_GET, handleLteRxDebug);'
        if 'server.on("/api/lilygo/lte/debug"' in w:
            idx = w.find('server.on("/api/lilygo/lte/debug"')
            line_end = w.find('\n', idx)
            w = w[:line_end+1] + '    ' + route + '\n' + w[line_end+1:]
        elif 'server.begin();' in w:
            w = w.replace('server.begin();', route + '\n    server.begin();', 1)
    write(web_cpp, w)

print('Applied clean LilyGO LTE stack v3.')
