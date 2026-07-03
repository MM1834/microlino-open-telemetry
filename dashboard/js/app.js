(function () {
  const cfg = window.MOT_CONFIG || {};
  const mqttCfg = cfg.mqtt || {};
  const vehicleCfg = cfg.vehicle || {};
  const $ = (id) => document.getElementById(id);
  const state = { lastMessage: 0, values: {} };

  function setText(id, value) { const el = $(id); if (el) el.textContent = value; }
  function fmtNum(v, digits = 0) { const n = Number(v); return Number.isFinite(n) ? n.toFixed(digits) : '--'; }
  function baseTopic() {
    const prefix = (mqttCfg.topicPrefix || 'mot').replace(/\/$/, '');
    const vehicle = mqttCfg.vehicleId || 'pioneer';
    return `${prefix}/${vehicle}`;
  }
  function setOnline(ok, detail) {
    setText('mqtt-status', ok ? 'Online' : 'Offline');
    setText('side-online', ok ? 'Online' : 'Offline');
    setText('mqtt-detail', detail || (ok ? 'Verbunden mit MQTT' : 'MQTT getrennt'));
    $('mqtt-dot')?.classList.toggle('online', ok);
    $('side-dot')?.classList.toggle('online', ok);
  }
  function updateClock() {
    const d = new Date();
    setText('date-now', d.toLocaleDateString(cfg.dashboard?.locale || 'de-CH'));
    setText('time-now', d.toLocaleTimeString(cfg.dashboard?.locale || 'de-CH'));
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
  function setUpdated() {
    const s = new Date().toLocaleTimeString(cfg.dashboard?.locale || 'de-CH');
    setText('side-updated', s); setText('location-updated', `Letzte Aktualisierung ${s}`);
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
    state.values[key] = val; state.lastMessage = Date.now(); setUpdated();
    switch (key) {
      case 'display/soc': setSoc(val); break;
      case 'display/speed_kmh': case 'display/speed': setText('speed-main', fmtNum(val,0)); setText('speed-card', fmtNum(val,0)); break;
      case 'display/odometer_km': case 'display/odo': setText('odo-main', `${fmtNum(val,1)} km`); break;
      case 'display/estimated_range_km': case 'display/range': state.values.range = val; setText('range-main', `${fmtNum(val,0)} km`); break;
      case 'charging/is_charging': { const charging = !!Number(val) || val === true; const t = charging ? 'Lädt' : 'Nicht am Laden'; setText('charging-main', t); setText('charging-card', t); break; }
      case 'charging/power_signed': case 'charging/power_display': setText('power', `${fmtNum(Number(val)/10,1)} kW`); break;
      case 'bms/pack_voltage': setText('voltage', `${fmtNum(val,1)} V`); setText('charge-voltage', `${fmtNum(val,1)} V`); break;
      case 'bms/pack_current': setText('current', `${fmtNum(val,1)} A`); setText('charge-current', `${fmtNum(val,1)} A`); break;
      case 'system/firmware': case 'system/version': setText('fw-version', val); break;
      case 'system/device_id': setText('device-id', val); break;
      case 'system/rssi': setText('rssi', `${fmtNum(val,0)} dBm`); break;
      case 'system/uptime': case 'system/uptime_sec': setText('uptime', uptime(val)); break;
      case 'location/lat': case 'gps/lat': updateCoords(); break;
      case 'location/lon': case 'gps/lon': updateCoords(); break;
    }
  }
  function updateCoords() {
    const lat = state.values['location/lat'] ?? state.values['gps/lat'];
    const lon = state.values['location/lon'] ?? state.values['gps/lon'];
    if (lat !== undefined && lon !== undefined) { setText('location-title', 'Letzter Standort'); setText('location-coords', `${fmtNum(lat,5)}° N · ${fmtNum(lon,5)}° E`); }
  }
  function initBars() {
    const wrap = $('cell-bars'); if (!wrap) return;
    wrap.innerHTML = ''; for (let i=0;i<16;i++){ const b=document.createElement('span'); b.style.height = `${55 + (i%4)*4}%`; wrap.appendChild(b); }
  }
  function initStatic() {
    const img = vehicleCfg.image || 'img/microlino.jpeg';
    $('hero-image')?.setAttribute('src', img); $('brand-image')?.setAttribute('src', img);
    setText('vehicle-name', vehicleCfg.name || 'Microlino Pioneer'); setText('side-vehicle', mqttCfg.vehicleId || 'pioneer'); setText('side-topic', `${baseTopic()}/#`);
    initBars(); setSoc(NaN);
  }
  function connect() {
    const mqttLib = window.mqtt || window.MQTT || window.Mqtt;
    if (!mqttLib || typeof mqttLib.connect !== 'function') { setOnline(false, 'mqtt.min.js fehlt oder ist ungültig'); return; }
    const protocol = mqttCfg.useTls ? 'wss' : 'ws';
    const path = mqttCfg.path || '/';
    const url = `${protocol}://${mqttCfg.host}:${mqttCfg.port}${path}`;
    setOnline(false, 'Connecting…');
    const client = mqttLib.connect(url, { username: mqttCfg.username || undefined, password: mqttCfg.password || undefined, clientId: `${mqttCfg.clientIdPrefix || 'mot-dashboard'}-${Math.random().toString(16).slice(2)}`, reconnectPeriod: 2500, connectTimeout: 15000 });
    client.on('connect', () => { setOnline(true, 'Verbunden mit MQTT'); client.subscribe(`${baseTopic()}/#`); });
    client.on('reconnect', () => setOnline(false, 'Reconnect…'));
    client.on('offline', () => setOnline(false, 'Offline'));
    client.on('close', () => setOnline(false, 'Verbindung geschlossen'));
    client.on('error', (err) => setOnline(false, err?.message || 'MQTT Fehler'));
    client.on('message', applyTopic);
  }
  initStatic(); connect();
})();
