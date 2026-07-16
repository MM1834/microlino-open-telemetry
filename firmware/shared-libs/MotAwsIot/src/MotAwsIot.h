#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

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
    String firmwareVersion;
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
    uint32_t heartbeatCount = 0;
    uint32_t birthCount = 0;
    uint32_t reconnectCount = 0;
    String message;
};

bool motLoadAwsCredentials(
    MotAwsCredentials& credentials,
    const char* basePath = "/aws"
);

class MotAwsIotClient {
public:
    MotAwsIotClient();

    bool begin(const MotAwsCredentials& credentials);
    void loop(const MotAwsRuntime& runtime, bool networkOnline);
    void disconnect();

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

    const MotAwsCredentials& credentials() const;
    const MotAwsStatus& status() const;

private:
    static constexpr time_t MIN_VALID_UTC = 1700000000;
    static constexpr uint32_t RECONNECT_INTERVAL_MS = 10000;
    static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;

    WiFiClientSecure secureClient_;
    PubSubClient mqtt_;
    MotAwsCredentials credentials_;
    MotAwsStatus status_;
    MotAwsRuntime runtime_;

    uint32_t lastReconnectAttemptMs_ = 0;
    uint32_t lastHeartbeatMs_ = 0;
    bool previousConnected_ = false;

    bool timeValid() const;
    bool connect();
    void publishBirth();
    void publishHeartbeat(bool force);
    static const char* resetReasonText();
};
