(function () {
  function sanitizeClientIdPart(value) {
    return String(value || '')
      .trim()
      .toLowerCase()
      .replace(/[^a-z0-9_-]+/g, '-')
      .replace(/^-+|-+$/g, '')
      .slice(0, 32);
  }

  function createClientIdSuffix() {
    try {
      if (window.crypto?.randomUUID) {
        return window.crypto.randomUUID().replace(/-/g, '').slice(0, 12);
      }

      if (window.crypto?.getRandomValues) {
        const bytes = new Uint8Array(6);
        window.crypto.getRandomValues(bytes);
        return Array.from(bytes, byte =>
          byte.toString(16).padStart(2, '0')
        ).join('');
      }
    } catch (error) {
      console.warn('Could not use crypto for MQTT client ID:', error);
    }

    return `${Date.now().toString(36)}${Math.random().toString(36).slice(2, 8)}`
      .slice(0, 12);
  }

  function getPersistentClientId(config) {
    const configuredId = sanitizeClientIdPart(config.clientId);
    if (configuredId) return configuredId;

    const prefix =
      sanitizeClientIdPart(config.clientIdPrefix) || 'mot-dashboard';
    const vehicle =
      sanitizeClientIdPart(config.vehicleId || 'vehicle');
    const broker =
      sanitizeClientIdPart(config.host || 'broker');
    const storageKey =
      config.clientIdStorageKey ||
      `mot.mqttClientId.${broker}.${vehicle}`;

    try {
      const stored = window.localStorage.getItem(storageKey);
      if (stored) return stored;

      const generated = `${prefix}-${createClientIdSuffix()}`;
      window.localStorage.setItem(storageKey, generated);
      return generated;
    } catch (error) {
      console.warn('localStorage unavailable for MQTT client ID:', error);
      return `${prefix}-${createClientIdSuffix()}`;
    }
  }

  window.MOTDataProviders.register('legacy-mqtt', function (options) {
    const config = options.config || {};
    let client = null;

    function baseTopic() {
      const prefix = (config.topicPrefix || 'mot').replace(/\/$/, '');
      return `${prefix}/${config.vehicleId || 'pioneer'}`;
    }

    return {
      name: 'legacy-mqtt',

      start(callbacks) {
        const mqttLib = window.mqtt || window.MQTT || window.Mqtt;
        if (!mqttLib || typeof mqttLib.connect !== 'function') {
          callbacks.onConnection(false, 'mqtt.min.js fehlt oder ist ungültig');
          return;
        }

        const protocol = config.useTls ? 'wss' : 'ws';
        const path = config.path || '/';
        const url = `${protocol}://${config.host}:${config.port}${path}`;
        const clientId = getPersistentClientId(config);

        callbacks.onConnection(false, 'Connecting…');
        console.info('MOT data provider: legacy-mqtt');
        console.info('MQTT client ID:', clientId);

        client = mqttLib.connect(url, {
          username: config.username || undefined,
          password: config.password || undefined,
          clientId,
          reconnectPeriod: Number(config.reconnectPeriodMs ?? 2500),
          connectTimeout: Number(config.connectTimeoutMs ?? 15000)
        });

        client.on('connect', () => {
          callbacks.onConnection(true, 'Verbunden mit MQTT');
          client.subscribe(`${baseTopic()}/#`, error => {
            if (error) callbacks.onError(error);
          });
        });

        client.on('reconnect', () =>
          callbacks.onConnection(false, 'Reconnect…'));
        client.on('offline', () =>
          callbacks.onConnection(false, 'Offline'));
        client.on('close', () =>
          callbacks.onConnection(false, 'Verbindung geschlossen'));
        client.on('error', error => {
          callbacks.onConnection(false, error?.message || 'MQTT Fehler');
          callbacks.onError(error);
        });
        client.on('message', (topic, payload) =>
          callbacks.onMessage(topic, payload));
      },

      stop() {
        if (client) {
          client.end(true);
          client = null;
        }
      },

      describe() {
        return {
          type: 'legacy-mqtt',
          host: config.host,
          vehicleId: config.vehicleId,
          topic: `${baseTopic()}/#`
        };
      }
    };
  });
})();
