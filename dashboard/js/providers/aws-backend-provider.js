(function () {
  window.MOTDataProviders.register('aws-backend', function (options) {
    const config = options.config || {};
    let socket = null;
    let stopped = false;
    let reconnectTimer = null;

    function vehicleId() {
      return config.vehicleId || 'pioneer';
    }

    function topicFor(key) {
      const prefix = (config.topicPrefix || 'mot').replace(/\/$/, '');
      return `${prefix}/${vehicleId()}/${key}`;
    }

    function emitSnapshot(snapshot, callbacks) {
      const values = snapshot?.values || snapshot?.telemetry || snapshot;
      if (!values || typeof values !== 'object') return;

      Object.entries(values).forEach(([key, value]) => {
        callbacks.onMessage(topicFor(key), JSON.stringify(value));
      });
    }

    async function loadSnapshot(callbacks) {
      if (!config.apiBaseUrl) return;

      const url =
        `${config.apiBaseUrl.replace(/\/$/, '')}` +
        `/api/vehicles/${encodeURIComponent(vehicleId())}/snapshot`;

      const headers = { Accept: 'application/json' };
      if (typeof config.getAccessToken === 'function') {
        const token = await config.getAccessToken();
        if (token) headers.Authorization = `Bearer ${token}`;
      }

      const response = await fetch(url, { headers });
      if (!response.ok) {
        throw new Error(`Snapshot request failed: HTTP ${response.status}`);
      }

      emitSnapshot(await response.json(), callbacks);
    }

    async function connectSocket(callbacks) {
      if (!config.websocketUrl || stopped) {
        callbacks.onConnection(
          false,
          'AWS Backend Provider ist noch nicht konfiguriert'
        );
        return;
      }

      let url = config.websocketUrl
        .replace('{vehicleId}', encodeURIComponent(vehicleId()));

      if (typeof config.getAccessToken === 'function') {
        const token = await config.getAccessToken();
        if (token && config.tokenQueryParameter) {
          const separator = url.includes('?') ? '&' : '?';
          url += `${separator}${encodeURIComponent(config.tokenQueryParameter)}` +
            `=${encodeURIComponent(token)}`;
        }
      }

      callbacks.onConnection(false, 'AWS Backend wird verbunden…');
      socket = new WebSocket(url);

      socket.addEventListener('open', async () => {
        callbacks.onConnection(true, 'Verbunden mit AWS Backend');
        try {
          await loadSnapshot(callbacks);
        } catch (error) {
          callbacks.onError(error);
        }
      });

      socket.addEventListener('message', event => {
        try {
          const message = JSON.parse(event.data);

          // Preferred backend contract:
          // { "topic": "mot/pioneer/system/...", "payload": ... }
          if (message.topic) {
            const payload = typeof message.payload === 'string'
              ? message.payload
              : JSON.stringify(message.payload);
            callbacks.onMessage(message.topic, payload);
            return;
          }

          // Snapshot/update contract:
          // { "key": "system/last_seen_utc", "value": ... }
          if (message.key) {
            callbacks.onMessage(
              topicFor(message.key),
              JSON.stringify(message.value)
            );
          }
        } catch (error) {
          callbacks.onError(error);
        }
      });

      socket.addEventListener('close', () => {
        callbacks.onConnection(false, 'AWS Backend getrennt');
        if (!stopped) {
          reconnectTimer = window.setTimeout(
            () => connectSocket(callbacks),
            Number(config.reconnectPeriodMs ?? 5000)
          );
        }
      });

      socket.addEventListener('error', () => {
        callbacks.onConnection(false, 'AWS Backend Fehler');
      });
    }

    return {
      name: 'aws-backend',

      start(callbacks) {
        stopped = false;
        console.info('MOT data provider: aws-backend');
        connectSocket(callbacks).catch(callbacks.onError);
      },

      stop() {
        stopped = true;
        if (reconnectTimer) window.clearTimeout(reconnectTimer);
        reconnectTimer = null;
        if (socket) socket.close();
        socket = null;
      },

      describe() {
        return {
          type: 'aws-backend',
          apiBaseUrl: config.apiBaseUrl,
          websocketUrl: config.websocketUrl,
          vehicleId: vehicleId()
        };
      }
    };
  });
})();
