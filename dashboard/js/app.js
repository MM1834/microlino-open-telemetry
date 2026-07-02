(() => {
  const cfg = window.MOT_CONFIG || {};
  const state = { values: {}, topics: {}, lastMessageAt: 0 };
  const $ = (id) => document.getElementById(id);

  const topicBase = `${cfg.topicPrefix || 'mot'}/${cfg.vehicleId || 'pioneer'}`;
  $('vehicle-id').textContent = cfg.vehicleId || 'pioneer';
  $('topic-base').textContent = topicBase;

  function setText(id, value, fallback = '--') {
    const el = $(id);
    if (!el) return;
    el.textContent = (value === undefined || value === null || Number.isNaN(value)) ? fallback : value;
  }

  function parseBool(v) {
    return v === true || v === 'true' || v === '1' || v === 1 || v === 'yes';
  }

  function updateUi() {
    const v = state.values;
    const soc = parseFloat(v['display/soc']);
    if (!Number.isNaN(soc)) {
      setText('soc', soc.toFixed(1));
      $('soc-bar').style.width = `${Math.max(0, Math.min(100, soc))}%`;
    }
    setText('speed', fmt(v['display/speed_kmh'] ?? v['display/speed'], 1));
    setText('range', fmt(v['display/estimated_range_km'] ?? v['display/range'], 0));
    setText('odo', fmt(v['display/odometer_km'] ?? v['display/odo'], 1));
    setText('power-display', v['charging/power_display']);
    setText('firmware', v['system/firmware'] || v['system/firmware_version']);
    setText('device-id', v['system/device_id']);
    setText('ip-address', v['system/ip'] || v['system/ip_address']);
    setText('rssi', v['system/rssi'] || v['system/wifi_rssi']);
    setText('uptime', v['system/uptime'] || v['system/uptime_sec']);

    const charging = parseBool(v['charging/is_charging']);
    setText('is-charging', charging ? 'Charging' : 'Not charging');
    const badge = $('charging-badge');
    badge.textContent = charging ? '⚡ Charging' : 'Not charging';
    badge.classList.toggle('charging', charging);

    if (state.lastMessageAt) {
      setText('last-update', new Date(state.lastMessageAt).toLocaleTimeString());
      const stale = Date.now() - state.lastMessageAt > (cfg.staleAfterMs || 15000);
      $('stale-state').textContent = stale ? 'stale data' : 'live';
      $('stale-state').classList.toggle('stale', stale);
    }
  }

  function fmt(v, decimals) {
    const n = parseFloat(v);
    return Number.isNaN(n) ? undefined : n.toFixed(decimals);
  }

  function setMqttStatus(online, text) {
    const el = $('mqtt-status');
    el.textContent = text || (online ? 'MQTT online' : 'MQTT offline');
    el.classList.toggle('online', online);
    el.classList.toggle('offline', !online);
  }

  function logTopic(topic, payload) {
    state.topics[topic] = payload;
    const lines = Object.entries(state.topics)
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([t, p]) => `${t}\n  ${p}`);
    $('topic-log').textContent = lines.join('\n\n');
  }

  function connect() {
    const mqttLib = window.mqtt || window.MQTT || window.Mqtt;
    if (!mqttLib || typeof mqttLib.connect !== 'function') {
      setMqttStatus(false, 'mqtt.js missing');
      console.error('MQTT.js not available. window.mqtt =', window.mqtt);
      return;
    }
    const protocol = cfg.useTls ? 'wss' : 'ws';
    const url = `${protocol}://${cfg.host}:${cfg.port}`;
    const clientId = `mot-dashboard-${Math.random().toString(16).slice(2)}`;
    setMqttStatus(false, 'Connecting...');
    const client = mqttLib.connect(url, {
      clientId,
      username: cfg.username || undefined,
      password: cfg.password || undefined,
      reconnectPeriod: cfg.reconnectPeriodMs || 3000,
      connectTimeout: cfg.connectTimeoutMs || 8000,
      clean: true
    });
    client.on('connect', () => {
      setMqttStatus(true, 'MQTT online');
      client.subscribe(`${topicBase}/#`);
    });
    client.on('reconnect', () => setMqttStatus(false, 'Reconnecting...'));
    client.on('close', () => setMqttStatus(false, 'MQTT offline'));
    client.on('error', (err) => setMqttStatus(false, err.message || 'MQTT error'));
    client.on('message', (topic, payloadBuf) => {
      const payload = payloadBuf.toString();
      const suffix = topic.replace(`${topicBase}/`, '');
      state.values[suffix] = payload;
      state.lastMessageAt = Date.now();
      logTopic(topic, payload);
      updateUi();
    });
  }

  $('toggle-debug').addEventListener('click', () => {
    $('topic-log').classList.toggle('hidden');
  });

  setInterval(updateUi, 1000);
  connect();
})();
