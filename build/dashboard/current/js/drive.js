(function () {
  const cfg = window.MOT_CONFIG || {};
  const auth = window.MOTAuth?.create({ config: cfg.auth || {} });
  const $ = id => document.getElementById(id);
  const state = { provider: null, vehicleId: null, values: {}, metadata: {}, preferences: {}, forecast: null, journey: null };

  const number = value => { const parsed = Number(value); return Number.isFinite(parsed) ? parsed : NaN; };
  const isTrue = value => value === true || Number(value) === 1 || String(value).toLowerCase() === 'true';
  const text = (id, value) => { if ($(id)) $(id).textContent = value; };
  const tr = value => window.MOT_I18N?.translate?.(value) || value;
  const locale = () => window.MOT_I18N?.locale || cfg.dashboard?.locale || 'de-CH';
  const powerValue = () => {
    const vehicle = number(state.values['bms/vehicle_power_w']);
    const pack = number(state.values['bms/pack_power_w']);
    return Number.isFinite(vehicle) ? vehicle : (Number.isFinite(pack) ? -pack : NaN);
  };

  function renderPower() {
    const watts = powerValue();
    const charging = isTrue(state.values['charging/is_charging']);
    const regen = isTrue(state.values['bms/is_regenerating']) || watts < -100;
    const consumption = isTrue(state.values['bms/is_discharging']) || watts > 100;
    const mode = charging ? 'charging' : regen ? 'regeneration' : consumption ? 'consumption' : 'ready';
    const label = charging ? 'Laden' : regen ? 'Rekuperation' : consumption ? 'Verbrauch' : 'Bereit';
    const kw = Number.isFinite(watts) ? Math.abs(watts) / 1000 : NaN;
    const sign = mode === 'regeneration' || mode === 'charging' ? '+' : '';
    text('drive-power-mode', tr(label));
    text('drive-power', Number.isFinite(kw) ? `${sign}${kw.toLocaleString(locale(), { minimumFractionDigits: 1, maximumFractionDigits: 2 })} kW` : '-- kW');
    document.querySelector('.power-card')?.setAttribute('data-mode', mode);
    const meter = $('drive-power-meter');
    const max = charging ? 3.5 : 20;
    const level = charging ? (kw <= 1.6 ? 'low' : kw <= 2.4 ? 'medium' : 'high')
      : regen ? (kw <= 5 ? 'low' : kw <= 10 ? 'medium' : 'high')
        : (kw <= 3 ? 'low' : kw <= 10 ? 'medium' : 'high');
    meter.dataset.mode = mode; meter.dataset.level = level;
    meter.setAttribute('aria-valuemax', String(max)); meter.setAttribute('aria-valuenow', Number.isFinite(kw) ? kw.toFixed(2) : '0');
    meter.querySelector('i').style.width = `${Number.isFinite(kw) ? Math.min(100, kw / max * 100) : 0}%`;
    const received = number(state.metadata['bms/vehicle_power_w']?.receivedAt || state.metadata['bms/pack_power_w']?.receivedAt);
    text('drive-power-freshness', received && Date.now() - received > 120000
      ? `${tr('Nicht aktuell')} · ${tr('letzter Messpunkt')} ${new Date(received).toLocaleTimeString(locale(), { hour: '2-digit', minute: '2-digit' })}` : '');
  }

  function renderRange() {
    const soc = number(state.values['display/soc']);
    const configured = number(state.preferences.rangeKmAt100 || cfg.vehicle?.defaultRangeKmAt100 || 140) / 100;
    const learned = number(state.forecast?.effectiveKmPerSoc);
    const kmPerSoc = Number.isFinite(learned) ? learned : configured;
    const reserve = Math.max(0, Math.min(50, number(state.preferences.rangeReserveSoc || 0)));
    text('drive-range-zero', Number.isFinite(soc) ? `${Math.round(soc * kmPerSoc)} km` : '-- km');
    text('drive-range-reserve', Number.isFinite(soc) ? `${Math.round(Math.max(0, soc - reserve) * kmPerSoc)} km` : '-- km');
    text('drive-reserve-label', `${tr('bis')} ${reserve.toFixed(0)} % ${tr('Reserve')}`);
  }

  function appendLivePoint() {
    if (!state.journey?.active) return;
    const speed = number(state.values['display/speed_kmh'] ?? state.values['display/speed']);
    const power = powerValue();
    if (!Number.isFinite(speed) && !Number.isFinite(power)) return;
    const now = Date.now();
    const points = state.journey.points ||= [];
    const last = points[points.length - 1];
    if (last && now - last.ts < 4000) return;
    points.push({ ts: now, ...(Number.isFinite(speed) ? { speed } : {}), ...(Number.isFinite(power) ? { power } : {}) });
    if (points.length > 1500) points.splice(0, points.length - 1500);
  }

  function renderChart() {
    const canvas = $('drive-chart'), box = canvas.getBoundingClientRect(), ratio = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.round(box.width * ratio)); canvas.height = Math.max(1, Math.round(box.height * ratio));
    const ctx = canvas.getContext('2d'); ctx.scale(ratio, ratio); ctx.clearRect(0, 0, box.width, box.height);
    const points = state.journey?.points || [];
    const hasJourney = state.journey?.available === true || state.journey?.active === true;
    const historical = hasJourney && state.journey?.active !== true;
    document.querySelector('.journey-card')?.setAttribute('data-history', historical ? 'true' : 'false');
    text('drive-journey-mode', tr(state.journey?.active ? 'Aktuelle Fahrt' : (hasJourney ? 'Letzte Fahrt' : 'Aktuelle Fahrt')));
    text('drive-journey-title', state.journey?.active
      ? `${tr('Seit')} ${new Date(state.journey.startedAt).toLocaleTimeString(locale(), { hour: '2-digit', minute: '2-digit' })}`
      : (hasJourney
        ? `${tr('Beendet um')} ${new Date(state.journey.endedAt).toLocaleTimeString(locale(), { hour: '2-digit', minute: '2-digit' })}`
        : tr('Keine laufende Fahrt')));
    text('drive-speed', `${Number.isFinite(number(state.values['display/speed_kmh'] ?? state.values['display/speed'])) ? Math.round(number(state.values['display/speed_kmh'] ?? state.values['display/speed'])) : '--'} km/h`);
    if (!hasJourney || !points.length) { ctx.fillStyle='#9db0c8';ctx.textAlign='center';ctx.fillText(tr('Noch keine Fahrdaten'),box.width/2,box.height/2);return; }
    const pad=24,w=box.width-pad*2,h=box.height-pad*2,min=state.journey.startedAt,max=Math.max(state.journey.active ? Date.now() : state.journey.endedAt,min+60000);
    const x=ts=>pad+(ts-min)/(max-min)*w, zero=pad+h*.62, powerScale=(h*.35)/20000, speedScale=(h*.5)/Math.max(60,...points.map(p=>number(p.speed)||0));
    ctx.strokeStyle='#29425b';ctx.beginPath();ctx.moveTo(pad,zero);ctx.lineTo(pad+w,zero);ctx.stroke();
    const powerPoints=points.filter(p=>Number.isFinite(number(p.power)));
    const powerY=value=>zero-Math.max(-h*.35,Math.min(h*.35,value*powerScale));
    const powerColor=value=>value<0?'#31df42':value>10000?'#ff5b5b':value>3000?'#f2a72b':'#31df42';
    const powerBarWidth=Math.max(3,Math.min(8,w/Math.max(45,powerPoints.length*1.6)));
    powerPoints.forEach(point=>{
      const value=number(point.power),px=x(point.ts),py=powerY(value);
      ctx.fillStyle=powerColor(value);
      ctx.fillRect(px-powerBarWidth/2,Math.min(zero,py),powerBarWidth,Math.max(2,Math.abs(zero-py)));
    });
    ctx.strokeStyle='#2db8f4';ctx.lineWidth=3;ctx.beginPath();let previous=null;
    points.forEach(p=>{if(!Number.isFinite(number(p.speed)))return;const px=x(p.ts),py=zero-number(p.speed)*speedScale;const gap=previous&&p.ts-previous.ts>150000;if(!previous||gap)ctx.moveTo(px,py);else ctx.lineTo(px,py);previous=p;});ctx.stroke();
  }

  function render() { renderPower(); renderRange(); appendLivePoint(); renderChart(); }
  function onMessage(topic, payload, metadata) {
    const key=topic.split('/').slice(2).join('/'); let value=payload;
    if(payload==='true'||payload==='false')value=payload==='true';else if(payload!==''&&Number.isFinite(Number(payload)))value=Number(payload);else{try{value=JSON.parse(payload);}catch{}}
    state.values[key]=value; if(metadata)state.metadata[key]=metadata; render();
  }
  function mergeJourney(incoming) {
    if (!incoming?.journeyId || incoming.journeyId !== state.journey?.journeyId) return incoming;
    const points = new Map((state.journey.points || []).map(point => [Number(point.ts), { ...point }]));
    (incoming.points || []).forEach(point => {
      const timestamp = Number(point.ts);
      points.set(timestamp, { ...(points.get(timestamp) || {}), ...point, ts: timestamp });
    });
    return { ...state.journey, ...incoming, points: [...points.values()].sort((a, b) => a.ts - b.ts).slice(-1500) };
  }
  function normalizeJourneyHistory(journey) {
    if (!journey) return journey;
    return {
      ...journey,
      points: (journey.points || []).map(point => ({
        ...point,
        // AWS History stores charging/power_signed in 0.1 kW. The live
        // driving value is bms/vehicle_power_w in W; normalize both to W.
        ...(Number.isFinite(number(point.power)) ? { power: number(point.power) * 100 } : {})
      }))
    };
  }
  async function loadJourney() { state.journey=mergeJourney(normalizeJourneyHistory(await state.provider.getCurrentJourney()));render(); }
  async function selectVehicle(vehicleId) {
    state.vehicleId=vehicleId;state.values={};state.metadata={};state.journey=null;await state.provider.selectVehicle(vehicleId);
    const [prefs,forecast,journey]=await Promise.allSettled([state.provider.getNotificationPreferences(),state.provider.getRangeForecast(),state.provider.getCurrentJourney()]);
    state.preferences=prefs.status==='fulfilled'?prefs.value:{};state.forecast=forecast.status==='fulfilled'?forecast.value.rangeForecast:null;state.journey=journey.status==='fulfilled'?mergeJourney(normalizeJourneyHistory(journey.value)):{active:false,points:[]};render();
  }
  async function bootstrap() {
    if(!auth?.isConfigured())throw new Error('Cognito ist nicht konfiguriert.');await auth.restoreSession();
    if(!auth.isAuthenticated()){text('drive-status','Nicht angemeldet');return;}
    state.provider=window.MOTDataProviders.create('aws-backend',{config:{...(cfg.awsBackend||{}),vehicleId:cfg.mqtt?.vehicleId||'pioneer',getAccessToken:auth.getAccessToken,onUnauthorized:()=>location.reload()}});
    const vehicles=await state.provider.getVehicles();if(!vehicles.length)throw new Error('Kein Fahrzeug zugeordnet.');
    const select=$('drive-vehicle');vehicles.forEach(vehicle=>{const option=document.createElement('option');option.value=vehicle.vehicleId;option.textContent=vehicle.vehicleId;select.appendChild(option);});
    $('drive-denied').hidden=true;$('drive-content').hidden=false;text('drive-status','Live · fahrtenbezogen');
    state.provider.start({onMessage,onSnapshot:snapshot=>{state.values={...state.values,...snapshot.values};state.metadata={...state.metadata,...snapshot.metadata};render();},onConnection:connected=>text('drive-status',connected?'Live · fahrtenbezogen':'Verbindung unterbrochen'),onError:console.warn});
    await selectVehicle(vehicles[0].vehicleId);window.setInterval(loadJourney,60000);
  }
  $('drive-login')?.addEventListener('click',()=>auth.login({remember:true}));$('drive-vehicle')?.addEventListener('change',event=>selectVehicle(event.target.value));window.addEventListener('resize',renderChart);
  window.addEventListener('mot-language-change',render);
  bootstrap().catch(error=>{text('drive-status','Nicht verfügbar');text('drive-denied-message',error.message||'Fahrtansicht nicht verfügbar.');});
})();
