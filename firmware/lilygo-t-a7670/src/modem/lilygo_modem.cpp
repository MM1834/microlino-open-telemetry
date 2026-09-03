#include "lilygo_modem.h"

#include <Arduino.h>
#include <Client.h>

#if !defined(TINY_GSM_MODEM_SIM7670G) && !defined(TINY_GSM_MODEM_A76XXSSL)
#define TINY_GSM_MODEM_A76XXSSL
#endif

#ifndef TINY_GSM_USE_GPRS
#define TINY_GSM_USE_GPRS true
#endif

#include <TinyGsmClient.h>

#include "board_config.h"
#include "config/lilygo_config.h"

#ifndef SerialAT
#define SerialAT Serial1
#endif

#ifndef MODEM_BAUD
#define MODEM_BAUD 115200
#endif

static TinyGsm modem(SerialAT);
static TinyGsmClient lteClient(modem);
static TinyGsmClientSecure lteSecureClient(modem);

static bool modemReadyFlag = false;
static bool networkReadyFlag = false;
static bool gprsReadyFlag = false;
static bool tcpOpenFlag = false;

static String lastMessage = "";
static String lastTrace = "";
static uint32_t lastGprsEnsureMs = 0;
static uint32_t connectAttempts = 0;
static uint32_t consecutiveConnectFailures = 0;
static uint32_t recoveryCount = 0;
static uint32_t tcpOpenCount = 0;
static uint32_t tcpFailCount = 0;
static uint32_t bytesWritten = 0;
static uint32_t bytesRead = 0;

#if defined(TINY_GSM_MODEM_SIM7670G)
static constexpr const char* MODEM_BACKEND = "LewisXhe TinyGSM SIM7670G";
#else
static constexpr const char* MODEM_BACKEND = "LewisXhe TinyGSM A76XXSSL";
#endif

static void traceAppend(const String& line)
{
    lastTrace += String(millis()) + "ms " + line + "\n";

    if (lastTrace.length() > 5000) {
        lastTrace.remove(0, lastTrace.length() - 5000);
    }
}

static String esc(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

static void setupPins()
{
#ifdef BOARD_POWER_ON_PIN
    pinMode(BOARD_POWER_ON_PIN, OUTPUT);
    digitalWrite(BOARD_POWER_ON_PIN, HIGH);
#endif

#ifdef MODEM_PWR_PIN
    pinMode(MODEM_PWR_PIN, OUTPUT);
#endif

#ifdef MODEM_PWRKEY_PIN
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
#endif

#ifdef MODEM_RST_PIN
    pinMode(MODEM_RST_PIN, OUTPUT);
    digitalWrite(MODEM_RST_PIN, HIGH);
    delay(100);
    digitalWrite(MODEM_RST_PIN, LOW);
    delay(2600);
    digitalWrite(MODEM_RST_PIN, HIGH);
#endif

#ifdef MODEM_DTR_PIN
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_DTR_PIN, LOW);
#endif

#ifdef BOARD_POWER_SAVE_MODE_PIN
    pinMode(BOARD_POWER_SAVE_MODE_PIN, OUTPUT);
    // High selects the full-current modem supply mode required during LTE use.
    digitalWrite(BOARD_POWER_SAVE_MODE_PIN, HIGH);
#endif
}

static void powerKeyPulse()
{
#ifdef MODEM_PWR_PIN
    digitalWrite(MODEM_PWR_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWR_PIN, LOW);
#elif defined(MODEM_PWRKEY_PIN)
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(MODEM_POWERON_PULSE_WIDTH_MS);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
#endif
}

static bool initTinyGsmModem()
{
    Serial.printf("Initializing modem with %s...\n", MODEM_BACKEND);
    traceAppend("modem.init");

    modemReadyFlag = modem.init();

    if (!modemReadyFlag) {
        lastMessage = "TinyGSM modem init failed";
        traceAppend(lastMessage);
        return false;
    }

    String name = modem.getModemName();
    String info = modem.getModemInfo();

    Serial.printf("Modem Name: %s\n", name.c_str());
    Serial.printf("Modem Info: %s\n", info.c_str());

    traceAppend("modem name=" + name);
    traceAppend("modem info=" + info);

    lastMessage = "TinyGSM modem ready";
    return true;
}

static bool connectNetworkAndGprs(uint32_t timeoutMs = 15000)
{
    if (!modemReadyFlag && !initTinyGsmModem()) {
        return false;
    }

    connectAttempts++;

#if defined(TINY_GSM_MODEM_HAS_NETWORK_MODE)
    // Not all TinyGSM versions expose this symbol; keep disabled by default.
#endif

    Serial.print("Waiting for network...");
    traceAppend("waitForNetwork");

    networkReadyFlag = modem.waitForNetwork(timeoutMs);

    if (!networkReadyFlag) {
        Serial.println("fail");
        lastMessage = "TinyGSM network wait failed";
        traceAppend(lastMessage);
        gprsReadyFlag = false;
        consecutiveConnectFailures++;
        return false;
    }

    Serial.println("success");
    traceAppend("network connected");

    if (config.lteApn.isEmpty()) {
        lastMessage = "No LTE APN configured";
        traceAppend(lastMessage);
        gprsReadyFlag = false;
        return false;
    }

    Serial.printf("Connecting to %s...", config.lteApn.c_str());
    traceAppend("gprsConnect apn=" + config.lteApn);

    gprsReadyFlag = modem.gprsConnect(
        config.lteApn.c_str(),
        config.lteUser.c_str(),
        config.ltePass.c_str()
    );

    if (!gprsReadyFlag) {
        Serial.println("fail");
        lastMessage = "TinyGSM GPRS connect failed";
        traceAppend(lastMessage);
        consecutiveConnectFailures++;
        return false;
    }

    Serial.println("success");
    Serial.printf("IP Address: %s\n", modem.getLocalIP().c_str());

    lastGprsEnsureMs = millis();
    consecutiveConnectFailures = 0;
    lastMessage = "TinyGSM GPRS connected ip=" + modem.getLocalIP();
    traceAppend(lastMessage);

    return true;
}

void setupLilygoModem()
{
#if defined(LILYGO_SIM7670G_S3_STANDARD)
    Serial.println("LilyGO T-SIM7670G-S3: LTE modem setup");
#else
    Serial.println("LilyGO T-A7670G: LTE modem setup");
#endif
    Serial.printf(
        "Modem pins RX=%d TX=%d PWR=%d POWER_ON=%d RST=%d DTR=%d RI=%d\n",
        MODEM_RX_PIN,
        MODEM_TX_PIN,
#ifdef MODEM_PWR_PIN
        MODEM_PWR_PIN,
#else
        -1,
#endif
#ifdef BOARD_POWER_ON_PIN
        BOARD_POWER_ON_PIN,
#else
        -1,
#endif
#ifdef MODEM_RST_PIN
        MODEM_RST_PIN,
#else
        -1,
#endif
#ifdef MODEM_DTR_PIN
        MODEM_DTR_PIN,
#else
        -1,
#endif
#ifdef MODEM_RI_PIN
        MODEM_RI_PIN
#else
        -1
#endif
    );

    setupPins();

    SerialAT.end();
    delay(200);
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(300);

#if defined(LILYGO_SIM7670G_S3_STANDARD)
    // PWRKEY toggles the modem state. Probe first so an already running modem
    // is not accidentally switched off, then allow the cold boot enough time
    // to expose its UART before calling the more extensive init sequence.
    bool responding = modem.testAT(1000);
    if (!responding) {
        powerKeyPulse();
        const uint32_t deadline = millis() + 15000;
        while (!responding && static_cast<int32_t>(deadline - millis()) > 0) {
            delay(500);
            responding = modem.testAT(1000);
        }
    }
    if (!responding) {
        Serial.println("SIM7670G UART did not respond after power-on");
    }
    initTinyGsmModem();
#else
    powerKeyPulse();
    delay(3000);

    if (!initTinyGsmModem()) {
        Serial.println("TinyGSM init failed, retry power pulse...");
        powerKeyPulse();
        delay(5000);
        initTinyGsmModem();
    }
#endif

    // Packet data is established by the WiFi-first network manager only when
    // WiFi is unavailable. Modem setup must not delay a working WiFi path.
}

bool lilygoConfigureIntegratedGpsNmea(bool enabled)
{
#if defined(LILYGO_SIM7670G_S3_STANDARD)
    if (!modemReadyFlag) {
        lastMessage = "SIM7670G GNSS unavailable: modem not ready";
        traceAppend(lastMessage);
        return false;
    }

    if (!enabled) {
        modem.disableNMEA();
        const bool disabled = modem.disableGPS(
            MODEM_GPS_ENABLE_GPIO,
            MODEM_GPS_ENABLE_LEVEL ? 0 : 1);
        lastMessage = disabled ? "SIM7670G GNSS disabled" : "SIM7670G GNSS disable failed";
        traceAppend(lastMessage);
        return disabled;
    }

    bool ok = modem.enableGPS(MODEM_GPS_ENABLE_GPIO, MODEM_GPS_ENABLE_LEVEL);
    if (ok) ok = modem.setGPSBaud(GPS_BAUD);
    if (ok) ok = modem.setGPSMode(15);  // GPS + GLONASS + Galileo + BeiDou
    if (ok) ok = modem.setGPSOutputRate(1);
    if (ok) ok = modem.enableNMEA(false);  // route NMEA to GPIO48/45, not AT UART

    lastMessage = ok ? "SIM7670G GNSS NMEA ready" : "SIM7670G GNSS NMEA setup failed";
    traceAppend(lastMessage);
    return ok;
#else
    (void)enabled;
    return false;
#endif
}

bool lilygoEnsureGprsConnected()
{
    if (config.lteApn.isEmpty()) {
        gprsReadyFlag = false;
        lastMessage = "No LTE APN configured";
        return false;
    }

    if (!modemReadyFlag && !initTinyGsmModem()) {
        return false;
    }

    if (gprsReadyFlag && modem.isGprsConnected()) {
        lastGprsEnsureMs = millis();
        return true;
    }

    const bool connected = connectNetworkAndGprs(15000);

    // Escalate repeated registration/PDP failures to a bounded modem recovery.
    // The network manager applies the outer retry backoff.
    if (!connected && consecutiveConnectFailures >= 4) {
        recoveryCount++;
        consecutiveConnectFailures = 0;
        modemReadyFlag = false;
        networkReadyFlag = false;
        gprsReadyFlag = false;
        lteClient.stop();
        lteSecureClient.stop();
        traceAppend("modem recovery power pulse");
        powerKeyPulse();
        delay(3000);
        initTinyGsmModem();
    }

    return connected;
}

bool lilygoGprsConnected()
{
    if (!modemReadyFlag) return false;

    bool ok = modem.isGprsConnected();

    if (!ok) {
        gprsReadyFlag = false;
    }

    return ok;
}

String lilygoLteIp()
{
    if (!modemReadyFlag) return "";
    if (!gprsReadyFlag && !modem.isGprsConnected()) return "";
    return modem.getLocalIP();
}

Client* lilygoTinyGsmSecureClient()
{
    return &lteSecureClient;
}

static bool uploadTlsFile(const char* filename, const String& contents)
{
#if defined(TINY_GSM_MODEM_SIM7670G)
    // SIM7670G exposes the generic SIMCom filesystem API rather than the
    // A76XX certificate download helpers. fs_del() may fail when the file is
    // absent, which is harmless; fs_write() returns the written byte count.
    modem.fs_del(filename);
    return modem.fs_write(
        filename,
        reinterpret_cast<const uint8_t*>(contents.c_str()),
        contents.length()) == contents.length();
#else
    modem.deleteCertificate(filename);
    return modem.downloadCertificate(filename, contents.c_str());
#endif
}

bool lilygoConfigureAwsTlsClient(
    const String& rootCa,
    const String& certificate,
    const String& privateKey
) {
    if (!lilygoEnsureGprsConnected()) return false;

    static const char* ROOT_CA_FILE = "motRootCA.pem";
    static const char* CLIENT_CERT_FILE = "motClient.pem";
    static const char* PRIVATE_KEY_FILE = "motKey.pem";

    // SIMCom stores TLS material in its own filesystem. Never trace the
    // buffers; only report which upload stage failed.
    if (!uploadTlsFile(ROOT_CA_FILE, rootCa)) {
        lastMessage = "AWS TLS: root CA upload failed";
        traceAppend(lastMessage);
        return false;
    }
    if (!uploadTlsFile(CLIENT_CERT_FILE, certificate)) {
        lastMessage = "AWS TLS: client certificate upload failed";
        traceAppend(lastMessage);
        return false;
    }
    if (!uploadTlsFile(PRIVATE_KEY_FILE, privateKey)) {
        lastMessage = "AWS TLS: private key upload failed";
        traceAppend(lastMessage);
        return false;
    }

    lteSecureClient.setCertificate(ROOT_CA_FILE);
    lteSecureClient.setClientCertificate(CLIENT_CERT_FILE);
    lteSecureClient.setClientPrivateKey(PRIVATE_KEY_FILE);
    lastMessage = "AWS TLS: modem client configured";
    traceAppend(lastMessage);
    return true;
}

Client* lilygoTinyGsmClient()
{
    return &lteClient;
}

void lilygoTinyGsmMaintain()
{
    if (!modemReadyFlag) return;
    modem.maintain();
}


bool lilygoLteTcpOpen(const String& host, uint16_t port)
{
    if (!lilygoEnsureGprsConnected()) {
        tcpOpenFlag = false;
        tcpFailCount++;
        lastMessage = "TCP open failed: GPRS unavailable";
        traceAppend(lastMessage);
        return false;
    }

    tcpOpenFlag = false;
    lteClient.stop();

    traceAppend("TCP connect host=" + host + " port=" + String(port));

    uint32_t start = millis();
    tcpOpenFlag = lteClient.connect(host.c_str(), port);
    uint32_t elapsed = millis() - start;

    if (tcpOpenFlag) {
        tcpOpenCount++;
        lastMessage = "TCP connected in " + String(elapsed) + "ms";
    } else {
        tcpFailCount++;
        lastMessage = "TCP connect failed in " + String(elapsed) + "ms";
    }

    traceAppend(lastMessage);
    return tcpOpenFlag;
}

int lilygoLteTcpWrite(const uint8_t* data, size_t len)
{
    if (!tcpOpenFlag || !data || len == 0) return 0;

    size_t n = lteClient.write(data, len);
    bytesWritten += n;

    traceAppend("TCP write len=" + String(len) + " result=" + String((int)n));

    if (n == 0) {
        tcpOpenFlag = lteClient.connected();
    }

    return (int)n;
}

int lilygoLteTcpAvailable()
{
    if (!tcpOpenFlag) return 0;
    return lteClient.available();
}

int lilygoLteTcpRead(uint8_t* buffer, size_t len)
{
    if (!tcpOpenFlag || !buffer || len == 0) return 0;

    int n = lteClient.read(buffer, len);

    if (n > 0) {
        bytesRead += n;
        traceAppend("TCP read result=" + String(n));
    } else if (n < 0) {
        tcpOpenFlag = lteClient.connected();
        n = 0;
    }

    return n;
}

bool lilygoLteTcpConnected()
{
    tcpOpenFlag = lteClient.connected();
    return tcpOpenFlag;
}

void lilygoLteTcpClose()
{
    lteClient.stop();
    tcpOpenFlag = false;
    traceAppend("TCP close");
}

String lilygoLteTcpTestJson(const String& host, uint16_t port)
{
    uint32_t start = millis();
    bool ok = lilygoLteTcpOpen(host, port);
    uint32_t elapsed = millis() - start;
    lilygoLteTcpClose();

    String json = "{";
    json += "\"backend\":\"" + String(MODEM_BACKEND) + "\",";
    json += "\"host\":\"" + esc(host) + "\",";
    json += "\"port\":" + String(port) + ",";
    json += "\"tcpOpen\":" + String(ok ? "true" : "false") + ",";
    json += "\"elapsedMs\":" + String(elapsed) + ",";
    json += "\"modemReady\":" + String(modemReadyFlag ? "true" : "false") + ",";
    json += "\"networkReady\":" + String(networkReadyFlag ? "true" : "false") + ",";
    json += "\"gprsConnected\":" + String(lilygoGprsConnected() ? "true" : "false") + ",";
    json += "\"lteIp\":\"" + esc(lilygoLteIp()) + "\",";
    json += "\"message\":\"" + esc(lastMessage) + "\"";
    json += "}";
    return json;
}

String lilygoLteRxDebugJson()
{
    String json = "{";
    json += "\"backend\":\"" + String(MODEM_BACKEND) + "\",";
    json += "\"tcpOpenFlag\":" + String(tcpOpenFlag ? "true" : "false") + ",";
    json += "\"available\":" + String(lteClient.available()) + ",";
    json += "\"connected\":" + String(lteClient.connected() ? "true" : "false") + ",";
    json += "\"bytesWritten\":" + String(bytesWritten) + ",";
    json += "\"bytesRead\":" + String(bytesRead) + ",";
    json += "\"message\":\"" + esc(lastMessage) + "\",";
    json += "\"trace\":\"" + esc(lastTrace) + "\"";
    json += "}";
    return json;
}

String lilygoTinyGsmTraceJson()
{
    return lilygoLteRxDebugJson();
}

void lilygoTinyGsmTraceClear()
{
    lastTrace = "";
    bytesWritten = 0;
    bytesRead = 0;
}

String lilygoLteDebugJson()
{
    return lilygoModemStatusJson();
}

String lilygoModemStatusJson()
{
    int signal = modemReadyFlag ? modem.getSignalQuality() : 99;

    String json = "{";
    json += "\"backend\":\"" + String(MODEM_BACKEND) + "\",";
    json += "\"modemReady\":" + String(modemReadyFlag ? "true" : "false") + ",";
    json += "\"networkReady\":" + String(networkReadyFlag ? "true" : "false") + ",";
    json += "\"gprsAttached\":" + String(lilygoGprsConnected() ? "true" : "false") + ",";
    json += "\"pdpConfigured\":" + String(gprsReadyFlag ? "true" : "false") + ",";
    json += "\"lteIp\":\"" + esc(lilygoLteIp()) + "\",";
    json += "\"signalQuality\":" + String(signal) + ",";
    json += "\"connectAttempts\":" + String(connectAttempts) + ",";
    json += "\"consecutiveConnectFailures\":" + String(consecutiveConnectFailures) + ",";
    json += "\"recoveryCount\":" + String(recoveryCount) + ",";
    json += "\"tcpOpen\":" + String(tcpOpenFlag ? "true" : "false") + ",";
    json += "\"tcpOpenCount\":" + String(tcpOpenCount) + ",";
    json += "\"tcpFailCount\":" + String(tcpFailCount) + ",";
    json += "\"bytesWritten\":" + String(bytesWritten) + ",";
    json += "\"bytesRead\":" + String(bytesRead) + ",";
    json += "\"message\":\"" + esc(lastMessage) + "\"";

    if (modemReadyFlag) {
        json += ",\"modemName\":\"" + esc(modem.getModemName()) + "\"";
        json += ",\"modemInfo\":\"" + esc(modem.getModemInfo()) + "\"";
    }

    json += "}";
    return json;
}

void lilygoModemLoop()
{
    // TinyGSM transport currently has no periodic modem work.
}
