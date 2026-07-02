(() => {
  const cfg = window.MOT_CONFIG || {};
  const $ = (id) => document.getElementById(id);
  const state = {
    mqttConnected: false,
    lastUpdate: null,
    socHistory: [],
    telemetry: {
      display: {}, charging: {}, system: {}, battery: {}, location: {}, cells: []
    }
  };

  const baseTopic = `${trimSlash(cfg.topicPrefix || 'mot')}/${cfg.vehicleId || 'pioneer'}`;
  $('vehicle-title').textContent = cfg.vehicleName || 'Microlino Pioneer';
  $('side-vehicle').textContent = cfg.vehicleId || 'pioneer';
  $('side-topic').textContent = `${baseTopic}/...`;

  function trimSlash(s){ return String(s).replace(/^\/+|\/+$/g,''); }
  function setText(id, value, fallback='--'){ const el=$(id); if(el) el.textContent = value ?? fallback; }
  function num(v){ const n = Number(v); return Number.isFinite(n) ? n : null; }
  function bool(v){ return v === true || v === 'true' || v === '1' || v === 1; }
  function fmt(n, d=0){ n=num(n); return n===null ? '--' : n.toFixed(d); }
  function nowString(){ return new Date().toLocaleTimeString('de-CH',{hour:'2-digit',minute:'2-digit',second:'2-digit'}); }
  function setMqtt(online, label){
    state.mqttConnected = online;
    ['mqtt-dot'].forEach(id => $(id)?.classList.toggle('off', !online));
    setText('mqtt-text', label || (online?'Verbunden':'Offline'));
    setText('top-status', label || (online?'Online':'Offline'));
    setText('side-mqtt', online?'Verbunden':'Offline');
    document.body.classList.toggle('offline', !online);
  }

  function clock(){
    const d = new Date();
    setText('date', d.toLocaleDateString('de-CH'));
    setText('clock', d.toLocaleTimeString('de-CH',{hour:'2-digit',minute:'2-digit',second:'2-digit'}));
  }
  setInterval(clock,1000); clock();

  $('theme')?.addEventListener('click', () => document.body.classList.toggle('light'));

  function updateFromTopic(topic, payload){
    const suffix = topic.startsWith(baseTopic + '/') ? topic.slice(baseTopic.length + 1) : topic;
    const v = String(payload).trim();
    const t = state.telemetry;
    state.lastUpdate = new Date();
    setText('side-updated', nowString()); setText('map-updated', nowString());

    switch(suffix){
      case 'display/soc': t.display.soc = num(v); break;
      case 'display/speed_kmh': case 'display/speed': t.display.speedKmh = num(v); break;
      case 'display/odometer_km': case 'display/odo': t.display.odometerKm = num(v); break;
      case 'display/estimated_range_km': case 'display/range': t.display.estimatedRangeKm = num(v); break;
      case 'charging/is_charging': t.charging.isCharging = bool(v); break;
      case 'charging/power_display': t.charging.powerDisplay = num(v); break;
      case 'charging/power_signed': t.charging.powerSigned = num(v); break;
      case 'battery/pack_voltage': t.battery.voltage = num(v); break;
      case 'battery/pack_current': t.battery.current = num(v); break;
      case 'battery/pack_power': t.battery.power = num(v); break;
      case 'battery/temperature_min': t.battery.tempMin = num(v); break;
      case 'battery/temperature_max': t.battery.tempMax = num(v); break;
      case 'battery/temperature_avg': t.battery.tempAvg = num(v); break;
      case 'system/firmware_version': t.system.firmware = v; break;
      case 'system/device_id': t.system.deviceId = v; break;
      case 'system/network_mode': t.system.networkMode = v; break;
      case 'system/ip_address': t.system.ipAddress = v; break;
      case 'system/wifi_rssi': t.system.rssi = num(v); break;
      case 'system/uptime_sec': t.system.uptime = num(v); break;
      case 'system/12v_voltage': t.system.v12 = num(v); break;
      case 'location/lat': t.location.lat = num(v); break;
      case 'location/lng': t.location.lng = num(v); break;
      case 'location/address': t.location.address = v; break;
      default:
        if (suffix.startsWith('cells/')) handleCell(suffix, v);
        break;
    }
    render();
  }

  function handleCell(suffix, v){
    const m = suffix.match(/^cells\/(\d+)$/);
    if(m){ state.telemetry.cells[Number(m[1])-1] = num(v); }
  }

  function render(){
    const t = state.telemetry;
    const soc = num(t.display.soc);
    const speed = num(t.display.speedKmh) || 0;
    const range = num(t.display.estimatedRangeKm) ?? (soc!==null ? soc/100*(cfg.rangeFullKm||140) : null);
    const odo = num(t.display.odometerKm);
    const charging = !!t.charging.isCharging;
    const power = num(t.charging.powerSigned) ?? num(t.charging.powerDisplay);
    const p = Math.max(0, Math.min(100, soc ?? 0));

    ['soc-ring','soc-ring-2'].forEach(id => $(id)?.style.setProperty('--p', p));
    setText('hero-soc', soc===null?'--':Math.round(soc)+'%');
    setText('soc-main', soc===null?'--':Math.round(soc)+'%');
    setText('hero-range', range===null?'-- km':`${Math.round(range)} km`);
    setText('hero-speed', fmt(speed,0));
    setText('speed-main', fmt(speed,0));
    setText('hero-odo', odo===null?'-- km':`${fmt(odo,1)} km`);
    setText('hero-charge', charging?'Am Laden':'Nicht am Laden');
    setText('charge-state', charging?'Am Laden':'Nicht am Laden');
    setText('status-charging', charging?'Ja':'Nein');
    setText('hero-mode', speed>1?'Modus Driving':'Modus Parked');
    setText('charge-power', power===null?'--':`${fmt(power,1)} W`);
    setText('current', t.battery.current===undefined?'-- A':`${fmt(t.battery.current,1)} A`);
    setText('voltage', t.battery.voltage===undefined?'-- V':`${fmt(t.battery.voltage,1)} V`);
    setText('current-mode', (t.battery.current||0)<0?'Entladung':'');
    setText('charge-current', '--'); setText('charge-voltage','--');
    setText('trip','-- km'); setText('consumption','--'); setText('drive-time','--');
    setText('temp-min', t.battery.tempMin===undefined?'-- °C':`${fmt(t.battery.tempMin,1)} °C`);
    setText('temp-max', t.battery.tempMax===undefined?'-- °C':`${fmt(t.battery.tempMax,1)} °C`);
    setText('temp-avg', t.battery.tempAvg===undefined?'-- °C':`${fmt(t.battery.tempAvg,1)} °C`);
    setText('temp-amb','-- °C');
    setText('firmware', t.system.firmware || '--'); setText('device-id', t.system.deviceId || '--');
    setText('v12', t.system.v12===undefined?'-- V':`${fmt(t.system.v12,1)} V`);
    setText('address', t.location.address || 'Standort nicht verfügbar');
    const lat = t.location.lat ?? cfg.map?.defaultLat; const lng = t.location.lng ?? cfg.map?.defaultLng;
    setText('coords', (lat && lng) ? `${fmt(lat,5)}° N\n${fmt(lng,5)}° E` : '--');
    $('needle').style.transform = `rotate(${-115 + Math.min(120,speed)/120*230}deg)`;
    renderCells(); pushSoc(soc); drawChart();
  }

  function renderCells(){
    let cells = state.telemetry.cells.filter(v => Number.isFinite(v));
    if(!cells.length) cells = Array.from({length:16},()=>3.22);
    const min = Math.min(...cells), max=Math.max(...cells), delta=max-min;
    setText('cell-min', fmt(min,3)); setText('cell-max', fmt(max,3)); setText('cell-delta', fmt(delta,3));
    const bars = $('cell-bars'); bars.innerHTML='';
    cells.slice(0,24).forEach(v => { const b=document.createElement('div'); b.className='bar'; b.style.height=`${Math.max(12, Math.min(100, (v-3.0)/.35*100))}%`; bars.appendChild(b); });
  }

  function pushSoc(soc){
    if(soc===null) return;
    const h=state.socHistory; const last=h[h.length-1];
    if(!last || Date.now()-last.t>30000){ h.push({t:Date.now(),v:soc}); if(h.length>96) h.shift(); }
  }
  function drawChart(){
    const c=$('soc-chart'); if(!c) return; const ctx=c.getContext('2d'); const w=c.width,h=c.height;
    ctx.clearRect(0,0,w,h); ctx.strokeStyle='rgba(255,255,255,.13)'; ctx.lineWidth=1; ctx.font='12px sans-serif'; ctx.fillStyle='rgba(220,235,255,.75)';
    for(let i=0;i<=4;i++){ const y=20+i*(h-40)/4; ctx.beginPath(); ctx.moveTo(45,y); ctx.lineTo(w-16,y); ctx.stroke(); ctx.fillText(`${100-i*25}%`,6,y+4); }
    let data=state.socHistory.length?state.socHistory:Array.from({length:10},(_,i)=>({v:90-i*3+(i%3)*5}));
    ctx.beginPath(); data.forEach((p,i)=>{ const x=45+i*(w-65)/Math.max(1,data.length-1); const y=20+(100-p.v)*(h-40)/100; i?ctx.lineTo(x,y):ctx.moveTo(x,y); });
    ctx.strokeStyle='#42e043'; ctx.lineWidth=3; ctx.stroke();
    const grad=ctx.createLinearGradient(0,30,0,h); grad.addColorStop(0,'rgba(66,224,67,.28)'); grad.addColorStop(1,'rgba(66,224,67,0)'); ctx.lineTo(w-16,h-20); ctx.lineTo(45,h-20); ctx.fillStyle=grad; ctx.fill();
  }

  function connect(){
    const mqttLib = window.mqtt || window.MQTT || window.Mqtt;
    if(!mqttLib || typeof mqttLib.connect !== 'function'){ setMqtt(false,'mqtt.js fehlt'); return; }
    const proto = cfg.useTls ? 'wss' : 'ws'; const url = `${proto}://${cfg.host}:${cfg.port}${cfg.path || '/'}`;
    const clientId = `motdash_${Math.random().toString(16).slice(2,10)}`;
    const client = mqttLib.connect(url,{clientId,username:cfg.username||undefined,password:cfg.password||undefined,keepalive:30,reconnectPeriod:3000,connectTimeout:10000,clean:true});
    client.on('connect',()=>{ setMqtt(true,'Online'); client.subscribe(`${baseTopic}/#`); });
    client.on('reconnect',()=>setMqtt(false,'Reconnecting…'));
    client.on('offline',()=>setMqtt(false,'Offline'));
    client.on('error',(e)=>{ console.error('MQTT',e); setMqtt(false,'MQTT Fehler'); });
    client.on('message',(topic,payload)=>updateFromTopic(topic,payload.toString()));
  }

  renderCells(); drawChart(); render(); connect();
})();
