const state = { values: {}, client: null, connected: false };
const $ = (id) => document.getElementById(id);
const cfgKey = "mot-dashboard-config";

function loadConfig(){
  const saved = JSON.parse(localStorage.getItem(cfgKey) || "{}");
  return { ...window.MOT_DEFAULT_CONFIG, ...saved };
}
function saveConfig(){
  const cfg = { mqttUrl: $("mqttUrl").value.trim(), topicPrefix: $("topicPrefix").value.trim().replace(/\/$/, ""), username: $("mqttUser").value, password: $("mqttPass").value };
  localStorage.setItem(cfgKey, JSON.stringify(cfg));
  return cfg;
}
function applyConfig(cfg){
  $("mqttUrl").value = cfg.mqttUrl || "";
  $("topicPrefix").value = cfg.topicPrefix || "";
  $("mqttUser").value = cfg.username || "";
  $("mqttPass").value = cfg.password || "";
}
function setBadge(online){
  const b=$("connectionBadge"); b.textContent = online ? "MQTT Online" : "Offline"; b.className = online ? "badge badge-online" : "badge badge-offline";
}
function setText(id, value, decimals=1){
  if(value === undefined || value === null || value === "") { $(id).textContent="--"; return; }
  const n = Number(value); $(id).textContent = Number.isFinite(n) ? n.toFixed(decimals) : String(value);
}
function updateUi(){
  setText("socValue", state.values["display/soc"], 1);
  const soc = Number(state.values["display/soc"] || 0); $("socBar").style.width = `${Math.max(0, Math.min(100, soc))}%`;
  setText("rangeValue", state.values["display/range"], 0);
  setText("speedValue", state.values["display/speed"], 1);
  setText("odoValue", state.values["display/odo"], 1);
  const charging = String(state.values["charging/is_charging"] || "0") === "1" || String(state.values["charging/is_charging"]).toLowerCase() === "true";
  const c=$("chargingValue"); c.textContent = charging ? "⚡ Charging" : "Idle"; c.className = charging ? "status-pill status-charging" : "status-pill status-idle";
  $("lastUpdateValue").textContent = state.values.__lastUpdate || "--";
  $("rawValues").textContent = JSON.stringify(state.values, null, 2);
}
function connect(){
  const cfg = saveConfig();
  if(state.client){ state.client.end(true); state.client = null; }
  const options = { clientId: `mot-dashboard-${Math.random().toString(16).slice(2)}`, username: cfg.username || undefined, password: cfg.password || undefined, reconnectPeriod: 3000, connectTimeout: 10000 };
  state.client = mqtt.connect(cfg.mqttUrl, options);
  state.client.on("connect", () => { state.connected=true; setBadge(true); state.client.subscribe(`${cfg.topicPrefix}/#`); });
  state.client.on("close", () => { state.connected=false; setBadge(false); });
  state.client.on("error", () => setBadge(false));
  state.client.on("message", (topic, payload) => {
    const prefix = cfg.topicPrefix + "/";
    if(!topic.startsWith(prefix)) return;
    const key = topic.slice(prefix.length);
    state.values[key] = payload.toString();
    state.values.__lastUpdate = new Date().toLocaleTimeString();
    updateUi();
  });
}
function disconnect(){ if(state.client){ state.client.end(true); state.client = null; } setBadge(false); }

window.addEventListener("load", () => {
  applyConfig(loadConfig());
  $("saveBtn").addEventListener("click", saveConfig);
  $("connectBtn").addEventListener("click", connect);
  $("disconnectBtn").addEventListener("click", disconnect);
  setBadge(false); updateUi();
  if("serviceWorker" in navigator) navigator.serviceWorker.register("service-worker.js").catch(()=>{});
});
