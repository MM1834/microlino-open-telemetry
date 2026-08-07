// Public browser configuration for the controlled MOT beta portal.
// Copy this file to config.js in the uploaded motbeta directory.
window.MOT_CONFIG = {
  dataSource: { type: "aws-backend" },
  mqtt: { topicPrefix: "mot", vehicleId: "pioneer" },
  awsBackend: {
    apiBaseUrl: "https://yaugi9zu8l.execute-api.eu-north-1.amazonaws.com",
    onboardingApiBaseUrl: "https://3izicgmdxi.execute-api.eu-north-1.amazonaws.com",
    notificationApiBaseUrl: "https://llojj35e6e.execute-api.eu-north-1.amazonaws.com",
    websocketUrl: "wss://pvmzhwb5c1.execute-api.eu-north-1.amazonaws.com/$default",
    pollingIntervalMs: 30000,
    heartbeatMs: 30000,
    maxReconnectAttempts: 5,
    reconnectDelaysMs: [5000, 15000, 60000, 120000, 300000]
  },
  auth: {
    region: "eu-north-1",
    userPoolId: "eu-north-1_vbMnyGtc0",
    clientId: "2ekjdine65i98tq6hhonce2gmq",
    issuer: "https://cognito-idp.eu-north-1.amazonaws.com/eu-north-1_vbMnyGtc0",
    authorizeEndpoint: "https://mot-dev-002581114110-eu-north-1.auth.eu-north-1.amazoncognito.com/oauth2/authorize",
    tokenEndpoint: "https://mot-dev-002581114110-eu-north-1.auth.eu-north-1.amazoncognito.com/oauth2/token",
    logoutEndpoint: "https://mot-dev-002581114110-eu-north-1.auth.eu-north-1.amazoncognito.com/logout",
    redirectUri: "https://www.microlino-open-telemetry.ch/motbeta/callback/",
    logoutUri: "https://www.microlino-open-telemetry.ch/motbeta/",
    scopes: ["openid", "email", "profile"]
  },
  vehicle: {
    name: "Microlino Pioneer",
    image: "img/microlino.jpeg",
    defaultRangeKmAt100: 140,
    defaultLocation: {
      enabled: true,
      latitude: 47.46198,
      longitude: 8.11068,
      label: "Default location"
    }
  },
  dashboard: {
    title: "MOT Beta",
    locale: "de-CH",
    vehicleOnlineSeconds: 120,
    vehicleStaleSeconds: 600,
    locationFreshnessMs: 60000
  }
};
