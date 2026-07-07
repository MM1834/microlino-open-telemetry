#include "lilygo_modem.h"
#include "board_config.h"
#include "config/lilygo_config.h"
#include <string.h>

static HardwareSerial SerialAT(1);
static bool modemReadyFlag=false, simReadyFlag=false, networkRegisteredFlag=false, gprsAttachedFlag=false, pdpConfiguredFlag=false;
static String modemInfo="", revision="", imei="", operatorName="", registration="", gprs="", signalInfo="", lastAt="", lastMessage="", ipInfo="";
static unsigned long lastPollMs=0;
static bool lteTcpOpenFlag=false;
static size_t lteTcpBufferedAvailable=0;
static unsigned long lastGprsAttemptMs=0;

static String esc(String s){s.replace("\\","\\\\");s.replace("\"","\\\"");s.replace("\r","\r");s.replace("\n","\n");return s;}

static String atCommand(const String& cmd, uint32_t timeoutMs=3000)
{
    while(SerialAT.available()) SerialAT.read();
    SerialAT.print(cmd); SerialAT.print("\r\n");
    uint32_t start=millis(); String response;
    while(millis()-start<timeoutMs){
        while(SerialAT.available()) response+=(char)SerialAT.read();
        if(response.indexOf("\r\nOK\r\n")>=0 || response.indexOf("\r\nERROR\r\n")>=0 || response.endsWith("OK") || response.endsWith("ERROR")) break;
        delay(10);
    }
    response.trim(); lastAt=response; return response;
}
static bool ok(const String& r){return r.indexOf("OK")>=0;}
static bool registered(const String& r){return r.indexOf(",1")>=0 || r.indexOf(",5")>=0;}

static void modemPowerOn()
{
#ifdef BOARD_POWER_ON_PIN
    pinMode(BOARD_POWER_ON_PIN,OUTPUT);
    digitalWrite(BOARD_POWER_ON_PIN,HIGH);
    delay(100);
#endif
#ifdef MODEM_RST_PIN
    pinMode(MODEM_RST_PIN,OUTPUT);
    digitalWrite(MODEM_RST_PIN,HIGH);
    delay(100);
#endif
#ifdef MODEM_DTR_PIN
    pinMode(MODEM_DTR_PIN,OUTPUT);
    digitalWrite(MODEM_DTR_PIN,LOW);
#endif
#ifdef MODEM_PWR_PIN
    pinMode(MODEM_PWR_PIN,OUTPUT);
    digitalWrite(MODEM_PWR_PIN,HIGH);
    delay(1200);
    digitalWrite(MODEM_PWR_PIN,LOW);
    delay(8000);
#endif
}

static bool waitForAt(uint32_t timeoutMs)
{
    uint32_t start=millis();
    while(millis()-start<timeoutMs){
        String r = atCommand("AT",1000);
        if(ok(r)) return true;
        Serial.print(".");
        delay(1000);
    }
    return false;
}

static void configurePdp()
{
    String cmd="AT+CGDCONT=1,\"IP\",\""+config.lteApn+"\"";
    String r=atCommand(cmd,3000);
    pdpConfiguredFlag=ok(r);
    lastMessage=pdpConfiguredFlag?"PDP context configured":"PDP config failed: "+r;
}

static void updateStatus()
{
    if(!modemReadyFlag) return;
    signalInfo=atCommand("AT+CSQ",2000);
    registration=atCommand("AT+CEREG?",2000);
    if(!registered(registration)){
        String creg=atCommand("AT+CREG?",2000);
        if(creg.indexOf("+CREG:")>=0) registration=creg;
    }
    networkRegisteredFlag=registered(registration);
    gprs=atCommand("AT+CGATT?",2000);
    gprsAttachedFlag=gprs.indexOf("+CGATT: 1")>=0;
    String cops=atCommand("AT+COPS?",3000);
    if(cops.indexOf("+COPS:")>=0) operatorName=cops;
    ipInfo=atCommand("AT+CGPADDR=1",3000);
}

static void readIdentity()
{
    modemInfo=atCommand("ATI",3000);
    revision=atCommand("AT+CGMR",3000);
    imei=atCommand("AT+CGSN",3000);
    String sim=atCommand("AT+CPIN?",3000);
    simReadyFlag=sim.indexOf("READY")>=0;
}

bool lilygoEnsureGprsConnected()
{
    if (!modemReadyFlag) return false;

    updateStatus();
    if (gprsAttachedFlag) return true;

    if (millis() - lastGprsAttemptMs < 10000) return false;
    lastGprsAttemptMs = millis();

    if (!pdpConfiguredFlag) configurePdp();

    Serial.println("LTE: GPRS attach/reconnect attempt");
    String att = atCommand("AT+CGATT=1", 10000);
    if(!ok(att)) {
        lastMessage = "CGATT failed or pending: " + att;
    }

    updateStatus();

    if (gprsAttachedFlag) {
        lastMessage = "LTE GPRS attached";
    }

    return gprsAttachedFlag;
}

static void bringup()
{
    modemReadyFlag=ok(atCommand("AT",1500));
    if(!modemReadyFlag){lastMessage="AT failed";return;}
    readIdentity();
    configurePdp();
    lilygoEnsureGprsConnected();
    if(networkRegisteredFlag && gprsAttachedFlag) lastMessage="LTE registered and GPRS attached";
    else if(simReadyFlag) lastMessage="SIM ready, waiting for network/GPRS";
}


static bool waitForModemAt(uint32_t timeoutMs, uint32_t intervalMs)
{
    uint32_t start = millis();

    while (millis() - start < timeoutMs) {
        String r = atCommand("AT", 1200);
        if (r.indexOf("OK") >= 0) {
            modemReadyFlag = true;
            lastMessage = "AT ready";
            return true;
        }

        delay(intervalMs);
    }

    modemReadyFlag = false;
    return false;
}

static void reinitModemUart()
{
    SerialAT.end();
    delay(200);
    SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    delay(300);
}

static void softRecoverModem()
{
    atCommand("AT+CIPCLOSE=0", 2000);
    atCommand("AT+NETCLOSE", 5000);
    atCommand("AT+CFUN=0", 8000);
    delay(3000);
    atCommand("AT+CFUN=1", 10000);
    delay(5000);
}

static void hardPowerPulseModem()
{
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(1200);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(5000);
}

static bool recoverModemAt()
{
    Serial.println("Modem recovery: UART retry");
    reinitModemUart();
    if (waitForModemAt(8000, 1000)) return true;

    Serial.println("Modem recovery: soft CFUN reset");
    softRecoverModem();
    reinitModemUart();
    if (waitForModemAt(15000, 1000)) return true;

    Serial.println("Modem recovery: power-key pulse");
    hardPowerPulseModem();
    reinitModemUart();
    if (waitForModemAt(20000, 1000)) return true;

    lastMessage = "AT failed after recovery attempts";
    return false;
}


void setupLilygoModem()
{
    Serial.println("LilyGO T-A7670G: LTE modem setup");
    Serial.printf("Modem pins RX=%d TX=%d PWR=%d POWER_ON=%d RST=%d DTR=%d RI=%d\n",MODEM_RX_PIN,MODEM_TX_PIN,MODEM_PWR_PIN,BOARD_POWER_ON_PIN,MODEM_RST_PIN,MODEM_DTR_PIN,MODEM_RI_PIN);

    SerialAT.begin(MODEM_BAUD,SERIAL_8N1,MODEM_RX_PIN,MODEM_TX_PIN);
    delay(300);

    Serial.print("Powering modem, waiting for AT ");
    modemPowerOn();
    modemReadyFlag = waitForAt(45000);
    Serial.println();

    Serial.printf("modem AT ready=%d\n",modemReadyFlag?1:0);

    if (!modemReadyFlag) {
        modemReadyFlag = recoverModemAt();
        Serial.printf("modem AT ready after recovery=%d\n", modemReadyFlag ? 1 : 0);
    }

    if (modemReadyFlag) {
        bringup();
    } else {
        lastMessage = "AT failed";
    }

}

void lilygoModemLoop()
{
    if(millis()-lastPollMs>15000){
        lastPollMs=millis();
        updateStatus();
        if(modemReadyFlag && !gprsAttachedFlag){
            lilygoEnsureGprsConnected();
        }
    }
}
bool lilygoModemReady(){return modemReadyFlag;}
bool lilygoNetworkRegistered(){return networkRegisteredFlag;}
bool lilygoGprsConnected(){return gprsAttachedFlag;}

String lilygoModemStatusJson()
{
    String json="{";
    json+="\"modemReady\":"+String(modemReadyFlag?"true":"false")+",";
    json+="\"simReady\":"+String(simReadyFlag?"true":"false")+",";
    json+="\"networkRegistered\":"+String(networkRegisteredFlag?"true":"false")+",";
    json+="\"gprsAttached\":"+String(gprsAttachedFlag?"true":"false")+",";
    json+="\"pdpConfigured\":"+String(pdpConfiguredFlag?"true":"false")+",";
    json+="\"rxPin\":"+String(MODEM_RX_PIN)+",\"txPin\":"+String(MODEM_TX_PIN)+",\"baud\":"+String(MODEM_BAUD)+",";
    json+="\"apn\":\""+esc(config.lteApn)+"\",";
    json+="\"operator\":\""+esc(operatorName)+"\",";
    json+="\"signal\":\""+esc(signalInfo)+"\",";
    json+="\"registration\":\""+esc(registration)+"\",";
    json+="\"gprs\":\""+esc(gprs)+"\",";
    json+="\"ipInfo\":\""+esc(ipInfo)+"\",";
    json+="\"modemInfo\":\""+esc(modemInfo)+"\",";
    json+="\"revision\":\""+esc(revision)+"\",";
    json+="\"imei\":\""+esc(imei)+"\",";
    json+="\"lastAt\":\""+esc(lastAt)+"\",";
    json+="\"message\":\""+esc(lastMessage)+"\"";
    json+="}"; return json;
}

String lilygoLteIp()
{
    // Expected:
    // AT+CGPADDR=1
    // +CGPADDR: 1,10.244.254.19
    // OK

    int pos = ipInfo.indexOf("+CGPADDR:");
    if (pos < 0) return "";

    pos = ipInfo.indexOf(',', pos);
    if (pos < 0) return "";

    pos++;

    int end = ipInfo.indexOf('\r', pos);
    if (end < 0) end = ipInfo.indexOf('\n', pos);
    if (end < 0) end = ipInfo.length();

    String ip = ipInfo.substring(pos, end);
    ip.trim();

    return ip;
}


#ifndef MOT_LILYGO_LTE_TCP_FUNCTIONS_APPENDED
#define MOT_LILYGO_LTE_TCP_FUNCTIONS_APPENDED

static String atCommandPrompt(const String& cmd, const uint8_t* data, size_t len, uint32_t timeoutMs)
{
    while (SerialAT.available()) SerialAT.read();

    SerialAT.print(cmd);
    SerialAT.print("\r\n");

    uint32_t start = millis();
    String response;
    bool prompted = false;

    while (millis() - start < timeoutMs) {
        while (SerialAT.available()) {
            char c = (char)SerialAT.read();
            response += c;
            if (c == '>') {
                prompted = true;
                break;
            }
        }

        if (prompted) break;
        delay(10);
    }

    if (!prompted) {
        response.trim();
        lastAt = response;
        return response;
    }

    SerialAT.write(data, len);
    SerialAT.flush();

    start = millis();
    while (millis() - start < timeoutMs) {
        while (SerialAT.available()) {
            response += (char)SerialAT.read();
        }

        if (response.indexOf("SEND OK") >= 0 ||
            response.indexOf("ERROR") >= 0 ||
            response.indexOf("FAIL") >= 0) {
            break;
        }

        delay(10);
    }

    response.trim();
    lastAt = response;
    return response;
}


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


static size_t parseCipRxGetAvailable(const String& r)
{
    int pos = r.indexOf("+CIPRXGET:");
    if (pos < 0) return 0;

    int comma1 = r.indexOf(',', pos);
    if (comma1 < 0) return 0;

    int comma2 = r.indexOf(',', comma1 + 1);
    if (comma2 < 0) return 0;

    String n = r.substring(comma2 + 1);
    n.trim();
    return (size_t)n.toInt();
}

int lilygoLteTcpAvailable()
{
    if (!lteTcpOpenFlag && lteRxBufferLen == 0) return 0;
    ltePollIncoming(25);
    return (int)lteRxBufferLen;
}


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


void lilygoLteTcpClose()
{
    atCommand("AT+CIPCLOSE=0", 2000);
    lteTcpOpenFlag = false;
    lteRxReset();
}


bool lilygoLteTcpConnected()
{
    ltePollIncoming(5);
    return lteTcpOpenFlag;
}


#endif



static String jsonEscLteDebug(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

String lilygoLteDebugJson()
{
    String csq = atCommand("AT+CSQ", 3000);
    String cereg = atCommand("AT+CEREG?", 3000);
    String cgatt = atCommand("AT+CGATT?", 3000);
    String cgpaddr = atCommand("AT+CGPADDR=1", 3000);
    String netopen = atCommand("AT+NETOPEN?", 3000);
    String cipstatus = atCommand("AT+CIPSTATUS", 5000);

    String json = "{";
    json += "\"modemReady\":" + String(modemReadyFlag ? "true" : "false") + ",";
    json += "\"simReady\":" + String(simReadyFlag ? "true" : "false") + ",";
    json += "\"networkRegistered\":" + String(networkRegisteredFlag ? "true" : "false") + ",";
    json += "\"pdpConfigured\":" + String(pdpConfiguredFlag ? "true" : "false") + ",";
    json += "\"lteIp\":\"" + jsonEscLteDebug(lilygoLteIp()) + "\",";
    json += "\"lteTcpConnected\":" + String(lilygoLteTcpConnected() ? "true" : "false") + ",";
    json += "\"signal\":\"" + jsonEscLteDebug(csq) + "\",";
    json += "\"registration\":\"" + jsonEscLteDebug(cereg) + "\",";
    json += "\"gprs\":\"" + jsonEscLteDebug(cgatt) + "\",";
    json += "\"ipInfo\":\"" + jsonEscLteDebug(cgpaddr) + "\",";
    json += "\"netopen\":\"" + jsonEscLteDebug(netopen) + "\",";
    json += "\"cipstatus\":\"" + jsonEscLteDebug(cipstatus) + "\",";
    json += "\"lastAt\":\"" + jsonEscLteDebug(lastAt) + "\",";
    json += "\"message\":\"" + jsonEscLteDebug(lastMessage) + "\"";
    json += "}";
    return json;
}



static String jsonEscLteTcpTest(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

String lilygoLteTcpTestJson(const String& host, uint16_t port)
{
    unsigned long start = millis();

    String closeResp = atCommand("AT+CIPCLOSE=0", 3000);
    String netOpenResp = atCommand("AT+NETOPEN", 10000);
    String netOpenQuery = atCommand("AT+NETOPEN?", 3000);

    String cmd = "AT+CIPOPEN=0,\"TCP\",\"" + host + "\"," + String(port);
    String openResp = atCommand(cmd, 25000);

    unsigned long elapsed = millis() - start;

    bool tcpOpen =
        openResp.indexOf("+CIPOPEN: 0,0") >= 0 ||
        openResp.indexOf("OK") >= 0 ||
        openResp.indexOf("ALREADY") >= 0;

    String cipStatus = atCommand("AT+CIPSTATUS", 5000);
    String closeAfter = atCommand("AT+CIPCLOSE=0", 3000);
    lteTcpOpenFlag = false;

    String json = "{";
    json += "\"host\":\"" + jsonEscLteTcpTest(host) + "\",";
    json += "\"port\":" + String(port) + ",";
    json += "\"tcpOpen\":" + String(tcpOpen ? "true" : "false") + ",";
    json += "\"elapsedMs\":" + String(elapsed) + ",";
    json += "\"modemReady\":" + String(modemReadyFlag ? "true" : "false") + ",";
    json += "\"simReady\":" + String(simReadyFlag ? "true" : "false") + ",";
    json += "\"networkRegistered\":" + String(networkRegisteredFlag ? "true" : "false") + ",";
    json += "\"gprsAttached\":" + String(gprsAttachedFlag ? "true" : "false") + ",";
    json += "\"pdpConfigured\":" + String(pdpConfiguredFlag ? "true" : "false") + ",";
    json += "\"lteIp\":\"" + jsonEscLteTcpTest(lilygoLteIp()) + "\",";
    json += "\"closeBefore\":\"" + jsonEscLteTcpTest(closeResp) + "\",";
    json += "\"netOpen\":\"" + jsonEscLteTcpTest(netOpenResp) + "\",";
    json += "\"netOpenQuery\":\"" + jsonEscLteTcpTest(netOpenQuery) + "\",";
    json += "\"cipOpen\":\"" + jsonEscLteTcpTest(openResp) + "\",";
    json += "\"cipStatus\":\"" + jsonEscLteTcpTest(cipStatus) + "\",";
    json += "\"closeAfter\":\"" + jsonEscLteTcpTest(closeAfter) + "\",";
    json += "\"lastAt\":\"" + jsonEscLteTcpTest(lastAt) + "\"";
    json += "}";

    return json;
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

