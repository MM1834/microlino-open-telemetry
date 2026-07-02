const els = {
  status: document.getElementById('connectionStatus'),
  soc: document.getElementById('soc'),
  socBar: document.getElementById('socBar'),
  range: document.getElementById('range'),
  speed: document.getElementById('speed'),
  odo: document.getElementById('odo'),
  charging: document.getElementById('charging'),
  lastUpdate: document.getElementById('lastUpdate'),
  mqttUrl: document.getElementById('mqttUrl'),
  baseTopic: document.getElementById('baseTopic'),
  mqttUser: document.getElementById('mqttUser'),
  mqttPass: document.getElementById('mqttPass'),
  connectBtn: document.getElementById('connectBtn'),
  saveBtn: document.getElementById('saveBtn'),
};

let client = null;

function loadSettings() {
  els.mqttUrl.value = localStorage.getItem('mot.mqttUrl') || '';
  els.baseTopic.value = localStorage.getItem('mot.baseTopic') || 'mot/microlino';
  els.mqttUser.value = localStorage.getItem('mot.mqttUser') || '';
  els.mqttPass.value = localStorage.getItem('mot.mqttPass') || '';
}

function saveSettings() {
  localStorage.setItem('mot.mqttUrl', els.mqttUrl.value.trim());
  localStorage.setItem('mot.baseTopic', els.baseTopic.value.trim().replace(/\/$/, ''));
  localStorage.setItem('mot.mqttUser', els.mqttUser.value);
  localStorage.setItem('mot.mqttPass', els.mqttPass.value);
}

function setStatus(online) {
  els.status.textContent = online ? 'online' : 'offline';
  els.status.classList.toggle('online', online);
  els.status.classList.toggle('offline', !online);
}

function setLastUpdate() {
  els.lastUpdate.textContent = new Date().toLocaleTimeString();
}

function setNumber(el, value, decimals = 1) {
  const n = Number(value);
  el.textContent = Number.isFinite(n) ? n.toFixed(decimals) : '--';
}

function handleTopic(topic, payload) {
  const base = els.baseTopic.value.trim().replace(/\/$/, '');
  const suffix = topic.startsWith(base + '/') ? topic.slice(base.length + 1) : topic;
  const value = payload.toString();

  switch (suffix) {
    case 'display/soc': {
      const soc = Number(value);
      setNumber(els.soc, soc, 1);
      els.socBar.style.width = `${Math.max(0, Math.min(100, soc || 0))}%`;
      break;
    }
    case 'display/range':
      setNumber(els.range, value, 0);
      break;
    case 'display/speed':
      setNumber(els.speed, value, 1);
      break;
    case 'display/odo':
      setNumber(els.odo, value, 1);
      break;
    case 'charging/is_charging':
      els.charging.textContent = value === '1' || value === 'true' ? '⚡ Yes' : 'No';
      break;
  }

  setLastUpdate();
}

function connectMqtt() {
  saveSettings();

  if (client) {
    client.end(true);
    client = null;
  }

  const url = els.mqttUrl.value.trim();
  const base = els.baseTopic.value.trim().replace(/\/$/, '');

  if (!url) {
    alert('Please enter a MQTT WebSocket URL.');
    return;
  }

  client = mqtt.connect(url, {
    username: els.mqttUser.value || undefined,
    password: els.mqttPass.value || undefined,
    reconnectPeriod: 3000,
    connectTimeout: 10000,
  });

  client.on('connect', () => {
    setStatus(true);
    client.subscribe(`${base}/#`);
  });

  client.on('reconnect', () => setStatus(false));
  client.on('offline', () => setStatus(false));
  client.on('close', () => setStatus(false));
  client.on('error', () => setStatus(false));

  client.on('message', handleTopic);
}

els.connectBtn.addEventListener('click', connectMqtt);
els.saveBtn.addEventListener('click', saveSettings);

loadSettings();
setStatus(false);
