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
    locale: "de-CH"
  }
};
