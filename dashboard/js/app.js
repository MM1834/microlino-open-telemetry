(function () {
  const cfg = window.MOT_CONFIG || {};
  const mqttCfg = cfg.mqtt || {};
  const vehicleCfg = cfg.vehicle || {};
  const $ = (id) => document.getElementById(id);
  const state = { lastMessage: 0, values: {} };

  function setText(id, value) { const el = $(id); if (el) el.textContent = value; }
  function fmtNum(v, digits = 0) { const n = Number(v); return Number.isFinite(n) ? n.toFixed(digits) : '--'; }
  function fmtCoord(v, positiveSuffix, negativeSuffix) {
    const n = Number(v);
    if (!Number.isFinite(n)) return '--';
    const suffix = n >= 0 ? positiveSuffix : negativeSuffix;
    return `${Math.abs(n).toFixed(5)}° ${suffix}`;
  }
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
      case 'charging/power_signed': case 'charging/power_display': { const p=Number(val)/10; setText('power', `${fmtNum(p,1)} kW`); if(p < -0.1) setText('charging-card','Rekuperation'); break; }
      case 'bms/pack_voltage': setText('voltage', `${fmtNum(val,1)} V`); setText('charge-voltage', `${fmtNum(val,1)} V`); break;
      case 'bms/pack_current': setText('current', `${fmtNum(val,1)} A`); setText('charge-current', `${fmtNum(val,1)} A`); break;
      case 'system/firmware': case 'system/version': setText('fw-version', val); break;
      case 'system/device_id': setText('device-id', val); break;
      case 'system/rssi': setText('rssi', `${fmtNum(val,0)} dBm`); break;
      case 'system/uptime': case 'system/uptime_sec': setText('uptime', uptime(val)); break;
      case 'location/latitude': case 'location/lat': case 'gps/latitude': case 'gps/lat': updateCoords('mqtt'); break;
      case 'location/longitude': case 'location/lon': case 'gps/longitude': case 'gps/lon': updateCoords('mqtt'); break;
    }

    window.MOTHistoryRecorder?.update(state.values, {
      vehicleId: mqttCfg.vehicleId || 'pioneer'
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
      setText('location-title', source === 'default' ? (vehicleCfg.defaultLocation?.label || 'Default Standort') : 'Letzter Standort');
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
    initBars(); setSoc(NaN); applyDefaultLocation();
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


window.addEventListener('DOMContentLoaded', () => {
  window.MOTHistoryChart?.init();
});

