from pathlib import Path

root = Path.cwd()
HELPERS = '\nstatic bool waitForModemAt(uint32_t timeoutMs, uint32_t intervalMs)\n{\n    uint32_t start = millis();\n\n    while (millis() - start < timeoutMs) {\n        String r = atCommand("AT", 1200);\n        if (r.indexOf("OK") >= 0) {\n            modemReadyFlag = true;\n            lastMessage = "AT ready";\n            return true;\n        }\n\n        delay(intervalMs);\n    }\n\n    modemReadyFlag = false;\n    return false;\n}\n\nstatic void reinitModemUart()\n{\n    SerialAT.end();\n    delay(200);\n    SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);\n    delay(300);\n}\n\nstatic void softRecoverModem()\n{\n    atCommand("AT+CIPCLOSE=0", 2000);\n    atCommand("AT+NETCLOSE", 5000);\n    atCommand("AT+CFUN=0", 8000);\n    delay(3000);\n    atCommand("AT+CFUN=1", 10000);\n    delay(5000);\n}\n\nstatic void hardPowerPulseModem()\n{\n    digitalWrite(MODEM_PWRKEY_PIN, HIGH);\n    delay(1000);\n    digitalWrite(MODEM_PWRKEY_PIN, LOW);\n    delay(1200);\n    digitalWrite(MODEM_PWRKEY_PIN, HIGH);\n    delay(5000);\n}\n\nstatic bool recoverModemAt()\n{\n    Serial.println("Modem recovery: UART retry");\n    reinitModemUart();\n    if (waitForModemAt(8000, 1000)) return true;\n\n    Serial.println("Modem recovery: soft CFUN reset");\n    softRecoverModem();\n    reinitModemUart();\n    if (waitForModemAt(15000, 1000)) return true;\n\n    Serial.println("Modem recovery: power-key pulse");\n    hardPowerPulseModem();\n    reinitModemUart();\n    if (waitForModemAt(20000, 1000)) return true;\n\n    lastMessage = "AT failed after recovery attempts";\n    return false;\n}\n'
OLD_BLOCK = '    Serial.print("Powering modem, waiting for AT ");\n    modemReadyFlag = false;\n    for (int i = 0; i < 24; i++) {\n        String r = atCommand("AT", 1000);\n        if (r.indexOf("OK") >= 0) {\n            modemReadyFlag = true;\n            break;\n        }\n        Serial.print(".");\n        delay(1000);\n    }\n    Serial.println();\n    Serial.printf("modem AT ready=%d\\n", modemReadyFlag ? 1 : 0);\n\n    if (!modemReadyFlag) {\n        lastMessage = "AT failed";\n        return;\n    }\n'
NEW_BLOCK = '    Serial.print("Powering modem, waiting for AT ");\n    modemReadyFlag = waitForModemAt(24000, 1000);\n    Serial.println();\n    Serial.printf("modem AT ready=%d\\n", modemReadyFlag ? 1 : 0);\n\n    if (!modemReadyFlag) {\n        modemReadyFlag = recoverModemAt();\n        Serial.printf("modem AT ready after recovery=%d\\n", modemReadyFlag ? 1 : 0);\n    }\n\n    if (!modemReadyFlag) {\n        lastMessage = "AT failed";\n        return;\n    }\n'

def read(p):
    return p.read_text(encoding="utf-8")

def write(p, s):
    p.write_text(s, encoding="utf-8")

modem_cpp = root / "firmware/lilygo-t-a7670/src/modem/lilygo_modem.cpp"
s = read(modem_cpp)

if "static bool recoverModemAt()" not in s:
    marker = "void setupLilygoModem()"
    if marker not in s:
        raise SystemExit("Could not find setupLilygoModem()")
    s = s.replace(marker, HELPERS + "\n\n" + marker, 1)

if OLD_BLOCK in s:
    s = s.replace(OLD_BLOCK, NEW_BLOCK, 1)
elif 'Serial.printf("modem AT ready=%d\\n", modemReadyFlag ? 1 : 0);' in s and "modem AT ready after recovery" not in s:
    needle = 'Serial.printf("modem AT ready=%d\\n", modemReadyFlag ? 1 : 0);'
    s = s.replace(needle, needle + '''

    if (!modemReadyFlag) {
        modemReadyFlag = recoverModemAt();
        Serial.printf("modem AT ready after recovery=%d\\n", modemReadyFlag ? 1 : 0);
    }
''', 1)

write(modem_cpp, s)
print("Applied LilyGO modem boot recovery.")
