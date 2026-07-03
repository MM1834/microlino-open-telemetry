// Copy this file to config.js and adapt it for your own deployment.
// Do not commit private hosts, usernames, passwords or tokens.

window.MOT_CONFIG = {
  app: {
    title: "Microlino Open Telemetry",
    vehicleName: "Microlino Pioneer",
    locale: "de-CH",
    units: "metric"
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

    reconnectPeriodMs: 3000,
    connectTimeoutMs: 10000
  },

  map: {
    enabled: true,
    defaultLat: 47.478,
    defaultLon: 8.212,
    defaultZoom: 13
  }
};
