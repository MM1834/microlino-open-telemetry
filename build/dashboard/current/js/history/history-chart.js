(function(){
"use strict";
let currentRangeHours=24;
const RANGE_STORAGE_KEY="mot-history-range";
const RANGE_STORAGE_TTL_MS=7*24*60*60*1000;
const ALLOWED_RANGES=[24,168,720];

async function init(){
  if(!document.getElementById("soc-history-chart")) return;
  currentRangeHours=restoreRange();
  bindButtons();
  updateRangeButtons(currentRangeHours);
  await render(currentRangeHours);
  window.addEventListener("mot-history-sample",()=>render(currentRangeHours));
  window.addEventListener("mot-history-cleared",()=>render(currentRangeHours));
}

async function render(hours=24){
  currentRangeHours=hours;
  const vehicleId=getVehicleId();
  let samples=[];
  let resolutionSeconds=null;
  try{
    if(window.MOTHistorySource?.getHistory){
      const result=await window.MOTHistorySource.getHistory(hours);
      samples=Array.isArray(result?.points)?result.points:[];
      resolutionSeconds=result?.resolutionSeconds||null;
    }else if(window.MOTHistoryDB){
      samples=await window.MOTHistoryDB.getSamples(vehicleId,Date.now()-hours*3600000);
    }
  }catch(error){
    console.error("MOT history request failed",error);
  }

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
      return {...s,power:Math.abs(signedPower),_signedPower:signedPower,_powerMode:powerMode(signedPower,s)};
    })
    .sort((a,b)=>a.ts-b.ts);
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
    min:0,
    max:null,
    color:"#f59e0b",
    decimals:1,
    rangeHours:hours,
    latestPoint:powerSource[powerSource.length-1],
    formatValue:(value,point)=>`${point?._powerMode||"Leistung"} ${Number(value).toFixed(1)}`
  });

  renderSeries({
    canvasId:"charging-history-chart",
    emptyId:"charging-history-empty",
    samples,
    field:"charging",
    label:"Laden",
    unit:"",
    min:0,
    max:1,
    color:"#a855f7",
    rangeHours:hours,
    formatValue:value=>Number(value)>=0.5?"Ja":"Nein"
  });

  renderSeries({
    canvasId:"plugged-history-chart",
    emptyId:"plugged-history-empty",
    samples,
    field:"plugged",
    label:"Kabel angeschlossen",
    unit:"",
    min:0,
    max:1,
    color:"#ec4899",
    rangeHours:hours,
    formatValue:value=>Number(value)>=0.5?"Ja":"Nein"
  });

  const meta=document.getElementById("soc-history-meta");
  if(meta){
    const charging=samples.filter(s=>s.charging===true).length;
    const plugged=samples.filter(s=>s.plugged===true).length;
    const resolution=resolutionSeconds?` · Auflösung ${formatResolution(resolutionSeconds)}`:"";
    meta.textContent=`${samples.length} Punkte · ${vehicleId} · letzte ${Math.round(hours/24)}d${resolution} · Laden ${charging} · Kabel ${plugged} Intervalle`;
  }
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
      if(Number(current[field])>0&&restartTs>lastClosed.ts){
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

function powerMode(signedPower,sample){
  if(Math.abs(signedPower)<0.05) return "Leistung";
  if(signedPower>0) return "Verbrauch";
  return sample.charging===true||Number(sample.charging)>=0.5?"Laden":"Rekuperation";
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
  let maxY=o.max ?? Math.max(...vals,1);
  if(maxY<=minY) maxY=minY+1;
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
    ctx.fillText((maxY-(i*(maxY-minY)/4)).toFixed(0),6,y+4);
  }

  const minTs=points[0].ts;
  const maxTs=points[points.length-1].ts||minTs+1;
  const x=ts=>L+((ts-minTs)/Math.max(1,maxTs-minTs))*PW;
  const y=v=>T+(1-(v-minY)/(maxY-minY))*PH;

  const gradient=ctx.createLinearGradient(0,T,0,h-B);
  gradient.addColorStop(0,o.color+"55");
  gradient.addColorStop(1,o.color+"00");

  ctx.beginPath();
  points.forEach((p,i)=>{
    const px=x(p.ts), py=y(Number(p[o.field]));
    if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);
  });
  ctx.lineTo(x(points[points.length-1].ts),h-B);
  ctx.lineTo(x(points[0].ts),h-B);
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

window.MOTHistoryChart={init,render,closeInactiveGaps};
})();
