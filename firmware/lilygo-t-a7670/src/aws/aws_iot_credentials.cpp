#include "aws_iot_credentials.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

static AwsIotCredentials credentials;

static bool readTextFile(const char* path, String& value)
{
    File file = LittleFS.open(path, "r");
    if (!file) {
        credentials.message = String("AWS credential file missing: ") + path;
        return false;
    }

    value = file.readString();
    file.close();

    value.trim();
    if (value.isEmpty()) {
        credentials.message = String("AWS credential file empty: ") + path;
        return false;
    }
    return true;
}

bool loadAwsIotCredentials()
{
    credentials = AwsIotCredentials();

    if (!LittleFS.begin(false)) {
        credentials.message = "LittleFS mount failed";
        return false;
    }

    String deviceJson;
    if (!readTextFile("/aws/device.json", deviceJson)) return false;
    if (!readTextFile("/aws/AmazonRootCA1.pem", credentials.rootCa)) return false;
    if (!readTextFile("/aws/device-certificate.pem.crt", credentials.certificate)) return false;
    if (!readTextFile("/aws/device-private-key.pem.key", credentials.privateKey)) return false;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, deviceJson);
    if (error) {
        credentials.message = String("AWS device.json invalid: ") + error.c_str();
        return false;
    }

    credentials.endpoint = doc["endpoint"] | "";
    credentials.port = doc["port"] | 8883;
    credentials.thingName = doc["thingName"] | "";
    credentials.vehicleId = doc["vehicleId"] | "";
    credentials.topicPrefix = doc["topicPrefix"] | "mot";

    credentials.endpoint.trim();
    credentials.thingName.trim();
    credentials.vehicleId.trim();
    credentials.topicPrefix.trim();

    if (credentials.endpoint.isEmpty() ||
        credentials.thingName.isEmpty() ||
        credentials.vehicleId.isEmpty()) {
        credentials.message = "AWS device.json missing endpoint, thingName or vehicleId";
        return false;
    }

    credentials.loaded = true;
    credentials.message = "AWS IoT credentials loaded from LittleFS";
    return true;
}

const AwsIotCredentials& awsIotCredentials()
{
    return credentials;
}

static String esc(String value)
{
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\r", "\\r");
    value.replace("\n", "\\n");
    return value;
}

String awsIotCredentialsStatusJson()
{
    String json = "{";
    json += "\"loaded\":" + String(credentials.loaded ? "true" : "false") + ",";
    json += "\"endpoint\":\"" + esc(credentials.endpoint) + "\",";
    json += "\"port\":" + String(credentials.port) + ",";
    json += "\"thingName\":\"" + esc(credentials.thingName) + "\",";
    json += "\"vehicleId\":\"" + esc(credentials.vehicleId) + "\",";
    json += "\"topicPrefix\":\"" + esc(credentials.topicPrefix) + "\",";
    json += "\"message\":\"" + esc(credentials.message) + "\"";
    json += "}";
    return json;
}
