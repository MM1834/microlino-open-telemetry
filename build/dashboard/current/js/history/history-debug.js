(function(){
"use strict";

function vehicleId(){
  return window.MOT_CONFIG?.mqtt?.vehicleId ||
    window.MOT_CONFIG?.vehicleId ||
    window.CONFIG?.vehicleId ||
    window.CONFIG?.mqtt?.vehicle ||
    "pioneer";
}

async function refresh(){
  if(!window.MOTHistoryDB) return;
  const data=await window.MOTHistoryDB.getSamples(vehicleId(),0);
  const oldest=data[0];
  const newest=data[data.length-1];

  set("history-debug-count",data.length);
  set("history-debug-oldest",oldest?new Date(oldest.ts).toLocaleTimeString():"--");
  set("history-debug-newest",newest?new Date(newest.ts).toLocaleTimeString():"--");
  set("history-debug-latest-soc",newest?.soc!=null?`${newest.soc}%`:"--");
}

function set(id,value){
  const el=document.getElementById(id);
  if(el) el.textContent=value;
}

window.addEventListener("DOMContentLoaded",refresh);
window.addEventListener("mot-history-sample",refresh);
window.addEventListener("mot-history-cleared",refresh);
window.MOTHistoryDebug={refresh};
})();
