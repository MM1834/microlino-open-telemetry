#pragma once

#include <Arduino.h>

struct AwsIotCredentials {
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

bool loadAwsIotCredentials();
const AwsIotCredentials& awsIotCredentials();
String awsIotCredentialsStatusJson();
