#pragma once

#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "network/resolved_network_client_secure.h"

struct MotAwsCredentials {
    bool loaded = false;
    String endpoint;
    uint16_t port = 8883;
    String thingName;
    String vehicleId;
    String topicPrefix = "mot";
    String rootCa;
    String certificate;
    String privateKey;
    String message;
};

struct MotAwsRuntime {
    String deviceId;
    String deviceName;
    String board;
    String firmwareVersion;
    String can1Profile;
    String can2Profile;
    String networkMode;
    String transport = "WiFi";
    String ipAddress;
    int wifiRssi = 0;
    uint32_t uptimeSec = 0;
};

struct MotAwsStatus {
    bool credentialsLoaded = false;
    bool connected = false;
    bool timeValid = false;
    int mqttState = MQTT_DISCONNECTED;
    uint32_t connectAttempts = 0;
    uint32_t publishCount = 0;
    uint64_t payloadBytes = 0;
    uint64_t mqttPublishBytes = 0;
    uint32_t heartbeatCount = 0;
    uint32_t birthCount = 0;
    uint32_t reconnectCount = 0;
    uint32_t disconnectCount = 0;
    uint32_t consecutiveConnectFailures = 0;
    uint32_t totalConnectFailures = 0;
    uint32_t lastConnectDurationMs = 0;
    uint32_t reconnectDelayMs = 0;
    int tlsErrorCode = 0;
    String tlsError;
    String message;
};

using MotAwsMessageCallback = void (*)(char*, uint8_t*, unsigned int);

bool motLoadAwsCredentials(
    MotAwsCredentials& credentials,
    const char* basePath = "/aws"
);

class MotAwsIotClient {
public:
    MotAwsIotClient();

    bool begin(const MotAwsCredentials& credentials);
    bool begin(const MotAwsCredentials& credentials, Client& transportClient);
    void loop(const MotAwsRuntime& runtime, bool networkOnline);
    void disconnect();
    void setMinimalBirth(bool minimal);
    bool resolveEndpointOnLoopTask();
    bool endpointResolutionReady() const;

    bool connected();
    bool enabled() const;
    String topic(const char* suffix) const;

    bool publish(
        const char* suffix,
        const String& payload,
        bool retained = true
    );
    bool publishInt(
        const char* suffix,
        long value,
        bool retained = true
    );
    bool publishFloat(
        const char* suffix,
        float value,
        int decimals = 1,
        bool retained = true
    );
    bool publishBool(
        const char* suffix,
        bool value,
        bool retained = true
    );
    bool publishLastSeenUtc();
    void setMessageCallback(MotAwsMessageCallback callback);
    bool subscribe(const char* suffix, uint8_t qos = 1);

    const MotAwsCredentials& credentials() const;
    const MotAwsStatus& status() const;

private:
    static constexpr time_t MIN_VALID_UTC = 1700000000;
    static constexpr uint32_t RECONNECT_INTERVAL_MS = 10000;
    static constexpr uint32_t MAX_RECONNECT_INTERVAL_MS = 300000;
    static constexpr uint32_t STABLE_CONNECTION_MS = 60000;
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;

    ResolvedNetworkClientSecure secureClient_;
    PubSubClient mqtt_;
    MotAwsCredentials credentials_;
    MotAwsStatus status_;
    MotAwsRuntime runtime_;

    uint32_t lastReconnectAttemptMs_ = 0;
    uint32_t reconnectDelayMs_ = RECONNECT_INTERVAL_MS;
    uint32_t lastHeartbeatMs_ = 0;
    uint32_t connectedSinceMs_ = 0;
    bool previousConnected_ = false;
    bool birthPublished_ = false;
    bool minimalBirth_ = false;

    bool timeValid() const;
    bool configure(const MotAwsCredentials& credentials, Client& transportClient);
    bool connect();
    void publishBirth();
    void publishHeartbeat(bool force);
    static const char* resetReasonText();
};
