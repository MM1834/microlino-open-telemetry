(function () {
  window.MOTDataProviders.register('aws-backend', function (options) {
    const config = options.config || {};
    let stopped = false;
    let timer = null;
    let callbacksRef = null;
    let activeVehicleId = config.vehicleId || 'pioneer';
    let liveClient = null;
    let pollInFlight = false;

    const baseUrl = () => String(config.apiBaseUrl || '').replace(/\/$/, '');
    const onboardingBaseUrl = () => String(config.onboardingApiBaseUrl || '').replace(/\/$/, '');
    const interval = () => {
      const value = Number(config.pollingIntervalMs ?? 5000);
      return Number.isFinite(value) && value >= 1000 ? value : 5000;
    };

    async function headers() {
      const result = { Accept: 'application/json' };
      if (typeof config.getAccessToken === 'function') {
        const token = await config.getAccessToken();
        if (token) result.Authorization = `Bearer ${token}`;
      }
      return result;
    }

    async function get(path) {
      const response = await fetch(`${baseUrl()}${path}`, {
        headers: await headers(),
        cache: 'no-store'
      });
      if (!response.ok) {
        const error = new Error(`AWS API HTTP ${response.status}`);
        error.status = response.status;
        error.path = path;
        if (response.status === 401) config.onUnauthorized?.(error);
        throw error;
      }
      return response.json();
    }

    async function post(path, body) {
      if (!onboardingBaseUrl()) throw new Error('Onboarding API URL fehlt');
      const response = await fetch(`${onboardingBaseUrl()}${path}`, {
        method: 'POST',
        headers: { ...(await headers()), 'content-type': 'application/json' },
        cache: 'no-store',
        body: JSON.stringify(body)
      });
      if (!response.ok) {
        const conflictMessage = path.endsWith('/claims')
          ? 'Fahrzeug ist bereits zugewiesen oder nicht verfügbar'
          : 'Claim ungültig oder nicht mehr verfügbar';
        const error = new Error(response.status === 409
          ? conflictMessage
          : `Onboarding API HTTP ${response.status}`);
        error.status = response.status;
        if (response.status === 401) config.onUnauthorized?.(error);
        throw error;
      }
      return response.json();
    }

    function emit(snapshot, callbacks) {
      const vehicleId = snapshot.vehicleId || activeVehicleId;
      const prefix = String(config.topicPrefix || 'mot').replace(/\/$/, '');
      Object.entries(snapshot.values || {}).forEach(([key, value]) => {
        const payload = typeof value === 'string' ? value : JSON.stringify(value);
        callbacks.onMessage(`${prefix}/${vehicleId}/${key}`, payload);
      });
      callbacks.onSnapshot?.(snapshot);
    }

    async function poll(callbacks) {
      if (stopped || pollInFlight || document.hidden || !activeVehicleId) return;
      pollInFlight = true;
      const requestedVehicleId = activeVehicleId;
      try {
        const snapshot = await get(`/api/vehicles/${encodeURIComponent(requestedVehicleId)}/snapshot`);
        if (stopped || requestedVehicleId !== activeVehicleId) return;
        callbacks.onConnection(true, 'Verbunden mit AWS Vehicle API');
        emit(snapshot, callbacks);
      } catch (error) {
        if (requestedVehicleId === activeVehicleId) {
          callbacks.onConnection(false, error.message || 'AWS API Fehler');
          callbacks.onError(error);
        }
      } finally {
        pollInFlight = false;
      }
    }

    function startLive(callbacks) {
      if (!activeVehicleId || liveClient) return;
      if (!window.MOTLive?.createWebSocketClient) {
        callbacks.onLiveConnection?.({ state: 'disabled', detail: 'WebSocket Client fehlt' });
        return;
      }
      liveClient = window.MOTLive.createWebSocketClient({
        config,
        getAccessToken: config.getAccessToken,
        onState: status => callbacks.onLiveConnection?.(status),
        onMessage: message => {
          if (message?.type === 'telemetry') {
            const vehicleId = String(message.vehicleId || '');
            const topicSuffix = String(message.topicSuffix || '');
            if (!vehicleId || !topicSuffix || vehicleId !== activeVehicleId) return;
            const prefix = String(config.topicPrefix || 'mot').replace(/\/$/, '');
            const payload = typeof message.value === 'string'
              ? message.value
              : JSON.stringify(message.value);
            callbacks.onMessage(`${prefix}/${vehicleId}/${topicSuffix}`, payload);
            return;
          }
          callbacks.onLiveMessage?.(message);
        },
        onError: error => callbacks.onError(error)
      });
      liveClient.start(activeVehicleId);
    }

    async function syncVehicles(callbacks) {
      const result = await get('/api/vehicles');
      const vehicles = Array.isArray(result) ? result : (result.vehicles || []);
      const previousVehicleId = activeVehicleId;
      const exists = vehicles.some(v => v.vehicleId === activeVehicleId);
      if (!exists) activeVehicleId = vehicles[0]?.vehicleId || null;
      callbacks.onVehicles?.(vehicles);

      if (!activeVehicleId) {
        liveClient?.stop();
        liveClient = null;
        callbacks.onLiveConnection?.({
          state: 'disabled',
          detail: 'Keine aktive Fahrzeugzuordnung'
        });
        callbacks.onOnboardingRequired?.(true);
      } else if (!liveClient) {
        callbacks.onOnboardingRequired?.(false);
        startLive(callbacks);
      } else if (activeVehicleId !== previousVehicleId) {
        callbacks.onOnboardingRequired?.(false);
        liveClient.subscribe(activeVehicleId);
      } else {
        callbacks.onOnboardingRequired?.(false);
      }
      return Boolean(activeVehicleId);
    }

    async function refresh(callbacks) {
      if (stopped || document.hidden) return;
      try {
        const hasVehicle = await syncVehicles(callbacks);
        if (hasVehicle) await poll(callbacks);
      } catch (error) {
        callbacks.onConnection(false, error.message || 'AWS API Fehler');
        callbacks.onError(error);
      }
    }

    function handleVisibilityChange() {
      if (!document.hidden && callbacksRef && !stopped) poll(callbacksRef);
    }

    return {
      name: 'aws-backend',

      async start(callbacks) {
        callbacksRef = callbacks;
        stopped = false;
        if (!baseUrl()) {
          callbacks.onConnection(false, 'AWS API URL fehlt');
          return;
        }

        try {
          await refresh(callbacks);
          timer = window.setInterval(() => refresh(callbacks), interval());
          document.addEventListener('visibilitychange', handleVisibilityChange);
        } catch (error) {
          callbacks.onConnection(false, error.message || 'AWS API Fehler');
          callbacks.onError(error);
        }
      },

      async selectVehicle(vehicleId) {
        activeVehicleId = vehicleId;
        liveClient?.subscribe(vehicleId);
        if (callbacksRef) await poll(callbacksRef);
      },

      getSelectedVehicleId() { return activeVehicleId; },

      async claimVehicle(claim) {
        if (!callbacksRef) throw new Error('Dashboard ist noch nicht bereit');
        const result = await post('/api/onboarding/claim', { claim });
        await refresh(callbacksRef);
        return result;
      },

      async issueClaim(vehicleId) {
        return post('/api/onboarding/claims', { vehicleId });
      },

      stop() {
        stopped = true;
        callbacksRef = null;
        liveClient?.stop();
        liveClient = null;
        document.removeEventListener('visibilitychange', handleVisibilityChange);
        if (timer) window.clearInterval(timer);
        timer = null;
      },

      describe() {
        return {
          type: 'aws-backend',
          apiBaseUrl: baseUrl(),
          onboardingApiBaseUrl: onboardingBaseUrl(),
          websocketUrl: String(config.websocketUrl || ''),
          vehicleId: activeVehicleId,
          pollingIntervalMs: interval(),
          live: liveClient?.describe?.() || null
        };
      }
    };
  }, { capabilities: { live: true, write: false, history: true, authentication: true, multiVehicle: true } });
})();
