#pragma once

#include <Arduino.h>

struct ConfigurationValidationResult {
    bool valid = true;
    String error;
};

class ConfigurationManager {
public:
    virtual ~ConfigurationManager() = default;

    virtual void load() = 0;
    virtual void save() = 0;
    virtual void clear() = 0;
    virtual String exportJson(bool includeSecrets) const = 0;
    virtual bool importJson(const String& json, String& error) = 0;
    virtual ConfigurationValidationResult validate() const = 0;

    static String normalizeIdentifier(String value, const String& fallback)
    {
        value.trim();
        value.toLowerCase();
        value.replace(" ", "-");
        value.replace("/", "-");
        while (value.indexOf("--") >= 0) value.replace("--", "-");
        while (value.startsWith("-")) value.remove(0, 1);
        while (value.endsWith("-")) value.remove(value.length() - 1);
        return value.isEmpty() ? fallback : value;
    }

    static String normalizeTopicPrefix(String value)
    {
        value.trim();
        while (value.startsWith("/")) value.remove(0, 1);
        while (value.endsWith("/")) value.remove(value.length() - 1);
        return value.isEmpty() ? String("mot") : value;
    }

    static uint16_t normalizePort(uint16_t value, uint16_t fallback = 1883)
    {
        return value == 0 ? fallback : value;
    }

    static uint32_t normalizePublishInterval(uint32_t value)
    {
        return value < 1000 ? 5000 : value;
    }
};
