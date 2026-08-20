#pragma once

namespace ConfigKeys {

constexpr int SCHEMA_VERSION = 1;

constexpr char SCHEMA_VERSION_KEY[] = "schemaVersion";
constexpr char FIRMWARE[] = "firmware";
constexpr char BOARD[] = "board";
constexpr char SERVICES[] = "services";
constexpr char MQTT_SERVICE[] = "mqtt";
constexpr char AWS_SERVICE[] = "aws";
constexpr char ABRP_SERVICE[] = "abrp";
constexpr char GPS_ENABLED[] = "gpsEnabled";

constexpr char DEVICE_NAME[] = "deviceName";
constexpr char VEHICLE_NAME[] = "vehicleName";
constexpr char VEHICLE_ID[] = "vehicleId";
constexpr char MQTT_PREFIX[] = "mqttPrefix";

constexpr char WIFI_SSID[] = "wifiSsid";
constexpr char WIFI_PASS[] = "wifiPass";
constexpr char WIFI2_SSID[] = "wifi2Ssid";
constexpr char WIFI2_PASS[] = "wifi2Pass";
constexpr char LTE_APN[] = "lteApn";
constexpr char LTE_USER[] = "lteUser";
constexpr char LTE_PASS[] = "ltePass";

constexpr char MQTT_HOST[] = "mqttHost";
constexpr char MQTT_PORT[] = "mqttPort";
constexpr char MQTT_USER[] = "mqttUser";
constexpr char MQTT_PASS[] = "mqttPass";
constexpr char PUBLISH_INTERVAL_MS[] = "publishIntervalMs";

constexpr char ABRP_API_KEY[] = "abrpApiKey";
constexpr char ABRP_USER_TOKEN[] = "abrpUserToken";

constexpr char CAN1_PROFILE[] = "can1Profile";
constexpr char CAN2_PROFILE[] = "can2Profile";
constexpr char LEGACY_CAN_PROFILE[] = "canProfile";

constexpr char ONBOARDING_COMPLETE[] = "onboardingComplete";
constexpr char OTA_ENABLED[] = "otaEnabled";
constexpr char OTA_PASSWORD[] = "otaPassword";

} // namespace ConfigKeys
