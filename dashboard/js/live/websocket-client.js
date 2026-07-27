(function () {
  function create(options = {}) {
    const cfg = options.config || {};
    const getAccessToken = options.getAccessToken;
    const onState = options.onState || (() => {});
    const onMessage = options.onMessage || (() => {});
    const onError = options.onError || (() => {});

    let socket = null;
    let stopped = true;
    let reconnectTimer = null;
    let reconnectAttempt = 0;
    let activeVehicleId = null;
    let pingTimer = null;
    let consecutiveFailures = 0;
    let lastToken = '';

    const endpointUrl = () => String(cfg.websocketUrl || '').trim();
    const heartbeatMs = () => {
      const value = Number(cfg.heartbeatMs ?? 30000);
      return Number.isFinite(value) && value >= 10000 ? value : 30000;
    };
    const maxReconnectAttempts = () => {
      const value = Number(cfg.maxReconnectAttempts ?? 5);
      return Number.isInteger(value) && value >= 0 ? value : 5;
    };
    const reconnectDelays = () => {
      const configured = Array.isArray(cfg.reconnectDelaysMs)
        ? cfg.reconnectDelaysMs.map(Number).filter(v => Number.isFinite(v) && v >= 1000)
        : [];
      return configured.length ? configured : [5000, 15000, 60000, 120000, 300000];
    };

    function clearTimers() {
      if (reconnectTimer) window.clearTimeout(reconnectTimer);
      if (pingTimer) window.clearInterval(pingTimer);
      reconnectTimer = null;
      pingTimer = null;
    }

    function emitState(state, detail = '', extra = {}) {
      onState({ state, detail, ...extra });
    }

    function send(payload) {
      if (!socket || socket.readyState !== WebSocket.OPEN) return false;
      socket.send(JSON.stringify(payload));
      return true;
    }

    function subscribe(vehicleId) {
      activeVehicleId = vehicleId || null;
      if (activeVehicleId) {
        send({ action: 'subscribe', vehicleId: activeVehicleId });
      }
    }

    function scheduleReconnect() {
      if (stopped || reconnectTimer) return;
      if (consecutiveFailures >= maxReconnectAttempts()) {
        emitState('paused', 'Live-Verbindung nach wiederholten Fehlern pausiert', {
          failures: consecutiveFailures
        });
        return;
      }
      const delays = reconnectDelays();
      const index = Math.min(reconnectAttempt, delays.length - 1);
      const delayMs = delays[index];
      reconnectAttempt += 1;
      emitState('reconnecting', `Neuer Versuch in ${Math.round(delayMs / 1000)} s`, { delayMs });
      reconnectTimer = window.setTimeout(() => {
        reconnectTimer = null;
        connect();
      }, delayMs);
    }

    async function connect() {
      if (stopped || !endpointUrl()) return;
      if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;

      emitState('connecting', 'Live-Verbindung wird aufgebaut');

      try {
        const token = typeof getAccessToken === 'function' ? await getAccessToken() : '';
        if (!token) throw new Error('Access Token fehlt');
        if (lastToken && token !== lastToken) {
          reconnectAttempt = 0;
          consecutiveFailures = 0;
        }
        lastToken = token;

        const endpoint = new URL(endpointUrl());
        endpoint.searchParams.set('access_token', token);
        socket = new WebSocket(endpoint.toString());

        socket.addEventListener('open', () => {
          reconnectAttempt = 0;
          consecutiveFailures = 0;
          emitState('connected', 'WebSocket verbunden');
          if (activeVehicleId) subscribe(activeVehicleId);
          pingTimer = window.setInterval(() => send({ action: 'ping' }), heartbeatMs());
        });

        socket.addEventListener('message', event => {
          try {
            const message = JSON.parse(event.data);
            onMessage(message);
            if (message.type === 'subscribed') {
              emitState('connected', `Abonniert: ${message.vehicleId || activeVehicleId}`);
            } else if (message.type === 'pong') {
              emitState('connected', 'WebSocket verbunden');
            }
          } catch (error) {
            onError(new Error(`Ungültige WebSocket-Nachricht: ${error.message}`));
          }
        });

        socket.addEventListener('error', () => {
          onError(new Error('WebSocket-Verbindungsfehler'));
        });

        socket.addEventListener('close', event => {
          if (pingTimer) window.clearInterval(pingTimer);
          pingTimer = null;
          socket = null;
          if (!stopped) {
            consecutiveFailures += 1;
            emitState('disconnected', `Verbindung getrennt (${event.code})`, {
              code: event.code,
              failures: consecutiveFailures
            });
            scheduleReconnect();
          } else {
            emitState('disabled', 'Live-Verbindung beendet');
          }
        });
      } catch (error) {
        consecutiveFailures += 1;
        onError(error);
        emitState('disconnected', error.message || 'Live-Verbindung fehlgeschlagen');
        scheduleReconnect();
      }
    }

    return {
      start(vehicleId) {
        stopped = false;
        reconnectAttempt = 0;
        consecutiveFailures = 0;
        activeVehicleId = vehicleId || activeVehicleId;
        if (!endpointUrl()) {
          emitState('disabled', 'WebSocket URL nicht konfiguriert');
          return;
        }
        connect();
      },
      subscribe,
      stop() {
        stopped = true;
        clearTimers();
        if (socket) socket.close(1000, 'client stop');
        socket = null;
      },
      isConfigured() { return Boolean(endpointUrl()); },
      isConnected() { return socket?.readyState === WebSocket.OPEN; },
      describe() {
        return {
          websocketUrl: endpointUrl(),
          vehicleId: activeVehicleId,
          reconnectDelaysMs: reconnectDelays(),
          heartbeatMs: heartbeatMs(),
          maxReconnectAttempts: maxReconnectAttempts(),
          consecutiveFailures
        };
      }
    };
  }

  window.MOTLive = Object.assign(window.MOTLive || {}, { createWebSocketClient: create });
})();
