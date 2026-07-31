(function () {
  window.MOTDataProviders.register('aws-backend', function (options) {
    const config = options.config || {};
    let stopped = false;
    let timer = null;
    let callbacksRef = null;
    let activeVehicleId = config.vehicleId || 'pioneer';

    const baseUrl = () => String(config.apiBaseUrl || '').replace(/\/$/, '');
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

    function emit(snapshot, callbacks) {
      const vehicleId = snapshot.vehicleId || activeVehicleId;
      const prefix = String(config.topicPrefix || 'mot').replace(/\/$/, '');
      Object.entries(snapshot.values || {}).forEach(([key, value]) => {
        const payload = typeof value === 'string'
          ? value
          : JSON.stringify(value);
        callbacks.onMessage(`${prefix}/${vehicleId}/${key}`, payload);
      });
      callbacks.onSnapshot?.(snapshot);
    }

    async function poll(callbacks) {
      if (stopped) return;

      const requestedVehicleId = activeVehicleId;

      try {
        const snapshot = await get(
          `/api/vehicles/${encodeURIComponent(requestedVehicleId)}/snapshot`
        );

        if (stopped || requestedVehicleId !== activeVehicleId) return;

        callbacks.onConnection(true, 'Verbunden mit AWS Vehicle API');
        emit(snapshot, callbacks);
      } catch (error) {
        if (requestedVehicleId !== activeVehicleId) return;
        callbacks.onConnection(false, error.message || 'AWS API Fehler');
        callbacks.onError(error);
      }
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
          const result = await get('/api/vehicles');
          const vehicles = Array.isArray(result)
            ? result
            : (result.vehicles || []);

          const exists = vehicles.some(v => v.vehicleId === activeVehicleId);
          if (!exists && vehicles.length) activeVehicleId = vehicles[0].vehicleId;

          callbacks.onVehicles?.(vehicles);
          await poll(callbacks);
          timer = window.setInterval(() => poll(callbacks), interval());
        } catch (error) {
          callbacks.onConnection(false, error.message || 'AWS API Fehler');
          callbacks.onError(error);
        }
      },

      async selectVehicle(vehicleId) {
        activeVehicleId = vehicleId;
        if (callbacksRef) await poll(callbacksRef);
      },

      getSelectedVehicleId() {
        return activeVehicleId;
      },

      stop() {
        stopped = true;
        callbacksRef = null;
        if (timer) window.clearInterval(timer);
        timer = null;
      },

      describe() {
        return {
          type: 'aws-backend',
          apiBaseUrl: baseUrl(),
          vehicleId: activeVehicleId,
          pollingIntervalMs: interval()
        };
      }
    };
  }, { capabilities: { live: true, write: false, history: true, authentication: true, multiVehicle: true } });
})();
