let client = null;
let lastUpdate = null;

const $ = (id) => document.getElementById(id);

function setConnection(online) {
  const el = $('connection');
  el.textContent = online ? 'online' : 'offline';
  el.className = 'pill ' + (online ? 'online' : 'offline');
}

function setText(id, value, fallback = '--') {
  $(id).textContent = value === undefined || value === null || value === '' ? fallback : value;
}

function updateFromTopic(prefix, topic, payload) {
  const suffix = topic.startsWith(prefix + '/') ? topic.substring(prefix.length + 1) : topic;
  const value = payload.toString();

  switch (suffix) {
    case 'display/soc': {
      const soc = parseFloat(value);
      setText('soc', Number.isFinite(soc) ? soc.toFixed(1) : '--');
      $('socBar').style.width = Number.isFinite(soc) ? Math.max(0, Math.min(100, soc)) + '%' : '0%';
      break;
    }
    case 'display/speed': setText('speed', parseFloat(value).toFixed(1)); break;
    case 'display/odo': setText('odo', parseFloat(value).toFixed(1)); break;
    case 'display/range': setText('range', value); break;
    case 'charging/is_charging': setText('charging', value === '1' || value === 'true' ? 'Yes' : 'No'); break;
    case 'charging/power_display': setText('power', 'power ' + value); break;
    case 'vehicle/name': setText('vehicle', value); break;
    case 'system/device_id': setText('device', value); break;
    case 'system/firmware': setText('firmware', value); break;
  }

  lastUpdate = new Date();
  $('lastUpdate').textContent = lastUpdate.toLocaleTimeString();
}

function connectMqtt() {
  const url = $('mqttUrl').value.trim();
  const username = $('mqttUser').value.trim();
  const password = $('mqttPass').value;
  const prefix = $('mqttPrefix').value.trim().replace(/\/$/, '');

  localStorage.setItem('mot.mqttUrl', url);
  localStorage.setItem('mot.mqttUser', username);
  localStorage.setItem('mot.mqttPrefix', prefix);

  if (!url || !prefix) {
    alert('Please enter MQTT WebSocket URL and topic prefix.');
    return;
  }

  if (client) {
    client.end(true);
  }

  client = mqtt.connect(url, {
    username: username || undefined,
    password: password || undefined,
    reconnectPeriod: 3000,
    clean: true,
    clientId: 'mot-dashboard-' + Math.random().toString(16).slice(2)
  });

  client.on('connect', () => {
    setConnection(true);
    client.subscribe(prefix + '/#');
  });

  client.on('close', () => setConnection(false));
  client.on('offline', () => setConnection(false));
  client.on('error', (err) => console.warn('MQTT error', err));
  client.on('message', (topic, payload) => updateFromTopic(prefix, topic, payload));
}

function init() {
  $('mqttUrl').value = localStorage.getItem('mot.mqttUrl') || '';
  $('mqttUser').value = localStorage.getItem('mot.mqttUser') || '';
  $('mqttPrefix').value = localStorage.getItem('mot.mqttPrefix') || 'mot/pioneer';
  $('connectBtn').addEventListener('click', connectMqtt);
}

init();
