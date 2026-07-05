#include "lilygo_modem.h"
#include "board_config.h"
#include "config/lilygo_config.h"

static HardwareSerial SerialAT(1);
static bool modemReadyFlag=false, simReadyFlag=false, networkRegisteredFlag=false, gprsAttachedFlag=false, pdpConfiguredFlag=false;
static String modemInfo="", revision="", imei="", operatorName="", registration="", gprs="", signalInfo="", lastAt="", lastMessage="", ipInfo="";
static unsigned long lastPollMs=0;
static unsigned long lastGprsAttemptMs=0;

static String esc(String s){s.replace("\\","\\\\");s.replace("\"","\\\"");s.replace("\r","\\r");s.replace("\n","\\n");return s;}

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
    if(modemReadyFlag) bringup(); else lastMessage="AT failed";
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
