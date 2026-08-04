(function(){
"use strict";
let currentRangeHours=24;

async function init(){
  if(!document.getElementById("soc-history-chart")) return;
  bindButtons();
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
    color:"#38bdf8"
  });

  renderSeries({
    canvasId:"speed-history-chart",
    emptyId:"speed-history-empty",
    samples,
    field:samples.some(s=>s.speed!==undefined)?"speed":"speedKmh",
    label:"Speed",
    unit:"km/h",
    min:0,
    max:null,
    color:"#22c55e"
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
  ctx.fillStyle="rgba(226,232,240,.95)";
  ctx.font="13px system-ui";
  const latestValue=o.formatValue?o.formatValue(last[o.field]):Number(last[o.field]).toFixed(0);
  ctx.fillText(`${o.label}: ${latestValue}${o.unit?" "+o.unit:""}`,L,T+14);

  ctx.fillStyle="rgba(148,163,184,.9)";
  ctx.font="11px system-ui";
  ctx.fillText(new Date(first.ts).toLocaleTimeString([],{hour:"2-digit",minute:"2-digit"}),L,h-8);
  ctx.fillText(new Date(last.ts).toLocaleTimeString([],{hour:"2-digit",minute:"2-digit"}),Math.max(L,w-80),h-8);
}

function bindButtons(){
  document.querySelectorAll("[data-history-range]").forEach(btn=>btn.addEventListener("click",()=>{
    document.querySelectorAll("[data-history-range]").forEach(b=>b.classList.remove("active"));
    btn.classList.add("active");
    render(Number(btn.dataset.historyRange));
  }));
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

window.MOTHistoryChart={init,render};
})();
