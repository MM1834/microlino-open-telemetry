// Microlino Open Telemetry Dashboard configuration example
// Copy this file to config.js and adapt it for your MQTT WebSocket endpoint.
window.MOT_CONFIG = {
  // MQTT over WebSocket endpoint.
  // Use WSS when the dashboard is served over HTTPS.
  host: "mqtt.example.com",
  port: 443,
  useTls: true,
  path: "/",

  // Optional MQTT credentials.
  username: "",
  password: "",

  // MQTT topic base: <topicPrefix>/<vehicleId>/...
  topicPrefix: "mot",
  vehicleId: "microlino",

  // UI only.
  vehicleName: "Microlino",

  // Range estimate fallback used when no range topic is available.
  rangeFullKm: 140,

  // Default map position shown until location topics are received.
  map: {
    defaultLat: 47.3769,
    defaultLng: 8.5417,
    zoom: 13
  }
};
