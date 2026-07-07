from pathlib import Path
import re

root = Path.cwd()
modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"

s = modem_cpp.read_text(encoding="utf-8")

new_func = '\nbool lilygoLteTcpOpen(const String& host, uint16_t port)\n{\n    if (!modemReadyFlag) {\n        lastMessage = "LTE TCP open failed: modem not ready";\n        return false;\n    }\n\n    if (!lilygoEnsureGprsConnected()) {\n        lastMessage = "LTE TCP open failed: GPRS not connected";\n        return false;\n    }\n\n    // Start from a known socket state.\n    atCommand("AT+CIPCLOSE=0", 3000);\n\n    // NETOPEN may already be active. That is OK.\n    String netState = atCommand("AT+NETOPEN?", 3000);\n    if (netState.indexOf("+NETOPEN: 1") < 0) {\n        atCommand("AT+NETOPEN", 10000);\n        netState = atCommand("AT+NETOPEN?", 3000);\n    }\n\n    while (SerialAT.available()) SerialAT.read();\n\n    String cmd = "AT+CIPOPEN=0,\\"TCP\\",\\"" + host + "\\"," + String(port);\n    SerialAT.print(cmd);\n    SerialAT.print("\\r\\n");\n\n    String response;\n    uint32_t start = millis();\n\n    while (millis() - start < 25000) {\n        while (SerialAT.available()) {\n            response += (char)SerialAT.read();\n        }\n\n        // Command accepted + socket really opened.\n        if (response.indexOf("+CIPOPEN: 0,0") >= 0) {\n            lteTcpOpenFlag = true;\n            lastAt = response;\n            lastMessage = "LTE TCP open " + host + ":" + String(port);\n            return true;\n        }\n\n        // Any other CIPOPEN result for socket 0 is a failure.\n        int pos = response.indexOf("+CIPOPEN: 0,");\n        if (pos >= 0 && response.indexOf("+CIPOPEN: 0,0") < 0) {\n            lteTcpOpenFlag = false;\n            lastAt = response;\n            lastMessage = "LTE TCP open failed: " + response;\n            return false;\n        }\n\n        if (response.indexOf("ERROR") >= 0) {\n            lteTcpOpenFlag = false;\n            lastAt = response;\n            lastMessage = "LTE TCP open failed: " + response;\n            return false;\n        }\n\n        delay(10);\n    }\n\n    lteTcpOpenFlag = false;\n    lastAt = response;\n    lastMessage = "LTE TCP open timeout: " + response;\n    return false;\n}\n'

pattern = re.compile(
    r'bool\s+lilygoLteTcpOpen\s*\(\s*const\s+String\s*&\s*host\s*,\s*uint16_t\s+port\s*\)\s*\{.*?\n\}',
    re.DOTALL
)

matches = list(pattern.finditer(s))
if not matches:
    raise SystemExit("Could not find lilygoLteTcpOpen() in lilygo_modem.cpp")

# Replace the last occurrence in case old patch artifacts left duplicates.
m = matches[-1]
s = s[:m.start()] + new_func.strip() + s[m.end():]

modem_cpp.write_text(s, encoding="utf-8")
print("Patched lilygoLteTcpOpen(): now waits for +CIPOPEN: 0,0 URC.")
