(function () {
  const cfg = window.MOT_CONFIG || {};
  const mqttCfg = cfg.mqtt || {};
  const vehicleCfg = cfg.vehicle || {};
  const dashboardCfg = cfg.dashboard || {};
  const dataSourceCfg = cfg.dataSource || { type: 'legacy-mqtt' };
  const auth = dataSourceCfg.type === 'aws-backend' && window.MOTAuth
    ? window.MOTAuth.create({ config: cfg.auth || {} })
    : null;
  const $ = (id) => document.getElementById(id);
  const state = {
    lastMessage: 0,
    values: {},
    metadata: {},
    mqttConnected: false,
    mqttDetail: '',
    networkMode: '--',
    deviceIp: '--',
    vehicleLastSeenMs: 0,
    vehicleLastSeenSource: '',
    availableVehicles: [],
    selectedVehicleId: mqttCfg.vehicleId || 'pioneer',
    authBusy: false
  };


  function renderAuthState(message = '') {
    const container = $('auth-controls');
    const loginButton = $('auth-login');
    const logoutButton = $('auth-logout');
    const status = $('auth-status');
    if (!container || !auth) return;

    container.hidden = false;
    const authenticated = auth.isAuthenticated();
    loginButton.hidden = authenticated;
    logoutButton.hidden = !authenticated;
    loginButton.disabled = state.authBusy || !auth.isConfigured();
    logoutButton.disabled = state.authBusy;

    if (message) status.textContent = message;
    else if (!auth.isConfigured()) status.textContent = 'Cognito nicht konfiguriert';
    else status.textContent = authenticated ? 'Angemeldet' : 'Nicht angemeldet';
  }

  async function beginLogin() {
    if (!auth || state.authBusy) return;
    state.authBusy = true;
    renderAuthState('Weiterleitung…');
    try { await auth.login(); }
    catch (error) {
      state.authBusy = false;
      console.error('MOT login failed:', error);
      renderAuthState(error.message || 'Login fehlgeschlagen');
    }
  }

  async function beginLogout() {
    if (!auth || state.authBusy) return;
    state.authBusy = true;
    renderAuthState('Abmeldung…');
    try { await auth.logout(); }
    catch (error) {
      state.authBusy = false;
      console.error('MOT logout failed:', error);
      renderAuthState(error.message || 'Logout fehlgeschlagen');
    }
  }

  function configuredSeconds(value, fallback) {
    const seconds = Number(value ?? fallback);
    return Number.isFinite(seconds) && seconds >= 0 ? seconds : fallback;
  }

  const VEHICLE_ONLINE_MS =
    configuredSeconds(dashboardCfg.vehicleOnlineSeconds, 120) * 1000;
  const LOCATION_CURRENT_MS = (() => {
    const milliseconds = Number(dashboardCfg.locationFreshnessMs ?? 60000);
    return Number.isFinite(milliseconds) && milliseconds >= 0
      ? milliseconds
      : 60000;
  })();
  const VEHICLE_STALE_MS =
    Math.max(
      configuredSeconds(dashboardCfg.vehicleStaleSeconds, 600) * 1000,
      VEHICLE_ONLINE_MS
    );

  function setText(id, value) { const el = $(id); if (el) el.textContent = value; }
  function fmtNum(v, digits = 0) { const n = Number(v); return Number.isFinite(n) ? n.toFixed(digits) : '--'; }
  function fmtCoord(v, positiveSuffix, negativeSuffix) {
    const n = Number(v);
    if (!Number.isFinite(n)) return '--';
    const suffix = n >= 0 ? positiveSuffix : negativeSuffix;
    return `${Math.abs(n).toFixed(5)}° ${suffix}`;
  }
  function baseTopic(vehicleId = state.selectedVehicleId) {
    const prefix = (mqttCfg.topicPrefix || 'mot').replace(/\/$/, '');
    const vehicle = vehicleId || mqttCfg.vehicleId || 'pioneer';
    return `${prefix}/${vehicle}`;
  }
  function isUsableIp(value) {
    const ip = String(value || '').trim();
    return ip !== '' && ip !== '--' && ip !== '0.0.0.0';
  }

  function parseTimestampMs(value) {
    if (value === null || value === undefined) return 0;

    if (typeof value === 'number' && Number.isFinite(value)) {
      if (value >= 1e12) return value;
      if (value >= 1e9) return value * 1000;
      return 0;
    }

    const raw = String(value).trim();
    if (!raw) return 0;

    if (/^\d{13}$/.test(raw)) return Number(raw);
    if (/^\d{10}$/.test(raw)) return Number(raw) * 1000;

    const parsed = Date.parse(raw);
    return Number.isNaN(parsed) ? 0 : parsed;
  }

  function relativeTime(timestampMs) {
    if (!timestampMs) return 'Noch kein Update empfangen';

    const seconds = Math.max(0, Math.floor((Date.now() - timestampMs) / 1000));
    if (seconds < 5) return 'gerade eben';
    if (seconds < 60) return `vor ${seconds} s`;

    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `vor ${minutes} min`;

    const hours = Math.floor(minutes / 60);
    if (hours < 48) return `vor ${hours} h`;

    const days = Math.floor(hours / 24);
    return `vor ${days} Tagen`;
  }

  function updateVehicleStatus() {
    const statusEl = $('vehicle-status');
    const detailEl = $('vehicle-last-update');
    const dotEl = $('vehicle-dot');

    if (!statusEl || !detailEl || !dotEl) return;

    dotEl.classList.remove('online', 'stale', 'offline');

    if (!state.vehicleLastSeenMs) {
      statusEl.textContent = 'Keine Daten';
      detailEl.textContent = 'Noch kein Update empfangen';
      setText('side-updated', '--');
      dotEl.classList.add('offline');
      return;
    }

    const ageMs = Math.max(0, Date.now() - state.vehicleLastSeenMs);
    const relative = relativeTime(state.vehicleLastSeenMs);

    if (ageMs <= VEHICLE_ONLINE_MS) {
      statusEl.textContent = 'Online';
      dotEl.classList.add('online');
    } else if (ageMs <= VEHICLE_STALE_MS) {
      statusEl.textContent = 'Daten veraltet';
      dotEl.classList.add('stale');
    } else {
      statusEl.textContent = 'Offline';
      dotEl.classList.add('offline');
    }

    detailEl.textContent = `Letztes Update ${relative}`;
    setText('side-updated', relative);
  }

  function setVehicleLastSeen(value, source) {
    const timestampMs = parseTimestampMs(value);
    if (!timestampMs) return;

    state.vehicleLastSeenMs = timestampMs;
    state.vehicleLastSeenSource = source || '';
    updateVehicleStatus();
  }

  function updateDeviceInfo() {
    const mode = String(state.networkMode || '--').trim() || '--';
    const ip = String(state.deviceIp || '--').trim() || '--';
    const hasIp = isUsableIp(ip);
    const localWebUiReachable = hasIp && mode.toLowerCase() === 'wifi';

    setText('device-network-mode', mode);

    const ipEl = $('device-ip');
    if (ipEl) {
      ipEl.textContent = hasIp ? ip : '--';

      if (localWebUiReachable) {
        ipEl.href = `http://${ip}/`;
        ipEl.classList.add('available');
        ipEl.setAttribute('aria-disabled', 'false');
        ipEl.title = 'Lokale Geräte-WebUI öffnen';
      } else {
        ipEl.removeAttribute('href');
        ipEl.classList.remove('available');
        ipEl.setAttribute('aria-disabled', 'true');
        ipEl.title = hasIp
          ? 'Diese IP ist nur im gleichen lokalen Netzwerk erreichbar'
          : 'Noch keine Geräte-IP über MQTT empfangen';
      }
    }

    let detail = state.mqttDetail ||
      (state.mqttConnected ? 'Verbunden' : 'MQTT getrennt');

    if (state.mqttConnected) {
      const parts = ['Verbunden mit MQTT'];
      if (mode !== '--') parts.push(mode);
      if (hasIp) parts.push(ip);
      detail = parts.join(' · ');
    }

    setText('mqtt-detail', detail);

    const mobileDetail = $('mobile-device-detail');
    if (mobileDetail) {
      mobileDetail.textContent =
        `Netzwerk: ${mode} · WebUI: ${hasIp ? ip : '--'}`;

      mobileDetail.classList.toggle('available', localWebUiReachable);

      if (localWebUiReachable) {
        mobileDetail.setAttribute('role', 'link');
        mobileDetail.setAttribute('tabindex', '0');
        mobileDetail.title = 'Lokale Geräte-WebUI öffnen';
      } else {
        mobileDetail.removeAttribute('role');
        mobileDetail.removeAttribute('tabindex');
        mobileDetail.title = hasIp
          ? 'Diese IP ist nur im gleichen lokalen WLAN erreichbar'
          : 'Noch keine Geräte-IP über MQTT empfangen';
      }
    }
  }

  function openLocalWebUi() {
    const mode = String(state.networkMode || '').trim().toLowerCase();
    const ip = String(state.deviceIp || '').trim();

    if (mode === 'wifi' && isUsableIp(ip)) {
      window.open(`http://${ip}/`, '_blank', 'noopener');
    }
  }

  document.addEventListener('click', event => {
    if (event.target?.id === 'mobile-device-detail' &&
        event.target.classList.contains('available')) {
      openLocalWebUi();
    }
  });

  document.addEventListener('keydown', event => {
    if (event.target?.id === 'mobile-device-detail' &&
        event.target.classList.contains('available') &&
        (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      openLocalWebUi();
    }
  });

  function setOnline(ok, detail) {
    state.mqttConnected = ok;
    state.mqttDetail = detail || (ok ? 'Verbunden mit MQTT' : 'MQTT getrennt');

    setText('mqtt-status', ok ? 'Online' : 'Offline');
    setText('side-online', ok ? 'Online' : 'Offline');
    $('mqtt-dot')?.classList.toggle('online', ok);
    $('side-dot')?.classList.toggle('online', ok);

    updateDeviceInfo();
  }
  function updateClock() {
    const d = new Date();
    setText('date-now', d.toLocaleDateString(cfg.dashboard?.locale || 'de-CH'));
    setText('time-now', d.toLocaleTimeString(cfg.dashboard?.locale || 'de-CH'));
    updateVehicleStatus();
    if (state.values['location/latitude'] !== undefined && state.values['location/longitude'] !== undefined) {
      renderLocationStatus('mqtt');
    }
  }
  setInterval(updateClock, 1000); updateClock();

  function setSoc(v) {
    const soc = Math.max(0, Math.min(100, Number(v)));
    if (!Number.isFinite(soc)) return;
    setText('soc-main', `${soc.toFixed(0)}%`); setText('soc-battery', `${soc.toFixed(0)}%`);
    $('soc-ring')?.style.setProperty('--p', soc); $('soc-ring-2')?.style.setProperty('--p', soc);
    if (!state.values.range) {
      const maxRange = Number(vehicleCfg.defaultRangeKmAt100 || 140);
      setText('range-main', `${Math.round(maxRange * soc / 100)} km`);
    }
  }
  function locationReceivedAtMs() {
    const latMeta =
      state.metadata['location/latitude'] ||
      state.metadata['location/lat'] ||
      state.metadata['gps/latitude'] ||
      state.metadata['gps/lat'];
    const lonMeta =
      state.metadata['location/longitude'] ||
      state.metadata['location/lon'] ||
      state.metadata['gps/longitude'] ||
      state.metadata['gps/lon'];
    const latMs = parseTimestampMs(latMeta?.receivedAt);
    const lonMs = parseTimestampMs(lonMeta?.receivedAt);
    if (latMs && lonMs) return Math.min(latMs, lonMs);
    return latMs || lonMs || 0;
  }

  function formatLocationTimestamp(timestampMs) {
    const date = new Date(timestampMs);
    if (Number.isNaN(date.getTime())) return 'Zeitpunkt nicht verfügbar';

    return new Intl.DateTimeFormat(dashboardCfg.locale || 'de-CH', {
      dateStyle: 'short',
      timeStyle: 'medium'
    }).format(date);
  }

  function renderLocationStatus(source = 'mqtt') {
    if (source === 'default') {
      setText(
        'location-title',
        vehicleCfg.defaultLocation?.label || 'Default Standort'
      );
      setText('location-updated', 'Default Standort aus config.js');
      return;
    }

    const receivedAt = locationReceivedAtMs();

    if (!receivedAt) {
      setText('location-title', 'Letzter Standort');
      setText('location-updated', 'Zeitpunkt nicht verfügbar');
      return;
    }

    const ageMs = Math.max(0, Date.now() - receivedAt);
    const isCurrent = ageMs <= LOCATION_CURRENT_MS;
    const absolute = formatLocationTimestamp(receivedAt);
    const relative = relativeTime(receivedAt);

    setText(
      'location-title',
      isCurrent ? 'Aktueller Standort' : 'Letzter Standort'
    );
    setText(
      'location-updated',
      `Letzte Aktualisierung ${relative} · ${absolute}`
    );
  }




  function uptime(sec) {
    const n = Number(sec); if (!Number.isFinite(n)) return '--';
    const h = Math.floor(n / 3600), m = Math.floor((n % 3600) / 60);
    return h > 0 ? `${h}h ${m}m` : `${m}m`;
  }
  function parsePayload(payload) {
    const s = payload.toString();
    if (s === 'true') return true; if (s === 'false') return false;
    const n = Number(s); if (s.trim() !== '' && Number.isFinite(n)) return n;
    try { return JSON.parse(s); } catch { return s; }
  }
  function applyTopic(topic, payload) {
    const base = baseTopic() + '/';
    const key = topic.startsWith(base) ? topic.slice(base.length) : topic;
    const val = parsePayload(payload);
    state.values[key] = val;
    state.lastMessage = Date.now();
    if (
      dataSourceCfg.type === 'legacy-mqtt' &&
      (key.startsWith('location/') || key.startsWith('gps/'))
    ) {
      // A direct MQTT message is a real, newly received update. For AWS REST
      // snapshots the authoritative timestamp comes from snapshot.metadata.
      state.metadata[key] = { receivedAt: Date.now() };
    }
    switch (key) {
      case 'display/soc': setSoc(val); break;
      case 'display/speed_kmh': case 'display/speed': setText('speed-main', fmtNum(val,0)); setText('speed-card', fmtNum(val,0)); break;
      case 'display/odometer_km': case 'display/odo': setText('odo-main', `${fmtNum(val,1)} km`); break;
      case 'display/estimated_range_km': case 'display/range': state.values.range = val; setText('range-main', `${fmtNum(val,0)} km`); break;
      case 'charging/is_charging': { const charging = !!Number(val) || val === true; const t = charging ? 'Lädt' : 'Nicht am Laden'; setText('charging-main', t); setText('charging-card', t); break; }
      case 'charging/power_signed': case 'charging/power_display': { const p=Number(val)/10; setText('power', `${fmtNum(p,1)} kW`); if(p < -0.1) setText('charging-card','Rekuperation'); break; }
      case 'bms/pack_voltage': setText('voltage', `${fmtNum(val,1)} V`); setText('charge-voltage', `${fmtNum(val,1)} V`); break;
      case 'bms/pack_current': setText('current', `${fmtNum(val,1)} A`); setText('charge-current', `${fmtNum(val,1)} A`); break;
      case 'system/firmware': case 'system/version': case 'system/firmware_version': setText('fw-version', val); break;
      case 'system/device_id': setText('device-id', val); break;
      case 'system/rssi': case 'system/wifi_rssi': setText('rssi', `${fmtNum(val,0)} dBm`); break;
      case 'system/ip_address':
        state.deviceIp = String(val || '--');
        updateDeviceInfo();
        break;
      case 'system/network_mode':
        state.networkMode = String(val || '--');
        updateDeviceInfo();
        break;
      case 'system/last_seen_utc':
      case 'time/utc':
        setVehicleLastSeen(val, key);
        break;
      case 'system/uptime': case 'system/uptime_sec': setText('uptime', uptime(val)); break;
      case 'location/latitude': case 'location/lat': case 'gps/latitude': case 'gps/lat': updateCoords('mqtt'); break;
      case 'location/longitude': case 'location/lon': case 'gps/longitude': case 'gps/lon': updateCoords('mqtt'); break;
    }

    window.MOTHistoryRecorder?.update(state.values, {
      vehicleId: state.selectedVehicleId || mqttCfg.vehicleId || 'pioneer'
    });
  }

  function updateLocationMap(lat, lon) {
    const nLat = Number(lat);
    const nLon = Number(lon);
    if (!Number.isFinite(nLat) || !Number.isFinite(nLon)) return;

    const delta = 0.0032;
    const bbox = [
      (nLon - delta).toFixed(6),
      (nLat - delta).toFixed(6),
      (nLon + delta).toFixed(6),
      (nLat + delta).toFixed(6)
    ].join(',');

    const marker = `${nLat.toFixed(6)},${nLon.toFixed(6)}`;
    const iframe = $('location-map-frame');
    const link = $('location-map-link');

    const src = `https://www.openstreetmap.org/export/embed.html?bbox=${encodeURIComponent(bbox)}&layer=mapnik&marker=${encodeURIComponent(marker)}`;
    const href = `https://www.openstreetmap.org/?mlat=${nLat.toFixed(6)}&mlon=${nLon.toFixed(6)}#map=17/${nLat.toFixed(6)}/${nLon.toFixed(6)}`;

    if (iframe && iframe.src !== src) iframe.src = src;
    if (link) link.href = href;
  }

  function updateCoords(source = 'mqtt') {
    const lat = state.values['location/latitude'] ?? state.values['location/lat'] ?? state.values['gps/latitude'] ?? state.values['gps/lat'];
    const lon = state.values['location/longitude'] ?? state.values['location/lon'] ?? state.values['gps/longitude'] ?? state.values['gps/lon'];

    if (lat !== undefined && lon !== undefined) {
      renderLocationStatus(source);
      setText('location-coords', `${fmtCoord(lat, 'N', 'S')} · ${fmtCoord(lon, 'E', 'W')}`);
      updateLocationMap(lat, lon);
    }
  }

  function applyDefaultLocation() {
    const loc = vehicleCfg.defaultLocation || {};
    if (loc.enabled === false) return;
    const lat = Number(loc.latitude);
    const lon = Number(loc.longitude);
    if (!Number.isFinite(lat) || !Number.isFinite(lon)) return;

    state.values['location/latitude'] = lat;
    state.values['location/longitude'] = lon;
    updateCoords('default');
    setText('location-updated', 'Default Standort aus config.js');
  }
  function initBars() {
    const wrap = $('cell-bars'); if (!wrap) return;
    wrap.innerHTML = ''; for (let i=0;i<16;i++){ const b=document.createElement('span'); b.style.height = `${55 + (i%4)*4}%`; wrap.appendChild(b); }
  }
  function initStatic() {
    const img = vehicleCfg.image || 'img/microlino.jpeg';
    $('hero-image')?.setAttribute('src', img); $('brand-image')?.setAttribute('src', img);
    setText('vehicle-name', vehicleCfg.name || 'Microlino Pioneer'); setText('side-vehicle', mqttCfg.vehicleId || 'pioneer'); setText('side-topic', `${baseTopic()}/#`);
    initBars(); setSoc(NaN); applyDefaultLocation(); updateDeviceInfo(); updateVehicleStatus();
  }



function resetDashboardForVehicle(vehicleId) {
  state.values = {};
  state.metadata = {};
  state.lastMessage = 0;
  state.networkMode = '--';
  state.deviceIp = '--';
  state.vehicleLastSeenMs = 0;
  state.vehicleLastSeenSource = '';

  setText('side-vehicle', vehicleId || '--');
  setText('side-topic', vehicleId ? `${baseTopic(vehicleId)}/#` : '--');

  setText('soc-main', '--');
  setText('soc-battery', '--');
  $('soc-ring')?.style.setProperty('--p', 0);
  $('soc-ring-2')?.style.setProperty('--p', 0);
  setText('speed-main', '--');
  setText('speed-card', '--');
  setText('odo-main', '-- km');
  setText('range-main', '-- km');

  setText('charging-main', 'Keine Daten');
  setText('charging-card', 'Keine Daten');
  setText('power', '-- kW');
  setText('voltage', '-- V');
  setText('charge-voltage', '-- V');
  setText('current', '-- A');
  setText('charge-current', '-- A');

  setText('fw-version', '--');
  setText('device-id', '--');
  setText('rssi', '-- dBm');
  setText('uptime', '--');

  setText('location-title', 'Kein Standort');
  setText('location-coords', '--');
  setText('location-updated', 'Noch keine Standortdaten');

  const mapFrame = $('location-map-frame');
  if (mapFrame) mapFrame.removeAttribute('src');
  const mapLink = $('location-map-link');
  if (mapLink) mapLink.removeAttribute('href');

  updateDeviceInfo();
  updateVehicleStatus();
}


function updateVehicleSelector(vehicles) {
  state.availableVehicles = Array.isArray(vehicles) ? vehicles : [];
  const wrap = $('vehicle-selector-wrap');
  const select = $('vehicle-selector');
  if (!wrap || !select) return;

  select.innerHTML = '';
  state.availableVehicles.forEach(vehicle => {
    const option = document.createElement('option');
    option.value = vehicle.vehicleId;
    const status = vehicle.online === true
      ? 'online'
      : (vehicle.online === false ? 'offline' : '');
    option.textContent = status
      ? `${vehicle.vehicleId} · ${status}`
      : vehicle.vehicleId;
    select.appendChild(option);
  });

  if (!state.availableVehicles.some(v => v.vehicleId === state.selectedVehicleId)
      && state.availableVehicles.length) {
    state.selectedVehicleId = state.availableVehicles[0].vehicleId;
  }

  select.value = state.selectedVehicleId;
  wrap.hidden = state.availableVehicles.length <= 1;
}

async function selectVehicle(vehicleId) {
  if (!vehicleId || vehicleId === state.selectedVehicleId) return;

  state.selectedVehicleId = vehicleId;
  resetDashboardForVehicle(vehicleId);
  updateVehicleSelector(state.availableVehicles);

  if (state.dataProvider?.selectVehicle) {
    await state.dataProvider.selectVehicle(vehicleId);
  }
}

function startDataProvider() {
  const registry = window.MOTDataProviders;
  if (!registry) {
    setOnline(false, 'Data Provider Registry fehlt');
    return;
  }

  const type = dataSourceCfg.type || 'legacy-mqtt';
  let providerConfig;

  if (type === 'legacy-mqtt') {
    providerConfig = mqttCfg;
  } else if (type === 'aws-backend') {
    providerConfig = {
      ...(cfg.awsBackend || {}),
      vehicleId: mqttCfg.vehicleId || 'pioneer',
      topicPrefix: mqttCfg.topicPrefix || 'mot',
      getAccessToken: auth?.getAccessToken,
      onUnauthorized: () => {
        renderAuthState('Sitzung abgelaufen – erneut anmelden');
      }
    };
  } else {
    setOnline(false, `Unbekannte Datenquelle: ${type}`);
    return;
  }

  try {
    const provider = registry.create(type, { config: providerConfig });
    state.dataProvider = provider;

    provider.start({
      onConnection: (ok, detail) => setOnline(ok, detail),
      onMessage: (topic, payload) => applyTopic(topic, payload),
      onVehicles: vehicles => {
        const previous = state.selectedVehicleId;
        updateVehicleSelector(vehicles);
        const selected = provider.getSelectedVehicleId?.();

        if (selected) {
          state.selectedVehicleId = selected;
          updateVehicleSelector(vehicles);

          if (selected !== previous) {
            resetDashboardForVehicle(selected);
          }
        }
      },
      onSnapshot: snapshot => {
        if (snapshot?.vehicleId) state.selectedVehicleId = snapshot.vehicleId;
        state.metadata = snapshot?.metadata || {};
        updateCoords('mqtt');
      },
      onError: error => console.error('MOT data provider error:', error)
    });

    console.info('MOT provider details:', provider.describe?.());
    console.info('MOT provider capabilities:', registry.capabilities?.(type));
    if (auth) console.info('MOT auth details:', auth.describe?.());
  } catch (error) {
    console.error(error);
    setOnline(false, error?.message || 'Datenquelle konnte nicht gestartet werden');
  }
}

  $('vehicle-selector')?.addEventListener('change', event => {
    selectVehicle(event.target.value);
  });
  $('auth-login')?.addEventListener('click', beginLogin);
  $('auth-logout')?.addEventListener('click', beginLogout);
  async function bootstrap() {
    initStatic();
    resetDashboardForVehicle(state.selectedVehicleId);
    updateVehicleSelector([]);

    if (auth) {
      renderAuthState('Sitzung wird geprüft…');
      try {
        await auth.restoreSession();
      } catch (error) {
        console.error('MOT authentication callback failed:', error);
        renderAuthState(error.message || 'Anmeldung fehlgeschlagen');
        setOnline(false, 'Anmeldung fehlgeschlagen');
        return;
      }
      renderAuthState();
      if (!auth.isAuthenticated()) {
        setOnline(false, 'Anmeldung erforderlich');
        return;
      }
    }
    startDataProvider();
  }

  bootstrap().catch(error => {
    console.error('MOT dashboard bootstrap failed:', error);
    setOnline(false, error?.message || 'Dashboard konnte nicht gestartet werden');
  });
})();


window.addEventListener('DOMContentLoaded', () => {
  window.MOTHistoryChart?.init();
});

