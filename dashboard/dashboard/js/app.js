(function () {
  const cfg = window.MOT_CONFIG || {};
  const state = {
    display: {},
    charging: {},
    system: {},
    lastUpdate: null
  };

  const $ = (id) => document.getElementById(id);

  function setText(id, value) {
    const el = $(id);
    if (el) el.textContent = value;
  }

  function parseBool(value) {
    if (typeof value === "boolean") return value;
    const v = String(value).trim().toLowerCase();
    return v === "1" || v === "true" || v === "yes" || v === "on";
  }

  function parseNumber(value) {
    const n = Number(String(value).replace(",", "."));
    return Number.isFinite(n) ? n : null;
  }

  function topicBase() {
    return `${cfg.topicPrefix || "mot"}/${cfg.vehicleId || "pioneer"}`;
  }

  function websocketUrl() {
    const proto = cfg.useTls ? "wss" : "ws";
    const host = cfg.host || location.hostname;
    const port = cfg.port ? `:${cfg.port}` : "";
    const path = cfg.omitPath ? "" : (cfg.path || "/mqtt");
    return `${proto}://${host}${port}${path}`;
  }

  function log(line) {
    const el = $("mqttLog");
    if (!el) return;
    const time = new Date().toLocaleTimeString();
    el.textContent = `[${time}] ${line}\n` + el.textContent;
    const lines = el.textContent.split("\n");
    if (lines.length > 80) el.textContent = lines.slice(0, 80).join("\n");
  }

  function render() {
    const soc = parseNumber(state.display.soc);
    if (soc !== null) {
      const bounded = Math.max(0, Math.min(100, soc));
      setText("socValue", bounded.toFixed(1));
      $("socBar").style.width = `${bounded}%`;
      $("socRingFill").style.background = `conic-gradient(var(--accent) ${bounded * 3.6}deg, var(--surface-3) 0deg)`;
    }

    setText("speedValue", state.display.speed !== undefined ? Number(state.display.speed).toFixed(1) : "--");
    setText("odoValue", state.display.odo !== undefined ? Number(state.display.odo).toFixed(1) : "--");
    setText("rangeValue", state.display.range !== undefined ? state.display.range : "--");

    if (state.charging.is_charging !== undefined) {
      const charging = parseBool(state.charging.is_charging);
      setText("chargingValue", charging ? "⚡ Charging" : "Not charging");
      $("chargingValue").className = charging ? "good" : "";
    }

    setText("powerValue", state.charging.power_display !== undefined ? state.charging.power_display : "--");
    setText("vehicleValue", cfg.vehicleId || "--");
    setText("brokerValue", `${cfg.useTls ? "wss" : "ws"}://${cfg.host || ""}:${cfg.port || ""}`);
    setText("topicValue", `${topicBase()}/#`);
    setText("deviceValue", state.system.device_id || "--");
    setText("firmwareValue", state.system.firmware || state.system.firmware_version || "--");
    setText("ipValue", state.system.ip || state.system.ip_address || "--");
    setText("rssiValue", state.system.rssi !== undefined ? `${state.system.rssi} dBm` : "--");
    setText("lastUpdate", state.lastUpdate ? state.lastUpdate.toLocaleTimeString() : "never");
  }

  function applyTopic(topic, payload) {
    const base = topicBase() + "/";
    let rel = topic.startsWith(base) ? topic.slice(base.length) : topic;

    const map = {
      "display/soc": ["display", "soc"],
      "display/speed": ["display", "speed"],
      "display/speed_kmh": ["display", "speed"],
      "display/odo": ["display", "odo"],
      "display/odometer_km": ["display", "odo"],
      "display/range": ["display", "range"],
      "display/estimated_range_km": ["display", "range"],
      "charging/is_charging": ["charging", "is_charging"],
      "charging/power_display": ["charging", "power_display"],
      "system/ip": ["system", "ip"],
      "system/ip_address": ["system", "ip_address"],
      "system/rssi": ["system", "rssi"],
      "system/firmware": ["system", "firmware"],
      "system/firmware_version": ["system", "firmware_version"],
      "system/device_id": ["system", "device_id"]
    };

    if (map[rel]) {
      const [section, key] = map[rel];
      state[section][key] = payload;
      state.lastUpdate = new Date();
      render();
    }
  }

  function connect() {
    if (!window.mqtt) {
      setText("mqttStatus", "MQTT library missing");
      log("mqtt.min.js missing. Replace placeholder with MQTT.js browser build.");
      return;
    }

    const url = websocketUrl();
    const options = {
      reconnectPeriod: cfg.reconnectPeriodMs || 3000,
      connectTimeout: cfg.connectTimeoutMs || 10000,
      clean: true,
      clientId: `mot-dashboard-${Math.random().toString(16).slice(2)}`
    };

    if (cfg.username) options.username = cfg.username;
    if (cfg.password) options.password = cfg.password;

    log(`Connecting to ${url}`);
    const client = mqtt.connect(url, options);

    client.on("connect", () => {
      setText("mqttStatus", "MQTT online");
      $("mqttStatus").classList.add("online");
      const topic = `${topicBase()}/#`;
      client.subscribe(topic, (err) => {
        if (err) log(`Subscribe error: ${err.message || err}`);
        else log(`Subscribed ${topic}`);
      });
    });

    client.on("reconnect", () => {
      setText("mqttStatus", "MQTT reconnecting");
      $("mqttStatus").classList.remove("online");
    });

    client.on("offline", () => {
      setText("mqttStatus", "MQTT offline");
      $("mqttStatus").classList.remove("online");
    });

    client.on("error", (err) => {
      log(`MQTT error: ${err.message || err}`);
    });

    client.on("message", (topic, message) => {
      const payload = message.toString();
      log(`${topic} = ${payload}`);
      applyTopic(topic, payload);
    });
  }

  document.addEventListener("DOMContentLoaded", () => {
    render();
    $("clearLog")?.addEventListener("click", () => setText("mqttLog", ""));
    if ("serviceWorker" in navigator) {
      navigator.serviceWorker.register("service-worker.js").catch(() => {});
    }
    connect();
  });
})();
