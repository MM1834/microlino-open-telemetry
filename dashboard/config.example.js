window.MOT_CONFIG = {
  // Current production target will become "aws-backend".
  // Keep "legacy-mqtt" while the authenticated backend is not deployed.
  dataSource: {
    type: "legacy-mqtt"
  },

  mqtt: {
    host: "mqtt.example.com",
    port: 443,
    useTls: true,
    path: "/",
    username: "",
    password: "",
    topicPrefix: "mot",
    vehicleId: "pioneer",
    clientIdPrefix: "mot-dashboard"
  },
  // Target provider for the production WebApp.
  // The browser talks to an authenticated application backend, not directly
  // to AWS IoT Core with device credentials.
  awsBackend: {
    apiBaseUrl: "",
    websocketUrl: "",
    reconnectPeriodMs: 5000

    // Authentication will be added with the user-management sprint.
    // getAccessToken: async () => "...",
    // tokenQueryParameter: "access_token"
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
    vehicleStaleSeconds: 600
  }
};
