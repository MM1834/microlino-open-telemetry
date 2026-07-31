#include "configuration_readiness.h"

#include <ArduinoJson.h>

bool ConfigurationReadiness::configured(const ConfigurationReadinessInput& input)
{
    if (!input.onboardingComplete || !input.networkConfigured || !input.canConfigured) return false;
    if (input.mqttEnabled && !input.mqttConfigured) return false;
    if (input.awsEnabled && !input.awsConfigured) return false;
    if (input.abrpEnabled && !input.abrpConfigured) return false;
    return true;
}

bool ConfigurationReadiness::ready(const ConfigurationReadinessInput& input)
{
    if (!configured(input) || !input.networkOnline || !input.canOnline) return false;
    if (input.mqttEnabled && !input.mqttOnline) return false;
    return true;
}

String ConfigurationReadiness::toJson(const ConfigurationReadinessInput& input)
{
    JsonDocument doc;
    doc["schemaVersion"] = 1;
    doc["configured"] = configured(input);
    doc["ready"] = ready(input);

    doc["checks"]["onboarding"]["required"] = true;
    doc["checks"]["onboarding"]["configured"] = input.onboardingComplete;

    doc["checks"]["network"]["required"] = true;
    doc["checks"]["network"]["configured"] = input.networkConfigured;
    doc["checks"]["network"]["online"] = input.networkOnline;

    doc["checks"]["can"]["required"] = true;
    doc["checks"]["can"]["configured"] = input.canConfigured;
    doc["checks"]["can"]["online"] = input.canOnline;

    doc["checks"]["gps"]["required"] = false;
    doc["checks"]["gps"]["detected"] = input.gpsDetected;
    doc["checks"]["gps"]["fix"] = input.gpsFix;
    doc["checks"]["gps"]["state"] = input.gpsState;

    doc["checks"]["mqtt"]["required"] = input.mqttEnabled;
    doc["checks"]["mqtt"]["enabled"] = input.mqttEnabled;
    doc["checks"]["mqtt"]["configured"] = input.mqttConfigured;
    doc["checks"]["mqtt"]["online"] = input.mqttOnline;

    doc["checks"]["aws"]["required"] = input.awsEnabled;
    doc["checks"]["aws"]["enabled"] = input.awsEnabled;
    doc["checks"]["aws"]["configured"] = input.awsConfigured;

    doc["checks"]["abrp"]["required"] = input.abrpEnabled;
    doc["checks"]["abrp"]["enabled"] = input.abrpEnabled;
    doc["checks"]["abrp"]["configured"] = input.abrpConfigured;

    String json;
    serializeJson(doc, json);
    return json;
}
