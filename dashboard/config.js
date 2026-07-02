// MOT Dashboard configuration
// Copy this folder to any static web host and adjust these values.
window.MOT_CONFIG = {
  host: "your-mqtt-host.example.com",
  port: 22026,
  useTls: false,
  username: "",
  password: "",
  topicPrefix: "mot",
  vehicleId: "pioneer",
  reconnectPeriodMs: 3000,
  connectTimeoutMs: 8000,
  staleAfterMs: 15000
};
