(function(){
"use strict";
let currentRangeHours=24;
const RANGE_STORAGE_KEY="mot-history-range";
const RANGE_STORAGE_TTL_MS=7*24*60*60*1000;
const ALLOWED_RANGES=[24,168,720];
let latestRenderRequest=0;
let inFlightHistory=null;
let inFlightHistoryKey="";

async function init(){
  if(!document.getElementById("soc-history-chart")) return;
  currentRangeHours=restoreRange();
  bindButtons();
  updateRangeButtons(currentRangeHours);
  await render(currentRangeHours);
  window.addEventListener("mot-history-sample",()=>render(currentRangeHours));
  window.addEventListener("mot-history-cleared",()=>render(currentRangeHours));
}

async function render(hours=currentRangeHours){
  const requestedHours=Number(hours);
  hours=ALLOWED_RANGES.includes(requestedHours)?requestedHours:currentRangeHours;
  currentRangeHours=hours;
  updateRangeButtons(hours);
  const vehicleId=getVehicleId();
  const requestId=++latestRenderRequest;
  const requestKey=`${vehicleId}|${hours}`;
  let samples=[];
  let resolutionSeconds=null;
  let rangeForecast=null;
  let pendingHistory=null;
  try{
    if(!inFlightHistory||inFlightHistoryKey!==requestKey){
      inFlightHistoryKey=requestKey;
      inFlightHistory=loadHistory(vehicleId,hours);
    }
    pendingHistory=inFlightHistory;
    const result=await pendingHistory;
    if(inFlightHistory===pendingHistory){
      inFlightHistory=null;
      inFlightHistoryKey="";
    }
    if(requestId!==latestRenderRequest) return false;
    if(result){
      samples=Array.isArray(result?.points)?result.points:[];
      resolutionSeconds=result?.resolutionSeconds||null;
      rangeForecast=result?.rangeForecast||null;
    }
  }catch(error){
    if(inFlightHistory===pendingHistory){
      inFlightHistory=null;
      inFlightHistoryKey="";
    }
    if(requestId!==latestRenderRequest) return false;
    console.error("MOT history request failed",error);
    updateRequestStatus("History-Aktualisierung fehlgeschlagen · letzte Daten bleiben sichtbar",true);
    return false;
  }
  updateRequestStatus("",false);
  window.dispatchEvent(new CustomEvent("mot-range-forecast",{detail:rangeForecast}));

  renderSeries({
    canvasId:"soc-history-chart",
    emptyId:"soc-history-empty",
    samples,
    field:"soc",
    label:"SoC",
    unit:"%",
    min:0,
    max:100,
    color:"#38bdf8",
    rangeHours:hours
  });

  const speedField=samples.some(s=>s.speed!==undefined)?"speed":"speedKmh";
  updateSpeedStatus(samples,speedField,resolutionSeconds);
  const speedSamples=closeInactiveGaps(samples,speedField,resolutionSeconds);
  renderSeries({
    canvasId:"speed-history-chart",
    emptyId:"speed-history-empty",
    samples:speedSamples,
    field:speedField,
    label:"Ø Speed",
    unit:"km/h",
    min:0,
    max:null,
    color:"#22c55e",
    rangeHours:hours
  });

  const powerSource=samples
    .filter(s=>s.power!==null&&s.power!==undefined&&Number.isFinite(Number(s.power)))
    .map(s=>{
      const signedPower=Number(s.power)/10;
      return {
        ...s,
        power:historyDisplayPower(signedPower),
        _signedPower:signedPower,
        _powerMode:powerMode(signedPower,s)
      };
    })
    .sort((a,b)=>a.ts-b.ts);
  updatePowerStatus(powerSource,resolutionSeconds);
  const powerSamples=closeInactiveGaps(
    powerSource,
    "power",
    resolutionSeconds
  );
  renderSeries({
    canvasId:"power-history-chart",
    emptyId:"power-history-empty",
    samples:powerSamples,
    field:"power",
    label:"Ø Nettoleistung",
    unit:"kW",
    min:null,
    max:null,
    color:"#f59e0b",
    decimals:1,
    rangeHours:hours,
    includeZero:true,
    axisStep:5,
    zeroBaseline:true,
    symmetricAroundZero:true,
    latestPoint:powerSource[powerSource.length-1],
    formatValue:(value,point)=>`${point?._powerMode||"Leistung"} ${formatSignedPower(value)}`
  });

  renderBinaryHistory(samples,hours);

  const meta=document.getElementById("soc-history-meta");
  if(meta){
    const charging=samples.filter(s=>s.charging===true).length;
    const plugged=samples.filter(s=>s.plugged===true).length;
    const resolution=resolutionSeconds?` · Auflösung ${formatResolution(resolutionSeconds)}`:"";
    meta.textContent=`${samples.length} Punkte · ${vehicleId} · letzte ${Math.round(hours/24)}d${resolution} · Laden ${charging} · Kabel ${plugged} Intervalle`;
  }
  return true;
}

async function loadHistory(vehicleId,hours){
  if(window.MOTHistorySource?.getHistory){
    return window.MOTHistorySource.getHistory(hours);
  }
  if(window.MOTHistoryDB){
    const points=await window.MOTHistoryDB.getSamples(vehicleId,Date.now()-hours*3600000);
    return {points};
  }
  return {points:[]};
}

function updateRequestStatus(message,isError){
  const status=document.getElementById("history-request-status");
  if(!status) return;
  status.textContent=message;
  status.classList.toggle("is-error",Boolean(isError));
}

function renderSeries(o){
  const canvas=document.getElementById(o.canvasId);
  const empty=document.getElementById(o.emptyId);
  if(!canvas) return;

  const points=o.samples.filter(s=>s[o.field]!==null&&s[o.field]!==undefined&&Number.isFinite(Number(s[o.field])));

  if(!points.length){
    if(empty) empty.style.display="block";
    canvas.style.display="none";
    return;
  }

  if(empty) empty.style.display="none";
  canvas.style.display="block";
  draw(canvas,points,o);
}

function renderBinaryHistory(samples,rangeHours){
  const canvas=document.getElementById("charging-history-chart");
  const empty=document.getElementById("charging-history-empty");
  if(!canvas) return;
  const series=[
    binarySeries(samples,"charging","Lädt","#a855f7",[]),
    binarySeries(samples,"plugged","Kabel","#ec4899",[8,5])
  ].filter(item=>item.points.length);
  if(!series.length){
    if(empty) empty.style.display="block";
    canvas.style.display="none";
    return;
  }
  if(empty) empty.style.display="none";
  canvas.style.display="block";
  drawBinaryHistory(canvas,series,rangeHours);
}

function binarySeries(samples,field,label,color,dash){
  const points=samples
    .filter(sample=>sample[field]!==null&&sample[field]!==undefined)
    .map(sample=>({ts:Number(sample.ts),value:Number(sample[field])>=0.5?1:0}))
    .filter(point=>Number.isFinite(point.ts))
    .sort((a,b)=>a.ts-b.ts);
  return {field,label,color,dash,points};
}

function binaryStepPath(points,endTs){
  if(!points.length) return [];
  const path=[{ts:points[0].ts,value:points[0].value}];
  for(let i=1;i<points.length;i++){
    const previous=points[i-1];
    const current=points[i];
    path.push({ts:current.ts,value:previous.value});
    path.push({ts:current.ts,value:current.value});
  }
  const last=points[points.length-1];
  if(Number.isFinite(endTs)&&endTs>last.ts) path.push({ts:endTs,value:last.value});
  return path;
}

function drawBinaryHistory(canvas,series,rangeHours){
  const ctx=canvas.getContext("2d");
  const dpr=window.devicePixelRatio||1;
  const w=canvas.clientWidth||600;
  const h=canvas.clientHeight||220;
  canvas.width=w*dpr;
  canvas.height=h*dpr;
  ctx.scale(dpr,dpr);
  ctx.clearRect(0,0,w,h);

  const L=42,R=18,T=28,B=28;
  const PW=w-L-R,PH=h-T-B;
  const timestamps=series.flatMap(item=>item.points.map(point=>point.ts));
  const minTs=Math.min(...timestamps);
  const lastTs=Math.max(...timestamps);
  const maxTs=lastTs>minTs?lastTs:minTs+1;
  const x=ts=>L+((ts-minTs)/Math.max(1,maxTs-minTs))*PW;
  const y=value=>T+(1-Number(value))*PH;

  ctx.strokeStyle="rgba(148,163,184,.25)";
  ctx.lineWidth=1;
  ctx.fillStyle="rgba(148,163,184,.9)";
  ctx.font="12px system-ui";
  ctx.textAlign="left";
  [[1,"Ein"],[0,"Aus"]].forEach(([value,label])=>{
    const py=y(value);
    ctx.beginPath();
    ctx.moveTo(L,py);
    ctx.lineTo(w-R,py);
    ctx.stroke();
    ctx.fillText(label,6,py+4);
  });

  series.forEach(item=>{
    const path=binaryStepPath(item.points,maxTs);
    ctx.beginPath();
    path.forEach((point,index)=>{
      const px=x(point.ts),py=y(point.value);
      if(index===0) ctx.moveTo(px,py); else ctx.lineTo(px,py);
    });
    ctx.strokeStyle=item.color;
    ctx.lineWidth=item.dash.length?2.5:3;
    ctx.setLineDash(item.dash);
    ctx.lineJoin="miter";
    ctx.stroke();
  });
  ctx.setLineDash([]);

  const latest=series.map(item=>{
    const point=item.points[item.points.length-1];
    return `${item.label}: ${point.value?"Ja":"Nein"}`;
  }).join(" · ");
  ctx.fillStyle="rgba(226,232,240,.95)";
  ctx.font="13px system-ui";
  ctx.textAlign="right";
  ctx.fillText(latest,w-R,17);

  ctx.fillStyle="rgba(148,163,184,.9)";
  ctx.font="11px system-ui";
  ctx.textAlign="left";
  ctx.fillText(formatAxisTime(minTs,rangeHours),L,h-8);
  ctx.textAlign="right";
  ctx.fillText(formatAxisTime(lastTs,rangeHours),w-R,h-8);
}

function closeInactiveGaps(samples,field,resolutionSeconds){
  const points=samples
    .filter(s=>s[field]!==null&&s[field]!==undefined&&Number.isFinite(Number(s[field])))
    .map(s=>({...s}))
    .sort((a,b)=>a.ts-b.ts);
  if(!points.length) return points;

  const intervalMs=Math.max(60,Number(resolutionSeconds)||300)*1000;
  const gapThresholdMs=intervalMs*1.5;
  const closed=[points[0]];

  for(let i=1;i<points.length;i++){
    const previous=points[i-1];
    const current=points[i];
    if(current.ts-previous.ts>gapThresholdMs){
      const stopTs=Math.min(previous.ts+intervalMs,current.ts-1000);
      if(Number(previous[field])!==0&&stopTs>previous.ts){
        closed.push({ts:stopTs,[field]:0,_syntheticGap:true});
      }
      const restartTs=current.ts-1000;
      const lastClosed=closed[closed.length-1];
      if(Number(current[field])!==0&&restartTs>lastClosed.ts){
        closed.push({ts:restartTs,[field]:0,_syntheticGap:true});
      }
    }
    closed.push(current);
  }

  const last=closed[closed.length-1];
  if(Number(last[field])!==0&&Date.now()-last.ts>gapThresholdMs){
    closed.push({ts:last.ts+intervalMs,[field]:0,_syntheticGap:true});
  }
  return closed;
}

function updateSpeedStatus(samples,field,resolutionSeconds){
  const status=document.getElementById("speed-history-status");
  if(!status) return;
  const points=samples
    .filter(s=>s[field]!==null&&s[field]!==undefined&&Number.isFinite(Number(s[field])))
    .sort((a,b)=>a.ts-b.ts);
  if(!points.length){
    status.textContent="Nicht aktuell · noch kein Messpunkt";
    status.classList.add("is-stale");
    return;
  }
  const intervalMs=Math.max(60,Number(resolutionSeconds)||300)*1000;
  const last=points[points.length-1];
  const stale=Date.now()-last.ts>intervalMs*1.5;
  const time=new Date(last.ts).toLocaleTimeString([],{hour:"2-digit",minute:"2-digit"});
  status.textContent=stale
    ?`Nicht aktuell · letzter Messpunkt ${time} (Stillstand oder offline)`
    :`Aktuell · letzter Messpunkt ${time}`;
  status.classList.toggle("is-stale",stale);
}

function updatePowerStatus(points,resolutionSeconds){
  const status=document.getElementById("power-history-status");
  if(!status) return;
  if(!points.length){
    status.textContent="Nicht aktuell · noch kein Messpunkt";
    status.classList.add("is-stale");
    return;
  }
  const intervalMs=Math.max(60,Number(resolutionSeconds)||300)*1000;
  const last=points[points.length-1];
  const stale=Date.now()-last.ts>intervalMs*1.5;
  const time=new Date(last.ts).toLocaleTimeString([],{hour:"2-digit",minute:"2-digit"});
  status.textContent=stale
    ?`Nicht aktuell · letzter Messpunkt ${time} (Stillstand oder offline)`
    :`Aktuell · letzter Messpunkt ${time}`;
  status.classList.toggle("is-stale",stale);
}

function powerMode(signedPower,sample){
  if(Math.abs(signedPower)<0.05) return "Leistung";
  if(signedPower>0) return "Verbrauch";
  return sample.charging===true||Number(sample.charging)>=0.5?"Laden":"Rekuperation";
}

function historyDisplayPower(signedPower){
  const value=-Number(signedPower);
  return Object.is(value,-0)?0:value;
}

function formatSignedPower(value){
  const numeric=Number(value);
  if(Math.abs(numeric)<0.05) return "0.0";
  return `${numeric>0?"+":"−"}${Math.abs(numeric).toFixed(1)}`;
}

function draw(canvas,points,o){
  const ctx=canvas.getContext("2d");
  const dpr=window.devicePixelRatio||1;
  const w=canvas.clientWidth||600;
  const h=canvas.clientHeight||220;

  canvas.width=w*dpr;
  canvas.height=h*dpr;
  ctx.scale(dpr,dpr);
  ctx.clearRect(0,0,w,h);

  const L=42,R=18,T=18,B=28;
  const PW=w-L-R,PH=h-T-B;
  const vals=points.map(p=>Number(p[o.field]));
  const minY=o.min ?? Math.min(...vals);
  let chartMinY=o.includeZero?Math.min(minY,0):minY;
  let maxY=o.max ?? Math.max(...vals,1);
  if(o.includeZero) maxY=Math.max(maxY,0);
  if(o.axisStep){
    chartMinY=Math.floor(chartMinY/o.axisStep)*o.axisStep;
    maxY=Math.ceil(maxY/o.axisStep)*o.axisStep;
  }
  if(o.symmetricAroundZero){
    const extent=Math.max(Math.abs(chartMinY),Math.abs(maxY),o.axisStep||1);
    chartMinY=-extent;
    maxY=extent;
  }
  if(maxY<=chartMinY){
    const fallback=o.axisStep||1;
    chartMinY=o.includeZero?-fallback:chartMinY;
    maxY=chartMinY+fallback*2;
  }
  if(o.field==="speedKmh"||o.field==="speed") maxY=Math.max(20,Math.ceil(maxY/10)*10);

  ctx.strokeStyle="rgba(148,163,184,.25)";
  ctx.lineWidth=1;
  ctx.fillStyle="rgba(148,163,184,.9)";
  ctx.font="12px system-ui";

  for(let i=0;i<=4;i++){
    const y=T+i*(PH/4);
    ctx.beginPath();
    ctx.moveTo(L,y);
    ctx.lineTo(w-R,y);
    ctx.stroke();
    ctx.fillText((maxY-(i*(maxY-chartMinY)/4)).toFixed(0),6,y+4);
  }

  const minTs=points[0].ts;
  const maxTs=points[points.length-1].ts||minTs+1;
  const x=ts=>L+((ts-minTs)/Math.max(1,maxTs-minTs))*PW;
  const y=v=>T+(1-(v-chartMinY)/(maxY-chartMinY))*PH;

  if(o.zeroBaseline&&chartMinY<=0&&maxY>=0){
    const zeroY=y(0);
    ctx.beginPath();
    ctx.moveTo(L,zeroY);
    ctx.lineTo(w-R,zeroY);
    ctx.strokeStyle="rgba(226,232,240,.65)";
    ctx.lineWidth=1.5;
    ctx.stroke();
  }

  const gradient=ctx.createLinearGradient(0,T,0,h-B);
  gradient.addColorStop(0,o.color+"55");
  gradient.addColorStop(1,o.color+"00");

  ctx.beginPath();
  points.forEach((p,i)=>{
    const px=x(p.ts), py=y(Number(p[o.field]));
    if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);
  });
  const fillBaseline=o.zeroBaseline?y(0):h-B;
  ctx.lineTo(x(points[points.length-1].ts),fillBaseline);
  ctx.lineTo(x(points[0].ts),fillBaseline);
  ctx.closePath();
  ctx.fillStyle=gradient;
  ctx.fill();

  ctx.beginPath();
  points.forEach((p,i)=>{
    const px=x(p.ts), py=y(Number(p[o.field]));
    if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);
  });
  ctx.strokeStyle=o.color;
  ctx.lineWidth=3;
  ctx.stroke();

  const first=points[0], last=points[points.length-1];
  const latestPoint=o.latestPoint||last;
  ctx.fillStyle="rgba(226,232,240,.95)";
  ctx.font="13px system-ui";
  ctx.textAlign="right";
  const latestValue=o.formatValue
    ?o.formatValue(latestPoint[o.field],latestPoint)
    :Number(latestPoint[o.field]).toFixed(o.decimals??0);
  ctx.fillText(`${o.label}: ${latestValue}${o.unit?" "+o.unit:""}`,w-R,T+14);

  ctx.fillStyle="rgba(148,163,184,.9)";
  ctx.font="11px system-ui";
  ctx.textAlign="left";
  ctx.fillText(formatAxisTime(first.ts,o.rangeHours),L,h-8);
  ctx.textAlign="right";
  ctx.fillText(formatAxisTime(last.ts,o.rangeHours),w-R,h-8);
}

function formatAxisTime(timestamp,rangeHours=24){
  const date=new Date(timestamp);
  const locale=window.MOT_CONFIG?.dashboard?.locale||"de-CH";
  if(rangeHours<=24){
    return date.toLocaleTimeString(locale,{hour:"2-digit",minute:"2-digit"});
  }
  if(rangeHours<=168){
    return date.toLocaleDateString(locale,{weekday:"short",day:"2-digit",month:"2-digit"});
  }
  return date.toLocaleDateString(locale,{day:"2-digit",month:"2-digit",year:"numeric"});
}

function bindButtons(){
  document.querySelectorAll("[data-history-range]").forEach(btn=>btn.addEventListener("click",()=>{
    const hours=Number(btn.dataset.historyRange);
    updateRangeButtons(hours);
    storeRange(hours);
    render(hours);
  }));
}

function updateRangeButtons(hours){
  document.querySelectorAll("[data-history-range]").forEach(btn=>{
    btn.classList.toggle("active",Number(btn.dataset.historyRange)===hours);
  });
}

function restoreRange(){
  try{
    const stored=JSON.parse(localStorage.getItem(RANGE_STORAGE_KEY)||"null");
    const hours=Number(stored?.hours);
    if(ALLOWED_RANGES.includes(hours)&&Number(stored?.expiresAt)>Date.now()) return hours;
    localStorage.removeItem(RANGE_STORAGE_KEY);
  }catch(_error){
    // Storage can be unavailable in restrictive browser/privacy modes.
  }
  return 24;
}

function storeRange(hours){
  if(!ALLOWED_RANGES.includes(hours)) return;
  try{
    localStorage.setItem(RANGE_STORAGE_KEY,JSON.stringify({
      hours,
      expiresAt:Date.now()+RANGE_STORAGE_TTL_MS
    }));
  }catch(_error){
    // The chart remains functional even when browser storage is unavailable.
  }
}

function getVehicleId(){
  return window.MOTHistorySource?.getVehicleId?.()||
    window.MOT_CONFIG?.mqtt?.vehicleId||
    window.MOT_CONFIG?.vehicleId||
    window.MOT?.vehicleId||
    window.CONFIG?.vehicleId||
    window.CONFIG?.mqtt?.vehicle||
    "pioneer";
}

function formatResolution(seconds){
  return seconds<3600?`${Math.round(seconds/60)} min`:`${Math.round(seconds/3600)} h`;
}

window.MOTHistoryChart={init,render,closeInactiveGaps,binaryStepPath,historyDisplayPower,formatSignedPower};
})();
