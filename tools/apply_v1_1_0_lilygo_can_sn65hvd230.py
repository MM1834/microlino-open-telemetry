from pathlib import Path

root = Path.cwd()
def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(s, encoding="utf-8")

board = root / "firmware/lilygo-t-a7670/include/board_config.h"
if board.exists():
    s = read(board)
    s = s.replace("#define CAN_TX_PIN 33", "#define CAN_TX_PIN 13")
    s = s.replace("CAN TX: GPIO33", "CAN TX: GPIO13")
    if "SN65HVD230" not in s:
        s += "\n// LilyGO CAN via SN65HVD230: RX=GPIO32, TX=GPIO13. GPIO33 is MODEM_RI and is not used for CAN.\n"
    write(board, s)

main = root / "firmware/lilygo-t-a7670/src/main.cpp"
if main.exists():
    s = read(main)
    if '#include "can/lilygo_can.h"' not in s:
        s = s.replace('#include "web/lilygo_web.h"\n', '#include "web/lilygo_web.h"\n#include "can/lilygo_can.h"\n')
    if "setupLilygoCan();" not in s:
        s = s.replace("    setupLilygoWeb();", "    setupLilygoCan();\n    setupLilygoWeb();")
    if "lilygoCanLoop();" not in s:
        s = s.replace("    lilygoWebLoop();", "    lilygoCanLoop();\n    lilygoWebLoop();")
    s = s.replace("NOTE: GPIO33 is MODEM_RI_PIN; CAN TX pin must be reviewed before enabling CAN.", "CAN enabled via SN65HVD230: RX GPIO32, TX GPIO13. GPIO33 remains MODEM_RI.")
    s = s.replace("CAN is intentionally not started in this sprint.", "CAN input is enabled via SN65HVD230.")
    write(main, s)

web = root / "firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    s = read(web)
    if '#include "can/lilygo_can.h"' not in s:
        s = s.replace('#include "gps/l76k_gps.h"\n', '#include "gps/l76k_gps.h"\n#include "can/lilygo_can.h"\n')
    if "handleCan()" not in s:
        s = s.replace("static void handleGps()", "static void handleCan(){ server.send(200, \"application/json\", lilygoCanStatusJson()); }\nstatic void handleCanFrames(){ server.send(200, \"application/json\", lilygoCanFramesJson()); }\nstatic void handleGps()")
    if 'server.on("/api/lilygo/can"' not in s:
        s = s.replace('server.on("/api/lilygo/gps",HTTP_GET,handleGps);', 'server.on("/api/lilygo/can",HTTP_GET,handleCan);server.on("/api/lilygo/can/frames",HTTP_GET,handleCanFrames);server.on("/api/lilygo/gps",HTTP_GET,handleGps);')
        s = s.replace('server.on("/api/lilygo/gps", HTTP_GET, handleGps);', 'server.on("/api/lilygo/can", HTTP_GET, handleCan); server.on("/api/lilygo/can/frames", HTTP_GET, handleCanFrames); server.on("/api/lilygo/gps", HTTP_GET, handleGps);')
    if '"can":' not in s:
        s = s.replace('json+="\\"gps\\":"+l76kGpsStatusJson();', 'json+="\\"gps\\":"+l76kGpsStatusJson()+",";json+="\\"can\\":"+lilygoCanStatusJson();')
        s = s.replace('json += "\\"gps\\":" + l76kGpsStatusJson();', 'json += "\\"gps\\":" + l76kGpsStatusJson() + ","; json += "\\"can\\":" + lilygoCanStatusJson();')
    if "CAN Input" not in s:
        marker = 's+="<div class=\'card\'><h2>L76K GPS</h2><button onclick=\'loadGps()\'>Refresh</button><pre id=\'gps\'>Loading...</pre></div>";'
        add = 's+="<div class=\'card\'><h2>CAN Input</h2><button onclick=\'loadCan()\'>Refresh</button><pre id=\'can\'>Loading...</pre><p><a href=\'/api/lilygo/can/frames\'>Latest frames JSON</a></p></div>";'
        if marker in s:
            s = s.replace(marker, marker + add)
    if "function loadCan" not in s:
        s = s.replace("async function loadGps(){const r=await fetch('/api/lilygo/gps');document.getElementById('gps').textContent=JSON.stringify(await r.json(),null,2)}",
                      "async function loadGps(){const r=await fetch('/api/lilygo/gps');document.getElementById('gps').textContent=JSON.stringify(await r.json(),null,2)}async function loadCan(){const r=await fetch('/api/lilygo/can');document.getElementById('can').textContent=JSON.stringify(await r.json(),null,2)}")
        s = s.replace("loadNetwork();loadModem();loadGps();setInterval(loadNetwork,5000);setInterval(loadGps,3000);",
                      "loadNetwork();loadModem();loadGps();loadCan();setInterval(loadNetwork,5000);setInterval(loadGps,3000);setInterval(loadCan,3000);")
    write(web, s)

print("MOT v1.1.0 LilyGO CAN SN65HVD230 patch applied.")
