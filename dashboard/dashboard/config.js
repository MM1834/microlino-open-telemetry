// MOT Dashboard configuration
// Edit this file before uploading to your web host.
window.MOT_CONFIG = {
  host: "mqtt.example.com",
  port: 22026,
  useTls: false,
  path: "/mqtt",

  username: "",
  password: "",

  topicPrefix: "mot",
  vehicleId: "pioneer",

  reconnectPeriodMs: 3000,
  connectTimeoutMs: 10000,

  // Set to true if your broker does not use a WebSocket path.
  omitPath: false
};
