(function(){
"use strict";

const DEFAULT_SAMPLE_INTERVAL_MS=60000;
const DEFAULT_RETENTION_DAYS=30;
let lastSample=null;
let lastStoredTs=0;

async function update(telemetry,options={}){
  if(!window.MOTHistoryDB||!telemetry) return;

  const vehicleId=getVehicleId(options);
  const now=Date.now();
  const intervalMs=getConfigNumber("history.sampleIntervalMs",DEFAULT_SAMPLE_INTERVAL_MS);
  const retentionDays=getConfigNumber("history.retentionDays",DEFAULT_RETENTION_DAYS);

  const sample={
    ts:now,
    vehicleId,
    soc:readNumber(telemetry,["display/soc","display.soc","soc"]),
    rangeKm:readNumber(telemetry,["display/estimated_range_km","display/range","display.estimated_range_km","display.range","rangeKm"]),
    speedKmh:readNumber(telemetry,["display/speed_kmh","display/speed","display.speed_kmh","display.speed","speedKmh"]),
    powerDisplay:readNumber(telemetry,["charging/power_display","charging/power_signed","charging.power_display","charging.power_signed","powerDisplay"]),
    isCharging:readBool(telemetry,["charging/is_charging","charging.is_charging","isCharging"]),
    rssi:readNumber(telemetry,["system/rssi","system/wifi_rssi","system.rssi","system.wifi_rssi","rssi"])
  };

  if(!shouldStore(sample,now,intervalMs)) return;

  lastSample=sample;
  lastStoredTs=now;
  await window.MOTHistoryDB.putSample(sample);

  if(retentionDays>0){
    window.MOTHistoryDB
      .deleteOlderThan(vehicleId,now-retentionDays*24*60*60*1000)
      .catch(()=>{});
  }

  window.dispatchEvent(new CustomEvent("mot-history-sample",{detail:sample}));
}

function shouldStore(sample,now,intervalMs){
  if(sample.soc===null&&sample.rangeKm===null&&sample.speedKmh===null) return false;
  if(!lastSample) return true;
  if(now-lastStoredTs>=intervalMs) return true;
  if(sample.soc!==lastSample.soc) return true;
  if(sample.isCharging!==lastSample.isCharging) return true;
  return false;
}

function getVehicleId(options){
  return options.vehicleId||
    window.MOT_CONFIG?.mqtt?.vehicleId||
    window.MOT_CONFIG?.vehicleId||
    window.MOT?.vehicleId||
    window.CONFIG?.vehicleId||
    window.CONFIG?.mqtt?.vehicle||
    "pioneer";
}

function getConfigNumber(path,fallback){
  let v=window.MOT_CONFIG||window.MOT||window.CONFIG||{};
  for(const p of path.split(".")) v=v?.[p];
  const n=Number(v);
  return Number.isFinite(n)?n:fallback;
}

function readPath(obj,path){
  if(Object.prototype.hasOwnProperty.call(obj,path)) return obj[path];
  return path.split(".").reduce((acc,key)=>acc&&acc[key],obj);
}

function readNumber(obj,paths){
  for(const path of paths){
    const value=readPath(obj,path);
    if(value!==undefined&&value!==null&&value!==""){
      const n=Number(value);
      if(Number.isFinite(n)) return n;
    }
  }
  return null;
}

function readBool(obj,paths){
  for(const path of paths){
    const value=readPath(obj,path);
    if(value!==undefined&&value!==null){
      if(value===true||value==="true") return true;
      if(value===false||value==="false") return false;
      return Boolean(Number(value));
    }
  }
  return false;
}

window.MOTHistoryRecorder={update};
})();
