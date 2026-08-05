const IOBROKER_DOT_PREFIX = 'mqtt.3.mot.xrpioneer.';
const IOBROKER_MQTT_PREFIX = 'mqtt/3/mot/xrpioneer/';
const TARGET_PREFIX = 'mot/xrpioneer/';

const TYPES = {
  'charging/is_charging': 'boolean',
  'charging/plugged': 'boolean',
  'charging/power_display': 'number',
  'display/estimated_range_km': 'number',
  'display/odometer_km': 'number',
  'display/soc': 'number',
  'display/speed_kmh': 'number',
  'system/device_id': 'string',
  'system/device_name': 'string',
  'system/firmware_version': 'string',
  'system/ip_address': 'string',
  'system/mqtt_client_id': 'string',
  'system/network_mode': 'string',
  'system/uptime_sec': 'number',
  'system/wifi_rssi': 'number'
};

function reject(reason) {
  const count = (context.get('rejected') || 0) + 1;
  context.set('rejected', count);
  node.status({fill: 'yellow', shape: 'ring', text: `blocked ${count}: ${reason}`});
  return [null, {
    payload: {
      event: 'mot_forward_blocked',
      reason,
      topic: String(msg.topic || '')
    }
  }];
}

if (typeof msg.topic !== 'string') {
  return reject('source-prefix');
}

let suffix;
if (msg.topic.startsWith(IOBROKER_MQTT_PREFIX)) {
  suffix = msg.topic.slice(IOBROKER_MQTT_PREFIX.length);
} else if (msg.topic.startsWith(IOBROKER_DOT_PREFIX)) {
  suffix = msg.topic.slice(IOBROKER_DOT_PREFIX.length).replaceAll('.', '/');
} else {
  return reject('source-prefix');
}
const type = TYPES[suffix];
if (!type) return reject('topic-not-allowlisted');

let value = Buffer.isBuffer(msg.payload) ? msg.payload.toString('utf8') : msg.payload;
if (typeof value === 'string') value = value.trim();

if (type === 'boolean') {
  if (value === true || value === 1 || /^(1|true|on|yes)$/i.test(String(value))) value = true;
  else if (value === false || value === 0 || /^(0|false|off|no)$/i.test(String(value))) value = false;
  else return reject('invalid-boolean');
} else if (type === 'number') {
  value = Number(value);
  if (!Number.isFinite(value)) return reject('invalid-number');
} else {
  value = String(value);
  if (!value.length || value.length > 256) return reject('invalid-string');
}

msg.topic = TARGET_PREFIX + suffix;
msg.payload = value;
msg.qos = 0;
msg.retain = true;
msg._motForwarded = true;
const count = (context.get('forwarded') || 0) + 1;
context.set('forwarded', count);
node.status({fill: 'green', shape: 'dot', text: `forwarded ${count}: ${suffix}`});
return [msg, null];
