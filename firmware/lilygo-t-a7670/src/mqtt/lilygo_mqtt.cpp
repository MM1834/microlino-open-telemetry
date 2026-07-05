#include "lilygo_mqtt.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config/lilygo_config.h"
#include "network/lilygo_network.h"
#include "modem/lilygo_modem.h"
#include "gps/l76k_gps.h"
#include "lte/lilygo_lte_client.h"
#include "telemetry/telemetry.h"

static WiFiClient wifiClient;
static LilygoLteClient lteClient;
static PubSubClient mqtt;
static String activeTransport = "";
static unsigned long lastReconnectAttemptMs = 0;
static unsigned long lastPublishMs = 0;
static uint32_t publishCount = 0;
static uint32_t connectAttempts = 0;
static int lastConnectState = 0;
static String lastMessage = "";

static const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;
static const unsigned long MQTT_PUBLISH_INTERVAL_MS = 5000;

static String esc(String s){ s.replace("\\","\\\\"); s.replace("\"","\\\""); s.replace("\r","\\r"); s.replace("\n","\\n"); return s; }

static const char* mqttStateText(int state)
{
    switch (state) {
        case MQTT_CONNECTION_TIMEOUT: return "connection timeout";
        case MQTT_CONNECTION_LOST: return "connection lost";
        case MQTT_CONNECT_FAILED: return "connect failed";
        case MQTT_DISCONNECTED: return "disconnected";
        case MQTT_CONNECTED: return "connected";
        case MQTT_CONNECT_BAD_PROTOCOL: return "bad protocol";
        case MQTT_CONNECT_BAD_CLIENT_ID: return "bad client id";
        case MQTT_CONNECT_UNAVAILABLE: return "server unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS: return "bad credentials";
        case MQTT_CONNECT_UNAUTHORIZED: return "unauthorized";
        default: return "unknown";
    }
}

static bool mqttEnabled(){ String host=config.mqttHost; host.trim(); return !host.isEmpty(); }

static String mqttClientId()
{
    String id=lilygoDeviceName(); id.trim(); id.toLowerCase(); id.replace(" ","-"); id.replace("/","-");
    if(id.isEmpty()) id="mot-lilygo"; if(!id.startsWith("mot-")) id="mot-"+id; return id;
}

static String topic(const char* suffix)
{
    String prefix=config.mqttPrefix; prefix.trim(); while(prefix.endsWith("/")) prefix.remove(prefix.length()-1); if(prefix.isEmpty()) prefix="mot";
    String vehicle=config.vehicleId; vehicle.trim(); vehicle.replace("/","-"); if(vehicle.isEmpty()) vehicle="pioneer";
    return prefix+"/"+vehicle+"/"+suffix;
}

static String wantedTransport()
{
    if (WiFi.status() == WL_CONNECTED) return "WiFi";
    if (lilygoGprsConnected()) return "LTE";
    return "";
}

static bool transportAvailable(){ return wantedTransport().length() > 0; }

static void selectTransport()
{
    String wanted=wantedTransport();
    if(wanted == activeTransport) return;
    if(mqtt.connected()) mqtt.disconnect();

    if(wanted=="WiFi"){ mqtt.setClient(wifiClient); activeTransport="WiFi"; Serial.println("MQTT: transport selected WiFi"); }
    else if(wanted=="LTE"){ mqtt.setClient(lteClient); activeTransport="LTE"; Serial.println("MQTT: transport selected LTE"); }
    else { activeTransport=""; lastMessage="MQTT transport unavailable"; }
    lastReconnectAttemptMs=0;
}

static void reconnectMqtt()
{
    if(!mqttEnabled()){ lastMessage="MQTT disabled: no host configured"; return; }
    selectTransport();
    if(!transportAvailable()){ if(mqtt.connected()) mqtt.disconnect(); lastMessage="MQTT transport unavailable: no WiFi/LTE route"; return; }
    if(mqtt.connected()) return;
    if(lastReconnectAttemptMs && millis()-lastReconnectAttemptMs<MQTT_RECONNECT_INTERVAL_MS) return;
    lastReconnectAttemptMs=millis(); connectAttempts++;

    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
    mqtt.setKeepAlive(30); mqtt.setSocketTimeout(10);

    Serial.printf("MQTT %s: connecting host=%s port=%u clientId=%s\n", activeTransport.c_str(), config.mqttHost.c_str(), config.mqttPort, mqttClientId().c_str());
    bool ok = config.mqttUser.length() ? mqtt.connect(mqttClientId().c_str(), config.mqttUser.c_str(), config.mqttPass.c_str()) : mqtt.connect(mqttClientId().c_str());
    lastConnectState=mqtt.state();
    if(ok){ lastMessage="MQTT connected via "+activeTransport; Serial.printf("MQTT %s: connected\n", activeTransport.c_str()); }
    else { lastMessage="MQTT connect failed rc="+String(lastConnectState)+" ("+mqttStateText(lastConnectState)+")"; Serial.printf("MQTT %s: failed rc=%d (%s)\n", activeTransport.c_str(), lastConnectState, mqttStateText(lastConnectState)); }
}

static void publishFloat(const char* suffix, float value, int decimals=1){ if(!mqtt.connected()||isnan(value)) return; char buf[32]; dtostrf(value,0,decimals,buf); mqtt.publish(topic(suffix).c_str(),buf,true); }
static void publishDouble(const char* suffix, double value, int decimals=6){ if(!mqtt.connected()||isnan(value)) return; char buf[40]; dtostrf(value,0,decimals,buf); mqtt.publish(topic(suffix).c_str(),buf,true); }
static void publishInt(const char* suffix, int value){ if(!mqtt.connected()) return; char buf[24]; snprintf(buf,sizeof(buf),"%d",value); mqtt.publish(topic(suffix).c_str(),buf,true); }
static void publishBool(const char* suffix, bool value){ if(!mqtt.connected()) return; mqtt.publish(topic(suffix).c_str(), value?"1":"0", true); }

void setupLilygoMqtt()
{
    mqtt.setClient(wifiClient);
    if(mqttEnabled()){ mqtt.setServer(config.mqttHost.c_str(), config.mqttPort); mqtt.setKeepAlive(30); mqtt.setSocketTimeout(10); Serial.printf("MQTT: enabled host=%s port=%u clientId=%s transport=WiFi/LTE\n", config.mqttHost.c_str(), config.mqttPort, mqttClientId().c_str()); lastMessage="MQTT enabled; waiting for transport"; }
    else { Serial.println("MQTT: disabled (no host configured)"); lastMessage="MQTT disabled: no host configured"; }
}

void lilygoMqttLoop()
{
    if(!mqttEnabled()) return;
    selectTransport();
    if(!transportAvailable()){ if(mqtt.connected()) mqtt.disconnect(); lastMessage="MQTT transport unavailable: no WiFi/LTE route"; return; }
    reconnectMqtt();
    mqtt.loop();
    if(mqtt.connected() && millis()-lastPublishMs>MQTT_PUBLISH_INTERVAL_MS){ lastPublishMs=millis(); publishLilygoTelemetry(); }
}

void publishLilygoTelemetry()
{
    if(!mqttEnabled() || !mqtt.connected()) return;
    publishFloat("display/soc", telemetry.display.soc);
    publishFloat("display/speed_kmh", telemetry.display.speedKmh);
    publishFloat("display/odometer_km", telemetry.display.odometerKm);
    publishInt("display/estimated_range_km", telemetry.display.estimatedRangeKm);
    publishBool("charging/is_charging", telemetry.charging.isCharging);
    publishBool("charging/plugged", telemetry.charging.plugged);
    publishInt("charging/power_display", telemetry.charging.powerDisplay);
    publishInt("charging/power_signed", telemetry.charging.powerSigned);

    if(l76kGpsValid()){
        publishDouble("location/latitude", l76kLatitude(), 6);
        publishDouble("location/longitude", l76kLongitude(), 6);
        publishFloat("location/speed_kmph", (float)l76kSpeedKmph(), 1);
        publishInt("location/satellites", (int)l76kSatellites());
        if(!isnan(l76kHdop())) publishFloat("location/hdop", (float)l76kHdop(), 1);
        publishInt("location/age_ms", (int)l76kLocationAgeMs());
    }

    mqtt.publish(topic("system/device_id").c_str(), lilygoDeviceName().c_str(), true);
    mqtt.publish(topic("system/device_name").c_str(), lilygoDeviceName().c_str(), true);
    mqtt.publish(topic("system/mqtt_client_id").c_str(), mqttClientId().c_str(), true);
    mqtt.publish(topic("system/firmware_version").c_str(), telemetry.system.firmwareVersion.c_str(), true);
    mqtt.publish(topic("system/ip_address").c_str(), telemetry.system.ipAddress.c_str(), true);
    mqtt.publish(topic("system/network_mode").c_str(), telemetry.system.networkMode.c_str(), true);
    mqtt.publish(topic("system/mqtt_transport").c_str(), activeTransport.c_str(), true);
    publishInt("system/wifi_rssi", telemetry.system.wifiRssi);
    publishInt("system/uptime_sec", telemetry.system.uptimeSec);
    publishCount++; lastMessage="Telemetry published via "+activeTransport;
}

String lilygoMqttStatusJson()
{
    String json="{";
    json+="\"enabled\":"+String(mqttEnabled()?"true":"false")+",";
    json+="\"connected\":"+String(mqtt.connected()?"true":"false")+",";
    json+="\"transport\":\""+esc(activeTransport)+"\",";
    json+="\"transportAvailable\":"+String(transportAvailable()?"true":"false")+",";
    json+="\"wantedTransport\":\""+esc(wantedTransport())+"\",";
    json+="\"host\":\""+esc(config.mqttHost)+"\",";
    json+="\"port\":"+String(config.mqttPort)+",";
    json+="\"clientId\":\""+esc(mqttClientId())+"\",";
    json+="\"state\":"+String(mqtt.state())+",";
    json+="\"stateText\":\""+String(mqttStateText(mqtt.state()))+"\",";
    json+="\"lastConnectState\":"+String(lastConnectState)+",";
    json+="\"lastConnectStateText\":\""+String(mqttStateText(lastConnectState))+"\",";
    json+="\"connectAttempts\":"+String(connectAttempts)+",";
    json+="\"publishCount\":"+String(publishCount)+",";
    json+="\"wifiIp\":\""+esc(WiFi.localIP().toString())+"\",";
    json+="\"lteIp\":\""+esc(lilygoLteIp())+"\",";
    json+="\"message\":\""+esc(lastMessage)+"\"}";
    return json;
}

String lilygoMqttDebugJson()
{
    String json="{";
    json+="\"wifiConnected\":"+String(WiFi.status()==WL_CONNECTED?"true":"false")+",";
    json+="\"networkMode\":\""+esc(lilygoNetworkModeName())+"\",";
    json+="\"wantedTransport\":\""+esc(wantedTransport())+"\",";
    json+="\"activeTransport\":\""+esc(activeTransport)+"\",";
    json+="\"transportAvailable\":"+String(transportAvailable()?"true":"false")+",";
    json+="\"mqttHost\":\""+esc(config.mqttHost)+"\",";
    json+="\"mqttPort\":"+String(config.mqttPort)+",";
    json+="\"lteGprsConnected\":"+String(lilygoGprsConnected()?"true":"false")+",";
    json+="\"lteIp\":\""+esc(lilygoLteIp())+"\",";
    json+="\"lteTcpConnected\":"+String(lilygoLteTcpConnected()?"true":"false")+",";
    json+="\"mqttConnected\":"+String(mqtt.connected()?"true":"false")+",";
    json+="\"mqttState\":"+String(mqtt.state())+",";
    json+="\"mqttStateText\":\""+String(mqttStateText(mqtt.state()))+"\",";
    json+="\"connectAttempts\":"+String(connectAttempts)+",";
    json+="\"message\":\""+esc(lastMessage)+"\"}";
    return json;
}
