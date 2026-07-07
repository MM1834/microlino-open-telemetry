
from pathlib import Path
import re

root = Path.cwd()

available_func = 'int lilygoLteTcpAvailable()\n{\n    if (!lteTcpOpenFlag) return 0;\n\n    String r = atCommand("AT+CIPRXGET=4,0", 3000);\n\n    int marker = r.indexOf("+CIPRXGET: 4,0,");\n    if (marker < 0) {\n        if (r.indexOf("CLOSED") >= 0 || r.indexOf("+IPCLOSE") >= 0) {\n            lteTcpOpenFlag = false;\n        }\n        return 0;\n    }\n\n    int start = marker + String("+CIPRXGET: 4,0,").length();\n    int end = r.indexOf(\'\\\\r\', start);\n    if (end < 0) end = r.indexOf(\'\\\\n\', start);\n    if (end < 0) end = r.length();\n\n    int available = r.substring(start, end).toInt();\n    if (available < 0) available = 0;\n    return available;\n}\n'
read_func = 'int lilygoLteTcpRead(uint8_t* buffer, size_t len)\n{\n    if (!lteTcpOpenFlag || !buffer || len == 0) return 0;\n\n    size_t requestLen = len;\n    if (requestLen > 512) requestLen = 512;\n\n    while (SerialAT.available()) SerialAT.read();\n\n    String cmd = "AT+CIPRXGET=2,0," + String(requestLen);\n    SerialAT.print(cmd);\n    SerialAT.print("\\\\r\\\\n");\n\n    String header;\n    uint32_t startMs = millis();\n    int bytesToRead = -1;\n\n    while (millis() - startMs < 5000) {\n        while (SerialAT.available()) {\n            char c = (char)SerialAT.read();\n            header += c;\n\n            int marker = header.indexOf("+CIPRXGET: 2,0,");\n            if (marker >= 0) {\n                int valueStart = marker + String("+CIPRXGET: 2,0,").length();\n                int valueEnd = header.indexOf(\',\', valueStart);\n                if (valueEnd < 0) valueEnd = header.indexOf(\'\\\\r\', valueStart);\n                if (valueEnd < 0) valueEnd = header.indexOf(\'\\\\n\', valueStart);\n\n                if (valueEnd > valueStart) {\n                    bytesToRead = header.substring(valueStart, valueEnd).toInt();\n                    break;\n                }\n            }\n\n            if (header.indexOf("ERROR") >= 0 || header.indexOf("CLOSED") >= 0) {\n                lteTcpOpenFlag = false;\n                lastAt = header;\n                return 0;\n            }\n        }\n\n        if (bytesToRead >= 0) break;\n        delay(5);\n    }\n\n    if (bytesToRead <= 0) {\n        lastAt = header;\n        return 0;\n    }\n\n    if ((size_t)bytesToRead > requestLen) bytesToRead = (int)requestLen;\n\n    int readCount = 0;\n    uint32_t payloadStart = millis();\n\n    while (readCount < bytesToRead && millis() - payloadStart < 5000) {\n        while (SerialAT.available() && readCount < bytesToRead) {\n            buffer[readCount++] = (uint8_t)SerialAT.read();\n        }\n        delay(2);\n    }\n\n    String tail;\n    uint32_t tailStart = millis();\n    while (millis() - tailStart < 300) {\n        while (SerialAT.available()) {\n            tail += (char)SerialAT.read();\n        }\n        if (tail.indexOf("OK") >= 0 || tail.indexOf("ERROR") >= 0) break;\n        delay(5);\n    }\n\n    lastAt = header + tail;\n    return readCount;\n}\n'

def replace_named_function(source, return_type, name, replacement):
    pattern = re.compile(
        return_type + r'\s+' + re.escape(name) + r'\s*\([^)]*\)\s*\{',
        re.DOTALL
    )
    m = pattern.search(source)
    if not m:
        raise SystemExit("Could not find " + name)

    start = m.start()
    brace = source.find('{', m.end() - 1)
    depth = 0
    end = None
    for i in range(brace, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                end = i + 1
                break
    if end is None:
        raise SystemExit("Could not parse " + name)

    return source[:start] + replacement.strip() + source[end:]

modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
s = modem_cpp.read_text(encoding="utf-8")
s = replace_named_function(s, "int", "lilygoLteTcpAvailable", available_func)
s = replace_named_function(s, "int", "lilygoLteTcpRead", read_func)
modem_cpp.write_text(s, encoding="utf-8")
print("Patched lilygoLteTcpAvailable() and lilygoLteTcpRead() using AT+CIPRXGET.")
