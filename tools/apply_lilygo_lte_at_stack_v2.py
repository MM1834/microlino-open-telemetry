
from pathlib import Path
import re

root = Path.cwd()

def replace_func(s, ret, name, body):
    pattern = ret + r'\s+' + re.escape(name) + r'\s*\([^)]*\)\s*\{'
    m = re.search(pattern, s)
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

open_func = """
bool lilygoLteTcpOpen(const String& host, uint16_t port)
{
    if (!modemReadyFlag) {
        lastMessage = "LTE TCP open failed: modem not ready";
        return false;
    }

    if (!lilygoEnsureGprsConnected()) {
        lastMessage = "LTE TCP open failed: GPRS not connected";
        return false;
    }

    lteTcpOpenFlag = false;
    lteRxBufferLen = 0;
    lteUrcText = "";

    atCommand("AT+CIPCLOSE=0", 2000);

    String netState = atCommand("AT+NETOPEN?", 3000);
    if (netState.indexOf("+NETOPEN: 1") < 0) {
        atCommand("AT+NETOPEN", 10000);
        netState = atCommand("AT+NETOPEN?", 3000);
    }

    while (SerialAT.available()) SerialAT.read();

    String cmd = "AT+CIPOPEN=0,\\"TCP\\",\\"" + host + "\\"," + String(port);
    SerialAT.print(cmd);
    SerialAT.print("\\r\\n");

    String response;
    bool sawOk = false;
    uint32_t start = millis();

    while (millis() - start < 12000) {
        while (SerialAT.available()) {
            response += (char)SerialAT.read();
        }

        if (response.indexOf("+CIPOPEN: 0,0") >= 0) {
            lteTcpOpenFlag = true;
            lastAt = response;
            lastMessage = "LTE TCP open confirmed " + host + ":" + String(port);
            return true;
        }

        int pos = response.indexOf("+CIPOPEN: 0,");
        if (pos >= 0 && response.indexOf("+CIPOPEN: 0,0") < 0) {
            lteTcpOpenFlag = false;
            lastAt = response;
            lastMessage = "LTE TCP open failed: " + response;
            return false;
        }

        if (response.indexOf("\\r\\nOK") >= 0 || response.endsWith("OK")) {
            sawOk = true;
        }

        if (sawOk && millis() - start > 1200) {
            lteTcpOpenFlag = true;
            lastAt = response;
            lastMessage = "LTE TCP open assumed after OK " + host + ":" + String(port);
            return true;
        }

        delay(10);
    }

    lteTcpOpenFlag = false;
    lastAt = response;
    lastMessage = "LTE TCP open timeout: " + response;
    return false;
}
"""

write_func = """
int lilygoLteTcpWrite(const uint8_t* data, size_t len)
{
    if (!lteTcpOpenFlag || len == 0) return 0;

    String pre;
    while (SerialAT.available()) {
        char c = (char)SerialAT.read();
        pre += c;
        lteUrcText += c;
        if (lteUrcText.length() > 1200) {
            lteUrcText.remove(0, lteUrcText.length() - 1200);
        }
    }

    String cmd = "AT+CIPSEND=0," + String(len);
    SerialAT.print(cmd);
    SerialAT.print("\\r\\n");

    String prompt;
    uint32_t promptStart = millis();
    bool gotPrompt = false;

    while (millis() - promptStart < 8000) {
        while (SerialAT.available()) {
            char c = (char)SerialAT.read();
            prompt += c;
            if (c == '>') {
                gotPrompt = true;
                break;
            }

            if (prompt.indexOf("ERROR") >= 0 || prompt.indexOf("FAIL") >= 0) {
                lteTcpOpenFlag = false;
                lastAt = pre + prompt;
                lastMessage = "LTE CIPSEND prompt failed";
                return 0;
            }
        }

        if (gotPrompt) break;
        delay(5);
    }

    if (!gotPrompt) {
        lastAt = pre + prompt;
        lastMessage = "LTE CIPSEND prompt timeout";
        return 0;
    }

    SerialAT.write(data, len);
    SerialAT.flush();

    String response;
    uint32_t sendStart = millis();

    while (millis() - sendStart < 12000) {
        while (SerialAT.available()) {
            char c = (char)SerialAT.read();
            response += c;
            lteUrcText += c;
            if (lteUrcText.length() > 1200) {
                lteUrcText.remove(0, lteUrcText.length() - 1200);
            }
        }

        if (response.indexOf("SEND OK") >= 0 || response.indexOf("DATA ACCEPT") >= 0) {
            lastAt = pre + prompt + response;
            lastMessage = "LTE TCP write OK";
            ltePollIncoming(50);
            return (int)len;
        }

        if (response.indexOf("SEND FAIL") >= 0 || response.indexOf("ERROR") >= 0) {
            lteTcpOpenFlag = false;
            lastAt = pre + prompt + response;
            lastMessage = "LTE TCP write failed";
            return 0;
        }

        delay(5);
    }

    lastAt = pre + prompt + response;
    lastMessage = "LTE TCP write timeout";
    return 0;
}
"""

connected_func = """
bool lilygoLteTcpConnected()
{
    ltePollIncoming(5);
    return lteTcpOpenFlag;
}
"""

close_func = """
void lilygoLteTcpClose()
{
    atCommand("AT+CIPCLOSE=0", 2000);
    lteTcpOpenFlag = false;
    lteRxBufferLen = 0;
    lteUrcText = "";
}
"""

lte_connected_cpp = """
uint8_t LilygoLteClient::connected()
{
    connectedFlag = lilygoLteTcpConnected();
    lteClientLastConnected = connectedFlag;
    return connectedFlag ? 1 : 0;
}
"""

modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
s = modem_cpp.read_text(encoding="utf-8")

if "static uint8_t lteRxBuffer" not in s or "ltePollIncoming" not in s:
    raise SystemExit("URC RX buffer not found. Apply mot-lilygo-lte-urc-rx-buffer first.")

s = replace_func(s, "bool", "lilygoLteTcpOpen", open_func)
s = replace_func(s, "int", "lilygoLteTcpWrite", write_func)
s = replace_func(s, "bool", "lilygoLteTcpConnected", connected_func)
s = replace_func(s, "void", "lilygoLteTcpClose", close_func)
modem_cpp.write_text(s, encoding="utf-8")

lte_cpp = root / "firmware/lilygo-t-a7670/src/lte/lilygo_lte_client.cpp"
if lte_cpp.exists():
    t = lte_cpp.read_text(encoding="utf-8")
    if "uint8_t LilygoLteClient::connected()" in t:
        t = replace_func(t, "uint8_t", "LilygoLteClient::connected", lte_connected_cpp)
    lte_cpp.write_text(t, encoding="utf-8")

print("Applied LilyGO LTE AT stack v2.")
