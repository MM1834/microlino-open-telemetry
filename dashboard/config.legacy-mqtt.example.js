window.MOT_CONFIG = {
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

  awsBackend: {
    apiBaseUrl: "",
    pollingIntervalMs: 5000
  },

  vehicle: {
    name: "Microlino",
    image: "img/microlino.jpeg",
    defaultRangeKmAt100: 140,
    defaultLocation: {
      enabled: false,
      latitude: 0,
      longitude: 0,
      label: ""
    }
  },

  dashboard: {
    title: "MOT Dashboard",
    locale: "de-CH",
    vehicleOnlineSeconds: 120,
    vehicleStaleSeconds: 600
  }
};
