// Copy this file from config.example.js and adjust it for your installation.
window.MOT_CONFIG = {
  // Production Dashboard uses the AWS Vehicle REST API.
  dataSource: {
    type: "aws-backend"
  },
  mqtt: {
    // Compatibility values used only to derive the MOT topic namespace.
    topicPrefix: "mot",
    vehicleId: "pioneer"
  },
  // Target provider for the production WebApp.
  // The browser talks to an authenticated application backend, not directly
  // to AWS IoT Core with device credentials.
  awsBackend: {
    apiBaseUrl: "https://yaugi9zu8l.execute-api.eu-north-1.amazonaws.com",
    pollingIntervalMs: 5000
  },

  vehicle: {
    name: "Microlino Pioneer",
    image: "img/microlino.jpeg",
    defaultRangeKmAt100: 140,

    // Used by the dashboard until real GPS/location MQTT topics are available.
    // Set enabled=false when you only want to show live GPS data from MQTT.
    defaultLocation: {
      enabled: true,
      latitude: 47.46198,
      longitude: 8.11068,
      label: "Default location"
    }
  },
  dashboard: {
    title: "MOT Dashboard",
    locale: "de-CH",

    // Microlino / OBD2 presence thresholds.
    // 0–120 seconds: Online
    // 121–600 seconds: Data stale
    // More than 600 seconds: Offline
    vehicleOnlineSeconds: 120,
    vehicleStaleSeconds: 600,


    // A newer coordinate pair is shown as "Aktueller Standort".
    // Older retained coordinates remain visible as "Letzter Standort".
    locationFreshnessMs: 60000
  }
};
