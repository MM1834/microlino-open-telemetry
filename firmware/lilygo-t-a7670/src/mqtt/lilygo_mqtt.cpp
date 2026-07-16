#include "lilygo_mqtt.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <esp_system.h>

#ifdef MOT_AWS_IOT
#include <WiFiClientSecure.h>
#include "aws/aws_iot_credentials.h"
#endif

#include "config/lilygo_config.h"
#include "network/lilygo_network.h"
#include "modem/lilygo_modem.h"
#include "gps/l76k_gps.h"
#include "lte/lilygo_lte_client.h"
#include "telemetry/telemetry.h"

#ifdef MOT_AWS_IOT
static WiFiClientSecure wifiClient;
#else
static WiFiClient wifiClient;
static LilygoLteClient lteClient;
#endif
static PubSubClient mqtt;
static String activeTransport = "";
static unsigned long lastReconnectAttemptMs = 0;
static unsigned long lastPublishMs = 0;
static uint32_t publishCount = 0;
static uint32_t connectAttempts = 0;
static uint32_t heartbeatCount = 0;
static uint32_t birthCount = 0;
static unsigned long lastHeartbeatMs = 0;
static bool previousMqttConnected = false;
static int lastConnectState = 0;
static String lastMessage = "";

static const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000;
static const unsigned long MQTT_LTE_RECONNECT_INTERVAL_MS = 60000;
static const unsigned long MQTT_PUBLISH_INTERVAL_MS = 5000;
static const unsigned long MQTT_HEARTBEAT_INTERVAL_MS = 30000;

static String topic(const char* suffix);
static String mqttClientId();

static bool ntpSyncRequested = false;
static const time_t MIN_VALID_UTC = 1700000000;

static void requestUtcSyncIfPossible()
{
    // ESP32 SNTP uses the WiFi/lwIP route. For LTE-only operation, a future
    // modem/GPS time provider can set the ESP32 system clock; the publish
    // function below will then automatically use that valid UTC.
    if (ntpSyncRequested || WiFi.status() != WL_CONNECTED) return;

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ntpSyncRequested = true;
    Serial.println("UTC: NTP synchronization requested via WiFi");
}

static bool utcTimeValid()
{
    return time(nullptr) >= MIN_VALID_UTC;
}

static bool publishMessage(const String& fullTopic, const String& payload, bool retained)
{
    if (!mqtt.connected()) return false;

    const bool ok = mqtt.publish(fullTopic.c_str(), payload.c_str(), retained);
    if (ok) publishCount++;
    return ok;
}

static bool publishSuffix(const char* suffix, const String& payload, bool retained)
{
    return publishMessage(topic(suffix), payload, retained);
}

static bool publishLastSeenUtc()
{
    requestUtcSyncIfPossible();

    const time_t nowUtc = time(nullptr);
    if (nowUtc < MIN_VALID_UTC) return false;

    char value[24];
    snprintf(value, sizeof(value), "%lld", static_cast<long long>(nowUtc));
    return publishSuffix("system/last_seen_utc", value, true);
}

static const char* resetReasonText(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON: return "power_on";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_watchdog";
        case ESP_RST_TASK_WDT: return "task_watchdog";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep_sleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

static bool publishHeartbeat(bool force)
{
    if (!mqtt.connected() || !utcTimeValid()) return false;

    const unsigned long nowMs = millis();
    if (!force && lastHeartbeatMs &&
        nowMs - lastHeartbeatMs < MQTT_HEARTBEAT_INTERVAL_MS) {
        return false;
    }

    lastHeartbeatMs = nowMs;

    char payload[320];
    snprintf(
        payload,
        sizeof(payload),
        "{\"utc\":%lld,\"uptime_sec\":%lu,\"free_heap\":%u,"
        "\"network_mode\":\"%s\",\"transport\":\"%s\","
        "\"ip_address\":\"%s\",\"wifi_rssi\":%d}",
        static_cast<long long>(time(nullptr)),
        nowMs / 1000UL,
        ESP.getFreeHeap(),
        lilygoNetworkModeName().c_str(),
        activeTransport.c_str(),
        lilygoNetworkIp().c_str(),
        WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0
    );

    const bool ok = publishSuffix("system/heartbeat", payload, false);
    if (ok) heartbeatCount++;
    return ok;
}

static void publishBirthMessages()
{
    if (!mqtt.connected()) return;

    publishSuffix("status/online", "true", true);
    publishSuffix("system/device_id", lilygoDeviceName(), true);
    publishSuffix("system/device_name", lilygoDeviceName(), true);
    publishSuffix("system/mqtt_client_id", mqttClientId(), true);
    publishSuffix("system/firmware_version", telemetry.system.firmwareVersion, true);
    publishSuffix("system/network_mode", lilygoNetworkModeName(), true);
    publishSuffix("system/mqtt_transport", activeTransport, true);
    publishSuffix("system/ip_address", lilygoNetworkIp(), true);
    publishSuffix("system/boot_reason", resetReasonText(esp_reset_reason()), true);
    publishLastSeenUtc();
    publishHeartbeat(true);
    birthCount++;
}

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

static bool mqttEnabled()
{
#ifdef MOT_AWS_IOT
    return awsIotCredentials().loaded;
#else
    String host = config.mqttHost;
    host.trim();
    return !host.isEmpty();
#endif
}

static String mqttClientId()
{
#ifdef MOT_AWS_IOT
    return awsIotCredentials().thingName;
#else
    String id=lilygoDeviceName(); id.trim(); id.toLowerCase(); id.replace(" ","-"); id.replace("/","-");
    if(id.isEmpty()) id="mot-lilygo"; if(!id.startsWith("mot-")) id="mot-"+id; return id;
#endif
}

static String topic(const char* suffix)
{
#ifdef MOT_AWS_IOT
    String prefix = awsIotCredentials().topicPrefix;
    String vehicle = awsIotCredentials().vehicleId;
#else
    String prefix = config.mqttPrefix;
    String vehicle = config.vehicleId;
#endif
    prefix.trim();
    while(prefix.endsWith("/")) prefix.remove(prefix.length()-1);
    if(prefix.isEmpty()) prefix="mot";
    vehicle.trim();
    vehicle.replace("/","-");
    if(vehicle.isEmpty()) vehicle="pioneer";
    return prefix+"/"+vehicle+"/"+suffix;
}

static String wantedTransport()
{
    if (WiFi.status() == WL_CONNECTED) return "WiFi";
#ifndef MOT_AWS_IOT
    if (lilygoGprsConnected()) return "LTE";
#endif
    return "";
}


static bool transportAvailable(){ return wantedTransport().length() > 0; }

static void selectTransport()
{
    String wanted=wantedTransport();
    if(wanted == activeTransport) return;
    if(mqtt.connected()) mqtt.disconnect();

    if(wanted=="WiFi"){
        mqtt.setClient(wifiClient);
        activeTransport="WiFi";
#ifdef MOT_AWS_IOT
        Serial.println("MQTT: transport selected AWS IoT over WiFi/TLS");
#else
        Serial.println("MQTT: transport selected WiFi");
#endif
    }
#ifndef MOT_AWS_IOT
    else if(wanted=="LTE"){ mqtt.setClient(lteClient); activeTransport="LTE"; Serial.println("MQTT: transport selected LTE"); }
#endif
    else { activeTransport=""; lastMessage="MQTT transport unavailable"; }
    lastReconnectAttemptMs=0;
}

static void reconnectMqtt()
{
    if(!mqttEnabled()){
#ifdef MOT_AWS_IOT
        lastMessage = "AWS IoT disabled: credentials not loaded";
#else
        lastMessage = "MQTT disabled: no host configured";
#endif
        return;
    }

    selectTransport();
    if(!transportAvailable()){
        if(mqtt.connected()) mqtt.disconnect();
#ifdef MOT_AWS_IOT
        lastMessage = "AWS IoT waiting for WiFi";
#else
        lastMessage = "MQTT transport unavailable: no WiFi/LTE route";
#endif
        return;
    }

    if(mqtt.connected()) return;

#ifdef MOT_AWS_IOT
    if(!utcTimeValid()){
        lastMessage = "AWS IoT waiting for valid UTC";
        return;
    }
    const unsigned long reconnectIntervalMs = MQTT_RECONNECT_INTERVAL_MS;
#else
    const unsigned long reconnectIntervalMs =
        activeTransport == "LTE" ? MQTT_LTE_RECONNECT_INTERVAL_MS : MQTT_RECONNECT_INTERVAL_MS;
#endif

    if(lastReconnectAttemptMs && millis()-lastReconnectAttemptMs<reconnectIntervalMs) return;
    lastReconnectAttemptMs=millis();
    connectAttempts++;

#ifdef MOT_AWS_IOT
    const AwsIotCredentials& aws = awsIotCredentials();
    mqtt.setServer(aws.endpoint.c_str(), aws.port);
#else
    mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
#endif
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(15);

#ifdef MOT_AWS_IOT
    Serial.printf(
        "AWS IoT: connecting endpoint=%s port=%u clientId=%s freeHeap=%u\n",
        aws.endpoint.c_str(),
        aws.port,
        mqttClientId().c_str(),
        ESP.getFreeHeap()
    );
    const String willTopic = topic("status/online");
    const bool ok = mqtt.connect(
        mqttClientId().c_str(),
        willTopic.c_str(),
        1,
        true,
        "false"
    );
#else
    Serial.printf(
        "MQTT %s: connecting host=%s port=%u clientId=%s\n",
        activeTransport.c_str(),
        config.mqttHost.c_str(),
        config.mqttPort,
        mqttClientId().c_str()
    );
    const bool ok = config.mqttUser.length()
        ? mqtt.connect(mqttClientId().c_str(), config.mqttUser.c_str(), config.mqttPass.c_str())
        : mqtt.connect(mqttClientId().c_str());
#endif

    lastConnectState=mqtt.state();
    if(ok){
#ifdef MOT_AWS_IOT
        lastMessage = "AWS IoT connected via WiFi/TLS";
        Serial.printf("AWS IoT: connected freeHeap=%u will=%s\n",
            ESP.getFreeHeap(), topic("status/online").c_str());
        publishBirthMessages();
#else
        lastMessage="MQTT connected via "+activeTransport;
        Serial.printf("MQTT %s: connected\n", activeTransport.c_str());
#endif
    } else {
#ifdef MOT_AWS_IOT
        lastMessage="AWS IoT connect failed rc="+String(lastConnectState)+" ("+mqttStateText(lastConnectState)+")";
        Serial.printf("AWS IoT: failed rc=%d (%s) freeHeap=%u\n",
            lastConnectState, mqttStateText(lastConnectState), ESP.getFreeHeap());
#else
        lastMessage="MQTT connect failed rc="+String(lastConnectState)+" ("+mqttStateText(lastConnectState)+")";
        Serial.printf("MQTT %s: failed rc=%d (%s)\n",
            activeTransport.c_str(), lastConnectState, mqttStateText(lastConnectState));
#endif
    }
}
static void publishFloat(const char* suffix, float value, int decimals=1)
{
    if (!mqtt.connected() || isnan(value)) return;
    char buf[32];
    dtostrf(value, 0, decimals, buf);
    publishSuffix(suffix, buf, true);
}

static void publishDouble(const char* suffix, double value, int decimals=6)
{
    if (!mqtt.connected() || isnan(value)) return;
    char buf[40];
    dtostrf(value, 0, decimals, buf);
    publishSuffix(suffix, buf, true);
}

static void publishInt(const char* suffix, int value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", value);
    publishSuffix(suffix, buf, true);
}

static void publishBool(const char* suffix, bool value)
{
    publishSuffix(suffix, value ? "1" : "0", true);
}

void setupLilygoMqtt()
{
#ifdef MOT_AWS_IOT
    if (!loadAwsIotCredentials()) {
        Serial.printf("AWS IoT: credential load failed: %s\n", awsIotCredentials().message.c_str());
        lastMessage = awsIotCredentials().message;
        return;
    }

    const AwsIotCredentials& aws = awsIotCredentials();
    wifiClient.setCACert(aws.rootCa.c_str());
    wifiClient.setCertificate(aws.certificate.c_str());
    wifiClient.setPrivateKey(aws.privateKey.c_str());
    wifiClient.setHandshakeTimeout(20);

    mqtt.setClient(wifiClient);
    mqtt.setServer(aws.endpoint.c_str(), aws.port);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(15);

    Serial.printf(
        "AWS IoT: enabled endpoint=%s port=%u clientId=%s credentials=LittleFS\n",
        aws.endpoint.c_str(),
        aws.port,
        aws.thingName.c_str()
    );
    lastMessage = "AWS IoT enabled; waiting for WiFi and valid UTC";
#else
    mqtt.setClient(wifiClient);
    if(mqttEnabled()){
        mqtt.setServer(config.mqttHost.c_str(), config.mqttPort);
        mqtt.setKeepAlive(30);
        mqtt.setSocketTimeout(10);
        Serial.printf(
            "MQTT: enabled host=%s port=%u clientId=%s transport=WiFi/LTE\n",
            config.mqttHost.c_str(),
            config.mqttPort,
            mqttClientId().c_str()
        );
        lastMessage="MQTT enabled; waiting for transport";
    } else {
        Serial.println("MQTT: disabled (no host configured)");
        lastMessage="MQTT disabled: no host configured";
    }
#endif
}
void lilygoMqttLoop()
{
    requestUtcSyncIfPossible();
    if (!mqttEnabled()) return;

    selectTransport();
    if (!transportAvailable()) {
        if (mqtt.connected()) mqtt.disconnect();
#ifdef MOT_AWS_IOT
        lastMessage = "AWS IoT waiting for WiFi";
#else
        lastMessage = "MQTT transport unavailable: no WiFi/LTE route";
#endif
        previousMqttConnected = false;
        return;
    }

    reconnectMqtt();

    const bool loopOk = mqtt.loop();
    const bool connectedNow = mqtt.connected();

    if (previousMqttConnected && !connectedNow) {
#ifdef MOT_AWS_IOT
        Serial.printf(
            "AWS IoT: connection lost state=%d (%s) loopOk=%s freeHeap=%u\n",
            mqtt.state(),
            mqttStateText(mqtt.state()),
            loopOk ? "true" : "false",
            ESP.getFreeHeap()
        );
#else
        Serial.printf(
            "MQTT: connection lost state=%d (%s)\n",
            mqtt.state(),
            mqttStateText(mqtt.state())
        );
#endif
    }
    previousMqttConnected = connectedNow;

    if (!connectedNow) return;

    publishHeartbeat(false);

    if (millis() - lastPublishMs > MQTT_PUBLISH_INTERVAL_MS) {
        lastPublishMs = millis();
        publishLilygoTelemetry();
    }
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

    publishSuffix("system/device_id", lilygoDeviceName(), true);
    publishSuffix("system/device_name", lilygoDeviceName(), true);
    publishSuffix("system/mqtt_client_id", mqttClientId(), true);
    publishSuffix("system/firmware_version", telemetry.system.firmwareVersion, true);
    publishSuffix("system/ip_address", lilygoNetworkIp(), true);
    publishSuffix("system/network_mode", lilygoNetworkModeName(), true);
    publishSuffix("system/mqtt_transport", activeTransport, true);
    publishInt("system/wifi_rssi", WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
    publishInt("system/uptime_sec", millis() / 1000UL);
    publishLastSeenUtc();
    lastMessage = "Telemetry published via " + activeTransport;
}

String lilygoMqttStatusJson()
{
    String json="{";
    json+="\"enabled\":"+String(mqttEnabled()?"true":"false")+",";
    json+="\"connected\":"+String(mqtt.connected()?"true":"false")+",";
    json+="\"transport\":\""+esc(activeTransport)+"\",";
    json+="\"transportAvailable\":"+String(transportAvailable()?"true":"false")+",";
    json+="\"wantedTransport\":\""+esc(wantedTransport())+"\",";
#ifdef MOT_AWS_IOT
    json+="\"mode\":\"AWS_IOT_X509\",";
    json+="\"host\":\""+esc(awsIotCredentials().endpoint)+"\",";
    json+="\"port\":"+String(awsIotCredentials().port)+",";
    json+="\"credentialsLoaded\":"+String(awsIotCredentials().loaded?"true":"false")+",";
#else
    json+="\"mode\":\"PLAIN_DEBUG\",";
    json+="\"host\":\""+esc(config.mqttHost)+"\",";
    json+="\"port\":"+String(config.mqttPort)+",";
#endif
    json+="\"clientId\":\""+esc(mqttClientId())+"\",";
    json+="\"state\":"+String(mqtt.state())+",";
    json+="\"stateText\":\""+String(mqttStateText(mqtt.state()))+"\",";
    json+="\"lastConnectState\":"+String(lastConnectState)+",";
    json+="\"lastConnectStateText\":\""+String(mqttStateText(lastConnectState))+"\",";
    json+="\"connectAttempts\":"+String(connectAttempts)+",";
    json+="\"timeValid\":"+String(utcTimeValid()?"true":"false")+",";
    json+="\"publishCount\":"+String(publishCount)+",";
    json+="\"heartbeatCount\":"+String(heartbeatCount)+",";
    json+="\"birthCount\":"+String(birthCount)+",";
    json+="\"lastHeartbeatMs\":"+String(lastHeartbeatMs)+",";
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
#ifdef MOT_AWS_IOT
    json+="\"mqttMode\":\"AWS_IOT_X509\",";
    json+="\"mqttHost\":\""+esc(awsIotCredentials().endpoint)+"\",";
    json+="\"mqttPort\":"+String(awsIotCredentials().port)+",";
    json+="\"credentialsLoaded\":"+String(awsIotCredentials().loaded?"true":"false")+",";
#else
    json+="\"mqttMode\":\"PLAIN_DEBUG\",";
    json+="\"mqttHost\":\""+esc(config.mqttHost)+"\",";
    json+="\"mqttPort\":"+String(config.mqttPort)+",";
#endif
    json+="\"lteGprsConnected\":"+String(lilygoGprsConnected()?"true":"false")+",";
    json+="\"lteIp\":\""+esc(lilygoLteIp())+"\",";
    json+="\"lteTcpConnected\":"+String(lilygoLteTcpConnected()?"true":"false")+",";
    json+="\"mqttConnected\":"+String(mqtt.connected()?"true":"false")+",";
    json+="\"mqttState\":"+String(mqtt.state())+",";
    json+="\"mqttStateText\":\""+String(mqttStateText(mqtt.state()))+"\",";
    json+="\"connectAttempts\":"+String(connectAttempts)+",";
    json+="\"heartbeatCount\":"+String(heartbeatCount)+",";
    json+="\"birthCount\":"+String(birthCount)+",";
    json+="\"message\":\""+esc(lastMessage)+"\"}";
    return json;
}
