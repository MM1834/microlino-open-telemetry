// Copy this file from config.example.js and adjust it for your installation.
window.MOT_CONFIG = {
  // Production Dashboard uses the AWS Vehicle REST API.
  dataSource: {
    type: "aws-backend"
  },
  mqtt: {
    // Compatibility values used only to derive the MOT topic namespace.
    topicPrefix: "mot",
    vehicleId: "your-default-vehicle"
  },
  // Target provider for the production WebApp.
  // The browser talks to an authenticated application backend, not directly
  // to AWS IoT Core with device credentials.
  awsBackend: {
    apiBaseUrl: "https://YOUR_API_ID.execute-api.eu-north-1.amazonaws.com",
    onboardingApiBaseUrl: "https://YOUR_ONBOARDING_API_ID.execute-api.eu-north-1.amazonaws.com",
    notificationApiBaseUrl: "https://YOUR_NOTIFICATION_API_ID.execute-api.eu-north-1.amazonaws.com",
    websocketUrl: "wss://YOUR_WEBSOCKET_API_ID.execute-api.eu-north-1.amazonaws.com/$default",

    // REST remains the data source in Phase 4A. WebSocket carries connection,
    // heartbeat and vehicle subscription control messages only.
    pollingIntervalMs: 30000,
    heartbeatMs: 30000,
    maxReconnectAttempts: 5,
    reconnectDelaysMs: [5000, 15000, 60000, 120000, 300000]
  },

  // Cognito User Pool Authorization Code flow with PKCE.
  // The app client must have no client secret and must allow the exact callback
  // and sign-out URL configured below.
  auth: {
    region: "eu-north-1",
    userPoolId: "eu-north-1_EXAMPLE",
    clientId: "YOUR_COGNITO_APP_CLIENT_ID",
    issuer: "https://cognito-idp.eu-north-1.amazonaws.com/eu-north-1_EXAMPLE",
    authorizeEndpoint: "https://YOUR_DOMAIN.auth.eu-north-1.amazoncognito.com/oauth2/authorize",
    tokenEndpoint: "https://YOUR_DOMAIN.auth.eu-north-1.amazoncognito.com/oauth2/token",
    logoutEndpoint: "https://YOUR_DOMAIN.auth.eu-north-1.amazoncognito.com/logout",
    redirectUri: "https://YOUR_DASHBOARD_URL/callback",
    logoutUri: "https://YOUR_DASHBOARD_URL/",
    scopes: ["openid", "email", "profile"],
    rememberDays: 30
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
    locationFreshnessMs: 60000

  }
};
