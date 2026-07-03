// Copy this file from config.example.js and adjust it for your installation.
window.MOT_CONFIG = {
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
  vehicle: {
    name: "Microlino Pioneer",
    image: "img/microlino.jpeg",
    defaultRangeKmAt100: 140
  },
  dashboard: {
    title: "MOT Dashboard",
    locale: "de-CH"
  }
};
