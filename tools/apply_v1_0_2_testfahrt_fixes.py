from pathlib import Path
root = Path.cwd()

def rw(path, fn):
    p = root / path
    if not p.exists():
        print(f"skip missing {path}")
        return
    s = p.read_text(encoding="utf-8")
    ns = fn(s)
    p.write_text(ns, encoding="utf-8")
    print(f"patched {path}")

def patch_recorder(s):
    s = s.replace("const DEFAULT_SAMPLE_INTERVAL_MS=60000,DEFAULT_RETENTION_DAYS=30;",
                  "const DEFAULT_SAMPLE_INTERVAL_MS=60000,DRIVING_SAMPLE_INTERVAL_MS=5000,DEFAULT_RETENTION_DAYS=30;")
    s = s.replace("const DEFAULT_SAMPLE_INTERVAL_MS=60000;\\nconst DEFAULT_RETENTION_DAYS=30;",
                  "const DEFAULT_SAMPLE_INTERVAL_MS=60000;\\nconst DRIVING_SAMPLE_INTERVAL_MS=5000;\\nconst DEFAULT_RETENTION_DAYS=30;")
    if "DRIVING_SAMPLE_INTERVAL_MS" not in s:
        s = s.replace("DEFAULT_SAMPLE_INTERVAL_MS = 60 * 1000;",
                      "DEFAULT_SAMPLE_INTERVAL_MS = 60 * 1000;\\n  const DRIVING_SAMPLE_INTERVAL_MS = 5 * 1000;")
    if "isDrivingSample" not in s:
        s = s.replace("if(now-lastStoredTs>=intervalMs)return true;",
                      "const isDrivingSample=Number(sample.speedKmh)>0;\\n  if(isDrivingSample&&now-lastStoredTs>=DRIVING_SAMPLE_INTERVAL_MS)return true;\\n  if(now-lastStoredTs>=intervalMs)return true;")
        s = s.replace("if (now - lastStoredTs >= intervalMs) return true;",
                      "const isDrivingSample = Number(sample.speedKmh) > 0;\\n  if (isDrivingSample && now - lastStoredTs >= DRIVING_SAMPLE_INTERVAL_MS) return true;\\n  if (now - lastStoredTs >= intervalMs) return true;")
    return s

def patch_app(s):
    s = s.replace(".setView([lat, lon], 14)", ".setView([lat, lon], 17)")
    s = s.replace(".setView([lat, lon], 15)", ".setView([lat, lon], 17)")
    s = s.replace("setZoom(14)", "setZoom(17)")
    s = s.replace("setZoom(15)", "setZoom(17)")
    if "Rekuperation" not in s:
        s = s.replace(
            "case 'charging/power_signed': case 'charging/power_display': setText('power', `${fmtNum(Number(val)/10,1)} kW`); break;",
            "case 'charging/power_signed': case 'charging/power_display': { const p=Number(val)/10; setText('power', `${fmtNum(p,1)} kW`); if(p < -0.1) setText('charging-card','Rekuperation'); break; }"
        )
    return s

def patch_web(s):
    for old in ["content='10'", 'content="10"', "setTimeout(loadStatus, 10000)", "setInterval(loadStatus, 10000)", "setTimeout(refreshStatus, 10000)", "setInterval(refreshStatus, 10000)"]:
        new = old.replace("10", "2").replace("10000", "2000")
        s = s.replace(old, new)
    return s

rw("dashboard/js/history/history-recorder.js", patch_recorder)
rw("dashboard/js/app.js", patch_app)
rw("firmware/esp32-wroom/src/web/web_ui.cpp", patch_web)
print("MOT v1.0.2 test-drive fixes patch applied.")
