#pragma once

#include <Arduino.h>

struct ConfigurationReadinessInput {
    bool onboardingComplete = false;
    bool networkConfigured = false;
    bool networkOnline = false;
    bool canConfigured = false;
    bool canOnline = false;
    bool gpsDetected = false;
    bool gpsFix = false;
    bool mqttEnabled = false;
    bool mqttConfigured = false;
    bool mqttOnline = false;
    bool awsEnabled = false;
    bool awsConfigured = false;
    bool abrpEnabled = false;
    bool abrpConfigured = false;
    String gpsState = "GPS_DISABLED";
};

class ConfigurationReadiness {
public:
    static bool configured(const ConfigurationReadinessInput& input);
    static bool ready(const ConfigurationReadinessInput& input);
    static String toJson(const ConfigurationReadinessInput& input);
};
