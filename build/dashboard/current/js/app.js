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
    authBusy: false,
    onboardingBusy: false,
    onboardingRequired: false,
    onboardingExpanded: false,
    notificationBusy: false,
    notificationVehicleId: null,
    notificationReadOnly: false,
    rangeForecast: null
  };


  function renderAuthState(message = '') {
    const container = $('auth-controls');
    const loginButton = $('auth-login');
    const logoutButton = $('auth-logout');
    const rememberWrap = $('auth-remember-wrap');
    const status = $('auth-status');
    if (!container || !auth) return;

    container.hidden = false;
    const authenticated = auth.isAuthenticated();
    loginButton.hidden = authenticated;
    logoutButton.hidden = !authenticated;
    if (rememberWrap) rememberWrap.hidden = authenticated;
    loginButton.disabled = state.authBusy || !auth.isConfigured();
    logoutButton.disabled = state.authBusy;

    if (message) status.textContent = message;
    else if (!auth.isConfigured()) status.textContent = 'Cognito nicht konfiguriert';
    else status.textContent = authenticated ? 'Angemeldet' : 'Nicht angemeldet';
    renderOnboarding(state.onboardingRequired, '');
  }

  function renderOnboarding(required, message = '') {
    const panel = $('onboarding-required');
    const form = $('onboarding-form');
    const input = $('onboarding-claim');
    const status = $('onboarding-status');
    const addButton = $('vehicle-add');
    const title = $('onboarding-title');
    const description = $('onboarding-description');
    if (!panel) return;
    state.onboardingRequired = Boolean(required);
    const authenticated = Boolean(auth?.isAuthenticated());
    const demoReadOnly = String(state.selectedVehicleId || '').startsWith('demo-');
    const canClaim = authenticated && Boolean(state.dataProvider?.claimVehicle) && !demoReadOnly;
    const visible = canClaim && (state.onboardingRequired || state.onboardingExpanded);
    panel.hidden = !visible;
    if (form) form.hidden = !visible;
    if (addButton) {
      addButton.hidden = !canClaim || state.onboardingRequired;
      addButton.textContent = state.onboardingExpanded ? 'Abbrechen' : 'Fahrzeug hinzufügen';
    }
    if (title) title.textContent = state.onboardingRequired ? 'Fahrzeug verbinden' : 'Weiteres Fahrzeug hinzufügen';
    if (description) description.textContent = state.onboardingRequired
      ? 'Deinem Konto ist noch kein Fahrzeug zugewiesen. Gib den einmaligen Claim-Code ein, den du mit deinem Adapter erhalten hast.'
      : 'Gib den einmaligen Claim-Code des zusätzlichen Fahrzeugs ein. Bestehende Fahrzeuge bleiben zugewiesen.';
    if (input) input.disabled = state.onboardingBusy;
    const button = $('onboarding-submit');
    if (button) button.disabled = state.onboardingBusy;
    if (status) status.textContent = message;
  }

  function toggleOnboarding() {
    if (state.onboardingRequired || !auth?.isAuthenticated()
      || String(state.selectedVehicleId || '').startsWith('demo-')) return;
    state.onboardingExpanded = !state.onboardingExpanded;
    renderOnboarding(false, '');
    if (state.onboardingExpanded) $('onboarding-claim')?.focus();
  }

  async function submitOnboarding(event) {
    event.preventDefault();
    if (state.onboardingBusy || !state.dataProvider?.claimVehicle
      || String(state.selectedVehicleId || '').startsWith('demo-')) return;
    const input = $('onboarding-claim');
    const claim = String(input?.value || '').trim();
    if (!claim) {
      renderOnboarding(state.onboardingRequired, 'Bitte Claim-Code eingeben.');
      return;
    }
    state.onboardingBusy = true;
    renderOnboarding(state.onboardingRequired, 'Fahrzeug wird zugewiesen…');
    try {
      await state.dataProvider.claimVehicle(claim);
      if (input) input.value = '';
      state.onboardingExpanded = false;
      renderOnboarding(false, 'Fahrzeug erfolgreich zugewiesen.');
    } catch (error) {
      state.onboardingExpanded = true;
      renderOnboarding(state.onboardingRequired, error?.message || 'Onboarding fehlgeschlagen');
    } finally {
      state.onboardingBusy = false;
      const stillRequired = state.availableVehicles.length === 0;
      renderOnboarding(stillRequired, stillRequired ? $('onboarding-status')?.textContent || '' : '');
    }
  }

  function renderOnboardingAdmin(message = '') {
    const panel = $('onboarding-admin');
    if (!panel || !auth) return;
    panel.hidden = !(auth.isAuthenticated() && auth.hasGroup?.('mot-beta-admins'));
    const status = $('admin-claim-status');
    if (status && message) status.textContent = message;
  }

  async function issueOnboardingClaim(event) {
    event.preventDefault();
    if (state.onboardingBusy || !state.dataProvider?.issueClaim) return;
    const vehicleId = String($('admin-vehicle-id')?.value || '').trim();
    const output = $('admin-claim-output');
    state.onboardingBusy = true;
    if (output) { output.hidden = true; output.textContent = ''; }
    renderOnboardingAdmin('Claim wird erstellt…');
    try {
      const result = await state.dataProvider.issueClaim(vehicleId);
      if (output) { output.textContent = result.claim || ''; output.hidden = false; }
      renderOnboardingAdmin(`Claim für ${result.vehicleId} erstellt; gültig bis ${new Date(result.expiresAt * 1000).toLocaleString()}.`);
    } catch (error) {
      renderOnboardingAdmin(error?.message || 'Claim-Ausgabe fehlgeschlagen');
    } finally {
      state.onboardingBusy = false;
    }
  }

  function clearIssuedClaim() {
    const output = $('admin-claim-output');
    if (output) { output.textContent = ''; output.hidden = true; }
    renderOnboardingAdmin('Claim-Anzeige wurde geleert.');
  }

  async function beginLogin() {
    if (!auth || state.authBusy) return;
    state.authBusy = true;
    renderAuthState('Weiterleitung…');
    try { await auth.login({ remember: Boolean($('auth-remember')?.checked) }); }
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
  const OBD2_FRESHNESS_KEYS = [
    'display/soc',
    'display/speed_kmh',
    'display/odometer_km'
  ];
  const CHARGING_FRESHNESS_KEYS = [
    'charging/is_charging',
    'charging/plugged'
  ];
  const POWER_FRESHNESS_KEYS = [
    'bms/vehicle_power_w',
    'bms/pack_power_w',
    'charging/power_signed',
    'charging/power_display'
  ];

  function setText(id, value) {
    const el = $(id);
    if (el) el.textContent = value;
    document.querySelectorAll(`[data-mirror-for="${id}"]`).forEach(mirror => {
      mirror.textContent = value;
    });
  }
  function syncDot(id) {
    const source = $(id);
    if (!source) return;
    document.querySelectorAll(`[data-mirror-dot="${id}"]`).forEach(mirror => {
      mirror.className = source.className;
    });
  }
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
      setText('vehicle-status', 'Keine Daten');
      setText('vehicle-last-update', 'Noch kein Update empfangen');
      setText('side-updated', '--');
      dotEl.classList.add('offline');
      syncDot('vehicle-dot');
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
    setText('vehicle-status', statusEl.textContent);
    setText('vehicle-last-update', detailEl.textContent);
    setText('side-updated', relative);
    syncDot('vehicle-dot');
  }

  function setLiveStatus(status = {}) {
    const stateName = status.state || 'disabled';
    const labels = {
      connected: 'Verbunden',
      connecting: 'Verbinden…',
      reconnecting: 'Wiederverbinden…',
      disconnected: 'Getrennt',
      disabled: 'Deaktiviert'
    };
    setText('live-status', labels[stateName] || stateName);
    setText('live-detail', status.detail || '');
    const dot = $('live-dot');
    if (!dot) return;
    dot.classList.remove('online', 'stale');
    if (stateName === 'connected') dot.classList.add('online');
    else if (stateName === 'connecting' || stateName === 'reconnecting') dot.classList.add('stale');
    syncDot('live-dot');
  }

  function updateObd2Freshness() {
    let latest = 0;
    let source = '';
    OBD2_FRESHNESS_KEYS.forEach(key => {
      const receivedAt = parseTimestampMs(state.metadata[key]?.receivedAt);
      if (receivedAt > latest) {
        latest = receivedAt;
        source = key;
      }
    });
    state.vehicleLastSeenMs = latest;
    state.vehicleLastSeenSource = source;
    updateVehicleStatus();
  }

  function updateSocFreshness() {
    const receivedAt = parseTimestampMs(state.metadata['display/soc']?.receivedAt);
    if (!receivedAt) {
      setText('soc-updated', 'Stand: unbekannt');
      return;
    }
    const ageMs = Math.max(0, Date.now() - receivedAt);
    const stateLabel = ageMs <= VEHICLE_ONLINE_MS ? 'aktuell' : 'veraltet';
    setText('soc-updated', `Stand: ${relativeTime(receivedAt)} · ${stateLabel}`);
  }

  function latestTopicTimestamp(keys) {
    return keys.reduce((latest, key) => Math.max(
      latest,
      parseTimestampMs(state.metadata[key]?.receivedAt)
    ), 0);
  }

  function formatFreshnessTime(timestampMs) {
    return new Intl.DateTimeFormat(dashboardCfg.locale || 'de-CH', {
      hour: '2-digit',
      minute: '2-digit'
    }).format(new Date(timestampMs));
  }

  function updatePowerFreshness() {
    const card = $('overview')?.querySelector('.overview-charging');
    const status = $('power-updated');
    if (!card || !status) return;

    const charging = state.values['charging/is_charging'] === true ||
      Number(state.values['charging/is_charging']) === 1;
    const speed = Number(state.values['display/speed_kmh'] ?? state.values['display/speed']);
    const moving = !charging && Number.isFinite(speed) && speed > 1;
    const chargingTimestamp = latestTopicTimestamp(CHARGING_FRESHNESS_KEYS);
    const powerTimestamp = latestTopicTimestamp(POWER_FRESHNESS_KEYS);
    const receivedAt = moving
      ? powerTimestamp
      : charging
        ? Math.max(chargingTimestamp, powerTimestamp)
        : chargingTimestamp;

    if (!receivedAt) {
      card.classList.remove('is-data-stale');
      status.hidden = true;
      status.textContent = '';
      return;
    }

    const stale = Date.now() - receivedAt > VEHICLE_ONLINE_MS;
    card.classList.toggle('is-data-stale', stale);
    status.hidden = !stale;
    status.textContent = stale
      ? `Nicht aktuell · letzter Messpunkt ${formatFreshnessTime(receivedAt)}`
      : '';
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
    setText('mobile-connection-network', `Netzwerk: ${mode} · WebUI: ${hasIp ? ip : '--'}`);
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
    syncDot('mqtt-dot');

    updateDeviceInfo();
  }
  function updateClock() {
    const d = new Date();
    setText('date-now', d.toLocaleDateString(cfg.dashboard?.locale || 'de-CH'));
    setText('time-now', d.toLocaleTimeString(cfg.dashboard?.locale || 'de-CH'));
    updateVehicleStatus();
    updateSocFreshness();
    updatePowerFreshness();
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
    renderRangeForecast();
  }
  function renderRangeForecast() {
    const soc = Number(state.values['display/soc']);
    const maxRange = Number(vehicleCfg.defaultRangeKmAt100 || 140);
    if (!Number.isFinite(soc)) return;
    const standardRange = Math.round(maxRange * soc / 100);
    const forecast = state.rangeForecast;
    const learned = Number(forecast?.effectiveKmPerSoc);
    const hasForecast = Number.isFinite(learned) && Number(forecast?.tripCount) > 0;
    const displayedRange = hasForecast ? Math.round(soc * learned) : standardRange;
    setText('range-main', `${displayedRange} km`);
    setText('range-forecast-main', `${displayedRange} km`);
    setText('range-method', hasForecast ? 'Persönliche Prognose' : `Nach SoC · Basis ${maxRange} km`);
    setText('range-soc-comparison', `Nach SoC: ${standardRange} km`);
    setText('range-forecast-basis', hasForecast
      ? `Basierend auf ${fmtNum(forecast.distanceKm, 0)} km · ${forecast.tripCount} ${Number(forecast.tripCount) === 1 ? 'Fahrt' : 'Fahrten'}`
      : 'Noch keine ausreichende Fahrhistorie');
  }
  window.addEventListener('mot-range-forecast', event => {
    state.rangeForecast = event.detail || null;
    renderRangeForecast();
  });
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
  function updateBmsPowerFlow() {
    const vehiclePower = Number(state.values['bms/vehicle_power_w']);
    const packPower = Number(state.values['bms/pack_power_w']);
    const powerW = Number.isFinite(vehiclePower)
      ? vehiclePower
      : (Number.isFinite(packPower) ? -packPower : NaN);
    const charging = state.values['charging/is_charging'] === true || Number(state.values['charging/is_charging']) === 1;
    const regenerating = state.values['bms/is_regenerating'] === true || Number(state.values['bms/is_regenerating']) === 1 || powerW < -100;
    const discharging = state.values['bms/is_discharging'] === true || Number(state.values['bms/is_discharging']) === 1 || powerW > 100;
    if (Number.isFinite(powerW)) {
      const displayedPowerW = charging ? Math.abs(powerW) : powerW;
      setText('power', `${fmtNum(displayedPowerW / 1000, 2)} kW`);
    }
    const chargingPowerMain = document.getElementById('charging-power-main');
    if (chargingPowerMain) {
      chargingPowerMain.hidden = !(charging && Number.isFinite(powerW));
      if (!chargingPowerMain.hidden) chargingPowerMain.textContent = `${fmtNum(Math.abs(powerW) / 1000, 2)} kW`;
    }
    setText('power-label', charging ? 'Ladeleistung' : 'Fahrzeugleistung');
    const flow = charging ? 'Laden' : regenerating ? 'Rekuperation' : discharging ? 'Verbrauch' : 'Bereit';
    setText('power-flow', flow);
    setText('mobile-power-flow', flow);
    setText('mobile-vehicle-power', Number.isFinite(powerW) ? `${fmtNum(Math.abs(powerW) / 1000, 2)} kW` : '-- kW');
    const speed = Number(state.values['display/speed_kmh'] ?? state.values['display/speed']);
    const moving = !charging && Number.isFinite(speed) && speed > 1;
    $('overview')?.querySelector('.overview-charging')?.classList.toggle('is-driving', moving);
    const powerKw = Number.isFinite(powerW) ? Math.abs(powerW) / 1000 : NaN;
    document.querySelectorAll('[data-power-meter]').forEach(meter => {
      const visible = Number.isFinite(powerKw) && (charging || moving);
      meter.hidden = !visible;
      if (!visible) return;
      const mode = charging ? 'charging' : regenerating ? 'regeneration' : 'consumption';
      const scaleMax = mode === 'charging' ? 3.5 : 20;
      const level = mode === 'charging'
        ? (powerKw <= 1.6 ? 'low' : powerKw <= 2.4 ? 'medium' : 'high')
        : mode === 'regeneration'
          ? (powerKw <= 5 ? 'low' : powerKw <= 10 ? 'medium' : 'high')
          : (powerKw <= 3 ? 'low' : powerKw <= 10 ? 'medium' : 'high');
      meter.dataset.mode = mode;
      meter.dataset.level = level;
      meter.setAttribute('aria-valuemax', String(scaleMax));
      meter.setAttribute('aria-valuenow', powerKw.toFixed(2));
      meter.querySelector('[data-power-meter-fill]')?.style.setProperty('--power-level', `${Math.min(100, powerKw / scaleMax * 100)}%`);
    });
    updatePowerFreshness();
  }
  function renderChargingState() {
    const charging = state.values['charging/is_charging'] === true || Number(state.values['charging/is_charging']) === 1;
    const plugged = state.values['charging/plugged'] === true || Number(state.values['charging/plugged']) === 1;
    const label = charging ? 'Lädt' : plugged ? 'Eingesteckt' : 'Nicht am Laden';
    setText('charging-main', label);
    setText('charging-card', label);
    updatePowerFreshness();
  }
  function applyTopic(topic, payload, metadata = null) {
    const base = baseTopic() + '/';
    const key = topic.startsWith(base) ? topic.slice(base.length) : topic;
    const val = parsePayload(payload);
    state.values[key] = val;
    state.lastMessage = Date.now();
    if (metadata?.receivedAt) {
      state.metadata[key] = metadata;
    } else if (dataSourceCfg.type === 'legacy-mqtt') {
      // A direct legacy MQTT message is a newly received value. AWS REST and
      // WebSocket updates carry their authoritative backend receivedAt value.
      state.metadata[key] = { receivedAt: Date.now() };
    }
    switch (key) {
      case 'display/soc': setSoc(val); break;
      case 'display/speed_kmh': case 'display/speed': setText('speed-main', fmtNum(val,0)); setText('speed-card', fmtNum(val,0)); updateBmsPowerFlow(); break;
      case 'display/odometer_km': case 'display/odo': setText('odo-main', `${fmtNum(val,0)} km`); break;
      case 'display/estimated_range_km': case 'display/range': state.values.range = val; renderRangeForecast(); break;
      case 'charging/is_charging': renderChargingState(); updateBmsPowerFlow(); break;
      case 'charging/plugged': renderChargingState(); break;
      case 'charging/power_signed': case 'charging/power_display': {
        const p=Number(val)/10;
        const charging = state.values['charging/is_charging'] === true || Number(state.values['charging/is_charging']) === 1;
        const plugged = state.values['charging/plugged'] === true || Number(state.values['charging/plugged']) === 1;
        setText('power', `${fmtNum(charging ? Math.abs(p) : p,1)} kW`);
        if(p < -0.1 && !charging && !plugged) setText('charging-card','Rekuperation');
        break;
      }
      case 'bms/pack_voltage': setText('voltage', `${fmtNum(val,1)} V`); setText('charge-voltage', `${fmtNum(val,1)} V`); break;
      case 'bms/pack_current': setText('current', `${fmtNum(val,1)} A`); setText('charge-current', `${fmtNum(val,1)} A`); break;
      case 'bms/pack_power_w': case 'bms/vehicle_power_w': case 'bms/is_regenerating': case 'bms/is_discharging': updateBmsPowerFlow(); break;
      case 'bms/cell_min_mv': setText('cell-min', `${fmtNum(Number(val)/1000,3)} V`); break;
      case 'bms/cell_max_mv': setText('cell-max', `${fmtNum(Number(val)/1000,3)} V`); break;
      case 'bms/cell_delta_mv': setText('cell-delta', `${fmtNum(val,0)} mV`); break;
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
        // Device heartbeat freshness belongs to the Cloud/device status. It must
        // not make retained OBD2 values appear current.
        break;
      case 'system/uptime': case 'system/uptime_sec': setText('uptime', uptime(val)); break;
      case 'location/latitude': case 'location/lat': case 'gps/latitude': case 'gps/lat': updateCoords('mqtt'); break;
      case 'location/longitude': case 'location/lon': case 'gps/longitude': case 'gps/lon': updateCoords('mqtt'); break;
    }
    if (OBD2_FRESHNESS_KEYS.includes(key)) updateObd2Freshness();
    if (key === 'display/soc') updateSocFreshness();
    if (CHARGING_FRESHNESS_KEYS.includes(key) || POWER_FRESHNESS_KEYS.includes(key)) updatePowerFreshness();

    if (dataSourceCfg.type === 'legacy-mqtt') {
      window.MOTHistoryRecorder?.update(state.values, {
        vehicleId: state.selectedVehicleId || mqttCfg.vehicleId || 'pioneer'
      });
    }
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
  state.rangeForecast = null;

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
  setText('range-method', `Nach SoC · Basis ${Number(vehicleCfg.defaultRangeKmAt100 || 140)} km`);
  setText('range-forecast-main', '-- km');
  setText('range-forecast-basis', 'Noch keine ausreichende Fahrhistorie');
  setText('range-soc-comparison', 'Nach SoC: -- km');

  setText('charging-main', 'Keine Daten');
  setText('charging-card', 'Keine Daten');
  setText('mobile-power-flow', '--');
  setText('mobile-vehicle-power', '-- kW');
  $('overview')?.querySelector('.overview-charging')?.classList.remove('is-driving');
  $('overview')?.querySelector('.overview-charging')?.classList.remove('is-data-stale');
  const powerUpdated = $('power-updated');
  if (powerUpdated) {
    powerUpdated.hidden = true;
    powerUpdated.textContent = '';
  }
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
  await loadNotificationPreferences(true);
  window.MOTHistoryChart?.render?.();
}

function renderNotificationPreferences(preferences = null, message = '') {
  const panel = $('settings');
  const supported = Boolean(state.dataProvider?.getNotificationPreferences && auth?.isAuthenticated());
  if (!panel) return;
  panel.hidden = !supported || !state.selectedVehicleId;
  if (panel.hidden) return;
  $('notification-vehicle').textContent = state.selectedVehicleId;
  if (preferences) {
    state.notificationReadOnly = preferences.readOnly === true;
    $('notification-enabled').checked = preferences.enabled === true;
    $('notification-threshold').value = Number(preferences.threshold || 80);
    $('notification-email-enabled').checked = preferences.emailEnabled === true;
    $('notification-journey-email-enabled').checked = preferences.journeyEmailEnabled === true;
    $('notification-charging-stop-email-enabled').checked = preferences.chargingStopEmailEnabled === true;
    $('notification-charging-stop-threshold').value = Number(preferences.chargingStopThreshold || 80);
    $('notification-email').value = preferences.email || '';
    $('notification-email-state').textContent = preferences.emailConfirmed
      ? 'E-Mail-Adresse bestätigt'
      : (preferences.emailEnabled ? 'Bestätigung ausstehend' : 'E-Mail deaktiviert');
    $('notification-email').dataset.confirmedEmail = preferences.emailConfirmed === true
      ? String(preferences.email || '').trim().toLowerCase()
      : '';
    updateEmailConfirmationHelp();
  }
  const disabled = state.notificationBusy || state.notificationReadOnly;
  ['notification-enabled', 'notification-threshold', 'notification-email-enabled',
    'notification-journey-email-enabled', 'notification-charging-stop-email-enabled',
    'notification-charging-stop-threshold', 'notification-email',
    'notification-save'].forEach(id => { if ($(id)) $(id).disabled = disabled; });
  $('notification-status').textContent = state.notificationReadOnly
    ? 'Demo-Zugang: Benachrichtigungen sind deaktiviert.'
    : message;
}

function updateEmailConfirmationHelp() {
  const email = $('notification-email');
  const help = $('notification-email-confirmation-help');
  if (!email || !help) return;
  const confirmedEmail = email.dataset.confirmedEmail || '';
  const currentEmail = String(email.value || '').trim().toLowerCase();
  const stillConfirmed = Boolean(confirmedEmail) && currentEmail === confirmedEmail;
  help.hidden = stillConfirmed;
  if (confirmedEmail && !stillConfirmed) {
    $('notification-email-state').textContent = 'Neue E-Mail-Adresse muss bestätigt werden';
  }
}

async function loadNotificationPreferences(force = false) {
  if (!state.dataProvider?.getNotificationPreferences || !state.selectedVehicleId) return;
  if (!force && state.notificationVehicleId === state.selectedVehicleId) return;
  state.notificationBusy = true;
  renderNotificationPreferences(null, 'Einstellungen werden geladen…');
  try {
    const preferences = await state.dataProvider.getNotificationPreferences();
    state.notificationVehicleId = state.selectedVehicleId;
    state.notificationBusy = false;
    renderNotificationPreferences(preferences, '');
  } catch (error) {
    state.notificationBusy = false;
    renderNotificationPreferences(null, error?.message || 'Einstellungen konnten nicht geladen werden');
  }
}

async function saveNotificationPreferences(event) {
  event.preventDefault();
  if (state.notificationBusy || state.notificationReadOnly || !state.dataProvider?.saveNotificationPreferences) return;
  if ($('notification-journey-email-enabled').checked && !$('notification-email-enabled').checked) {
    renderNotificationPreferences(null, 'Für Fahrtzusammenfassungen zuerst den E-Mail-Kanal aktivieren.');
    return;
  }
  if ($('notification-charging-stop-email-enabled').checked && !$('notification-email-enabled').checked) {
    renderNotificationPreferences(null, 'Für Ladestopp-Meldungen zuerst den E-Mail-Kanal aktivieren.');
    return;
  }
  state.notificationBusy = true;
  renderNotificationPreferences(null, 'Wird gespeichert…');
  try {
    const preferences = await state.dataProvider.saveNotificationPreferences({
      enabled: $('notification-enabled').checked,
      threshold: Number($('notification-threshold').value),
      emailEnabled: $('notification-email-enabled').checked,
      journeyEmailEnabled: $('notification-journey-email-enabled').checked,
      chargingStopEmailEnabled: $('notification-charging-stop-email-enabled').checked,
      chargingStopThreshold: Number($('notification-charging-stop-threshold').value),
      email: String($('notification-email').value || '').trim(),
      smsEnabled: false
    });
    state.notificationBusy = false;
    renderNotificationPreferences(preferences, 'Gespeichert');
  } catch (error) {
    state.notificationBusy = false;
    renderNotificationPreferences(null, error?.message || 'Speichern fehlgeschlagen');
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
    window.MOTHistorySource = {
      getHistory: hours => provider.getHistory?.(hours),
      getVehicleId: () => state.selectedVehicleId
    };

    provider.start({
      onConnection: (ok, detail) => setOnline(ok, detail),
      onLiveConnection: status => setLiveStatus(status),
      onLiveMessage: message => console.debug('MOT live control message:', message),
      onMessage: (topic, payload, metadata) => applyTopic(topic, payload, metadata),
      onVehicles: vehicles => {
        const previous = state.selectedVehicleId;
        updateVehicleSelector(vehicles);
        const selected = provider.getSelectedVehicleId?.();

        if (selected) {
          state.selectedVehicleId = selected;
          updateVehicleSelector(vehicles);

          if (selected !== previous) {
            resetDashboardForVehicle(selected);
            window.MOTHistoryChart?.render?.();
          }
        }
        loadNotificationPreferences().catch(error => console.error('Notification settings failed:', error));
      },
      onOnboardingRequired: required => renderOnboarding(required),
      onSnapshot: snapshot => {
        if (snapshot?.vehicleId) state.selectedVehicleId = snapshot.vehicleId;
        state.metadata = snapshot?.metadata || {};
        updateObd2Freshness();
        updateSocFreshness();
        updatePowerFreshness();
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
  $('vehicle-add')?.addEventListener('click', toggleOnboarding);
  $('onboarding-form')?.addEventListener('submit', submitOnboarding);
  $('admin-claim-form')?.addEventListener('submit', issueOnboardingClaim);
  $('admin-claim-clear')?.addEventListener('click', clearIssuedClaim);
  $('notification-form')?.addEventListener('submit', saveNotificationPreferences);
  $('notification-email')?.addEventListener('input', updateEmailConfirmationHelp);
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
      renderOnboardingAdmin();
    }
    setLiveStatus({ state: 'connecting', detail: 'WebSocket wird initialisiert' });
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
