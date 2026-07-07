from pathlib import Path
root = Path.cwd()
def read(p): return p.read_text(encoding="utf-8")
def write(p,s): p.write_text(s, encoding="utf-8")

pio = root/"firmware/lilygo-t-a7670/platformio.ini"
if pio.exists():
    s=read(pio)
    if "PubSubClient" not in s:
        s=s.replace("  mikalhart/TinyGPSPlus @ ^1.0.3", "  mikalhart/TinyGPSPlus @ ^1.0.3\n  PubSubClient @ ^2.8")
    if "-I ../common" not in s:
        s=s.replace("  -I include", "  -I include\n  -I ../common")
    write(pio,s)

can = root/"firmware/lilygo-t-a7670/src/can/lilygo_can.cpp"
if can.exists():
    s=read(can)
    if '#include "can/can_types.h"' not in s:
        s=s.replace('#include "board_config.h"\n', '#include "board_config.h"\n#include "can/can_types.h"\n#include "decoders/decoder_engine.h"\n')
    if "decoderEngineHandleFrame(frame);" not in s:
        s=s.replace("        logFrame(msg);\n", """        MotCanFrame frame;
        frame.id = msg.identifier;
        frame.extended = msg.extd;
        frame.dlc = msg.data_length_code;
        frame.receivedMs = millis();
        for (uint8_t i = 0; i < frame.dlc && i < 8; ++i) {
            frame.data[i] = msg.data[i];
        }
        decoderEngineHandleFrame(frame);

        logFrame(msg);
""")
    write(can,s)

main = root/"firmware/lilygo-t-a7670/src/main.cpp"
if main.exists():
    s=read(main)
    if '#include <WiFi.h>' not in s:
        s=s.replace('#include <Arduino.h>\n', '#include <Arduino.h>\n#include <WiFi.h>\n')
    if '#include "telemetry/telemetry.h"' not in s:
        s=s.replace('#include "board_config.h"\n', '#include "board_config.h"\n#include "telemetry/telemetry.h"\n')
    if '#include "mqtt/lilygo_mqtt.h"' not in s:
        s=s.replace('#include "web/lilygo_web.h"\n', '#include "web/lilygo_web.h"\n#include "mqtt/lilygo_mqtt.h"\n')
    if "telemetryInit();" not in s:
        s=s.replace("    loadLilygoConfig();", "    loadLilygoConfig();\n\n    telemetryInit();\n    telemetry.system.firmwareVersion = MOT_VERSION;\n    telemetry.system.deviceId = lilygoDeviceName();")
    if "setupLilygoMqtt();" not in s:
        s=s.replace("    setupLilygoWeb();", "    setupLilygoMqtt();\n    setupLilygoWeb();")
    if "lilygoMqttLoop();" not in s:
        s=s.replace("    lilygoWebLoop();", "    lilygoMqttLoop();\n    lilygoWebLoop();")
    if "telemetryUpdateSystemRuntime();" not in s:
        if "void loop(){" in s:
            s=s.replace("void loop(){", "void loop(){\n    static unsigned long lastSystemUpdateMs = 0;\n")
            s=s.replace("delay(2);}", "if (millis() - lastSystemUpdateMs > 1000) { lastSystemUpdateMs = millis(); telemetryUpdateSystemRuntime(); telemetry.system.networkMode = lilygoNetworkModeName(); telemetry.system.ipAddress = lilygoNetworkIp(); telemetry.system.wifiRssi = lilygoNetworkModeName() == \"WiFi\" ? WiFi.RSSI() : 0; }\n    delay(2);}")
        else:
            s=s.replace("void loop()\n{", "void loop()\n{\n    static unsigned long lastSystemUpdateMs = 0;")
            s=s.replace("    delay(2);\n}", "    if (millis() - lastSystemUpdateMs > 1000) {\n        lastSystemUpdateMs = millis();\n        telemetryUpdateSystemRuntime();\n        telemetry.system.networkMode = lilygoNetworkModeName();\n        telemetry.system.ipAddress = lilygoNetworkIp();\n        telemetry.system.wifiRssi = lilygoNetworkModeName() == \"WiFi\" ? WiFi.RSSI() : 0;\n    }\n\n    delay(2);\n}")
    write(main,s)

web = root/"firmware/lilygo-t-a7670/src/web/lilygo_web.cpp"
if web.exists():
    s=read(web)
    if '#include "mqtt/lilygo_mqtt.h"' not in s:
        s=s.replace('#include "gps/l76k_gps.h"\n', '#include "gps/l76k_gps.h"\n#include "mqtt/lilygo_mqtt.h"\n#include "api/telemetry_json.h"\n#include "telemetry/telemetry.h"\n')
    if "handleMqtt()" not in s:
        s=s.replace("static void handleModem()", "static void handleMqtt(){ server.send(200, \"application/json\", lilygoMqttStatusJson()); }\nstatic void handleTelemetry(){ server.send(200, \"application/json\", telemetryToJson(telemetry)); }\nstatic void handleModem()")
    if 'server.on("/api/lilygo/mqtt"' not in s:
        s=s.replace('server.on("/api/lilygo/modem",HTTP_GET,handleModem);', 'server.on("/api/lilygo/mqtt",HTTP_GET,handleMqtt);server.on("/api/telemetry",HTTP_GET,handleTelemetry);server.on("/api/lilygo/modem",HTTP_GET,handleModem);')
        s=s.replace('server.on("/api/lilygo/modem", HTTP_GET, handleModem);', 'server.on("/api/lilygo/mqtt", HTTP_GET, handleMqtt); server.on("/api/telemetry", HTTP_GET, handleTelemetry); server.on("/api/lilygo/modem", HTTP_GET, handleModem);')
    if '"mqtt":' not in s:
        s=s.replace('json+="\\"can\\":"+lilygoCanStatusJson();', 'json+="\\"can\\":"+lilygoCanStatusJson()+",";json+="\\"mqtt\\":"+lilygoMqttStatusJson()+",";json+="\\"telemetry\\":"+telemetryToJson(telemetry);')
        s=s.replace('json += "\\"can\\":" + lilygoCanStatusJson();', 'json += "\\"can\\":" + lilygoCanStatusJson() + ","; json += "\\"mqtt\\":" + lilygoMqttStatusJson() + ","; json += "\\"telemetry\\":" + telemetryToJson(telemetry);')
    if "MQTT WiFi" not in s:
        marker='s+="<div class=\'card\'><h2>CAN Input</h2><button onclick=\'loadCan()\'>Refresh</button><pre id=\'can\'>Loading...</pre><p><a href=\'/api/lilygo/can/frames\'>Latest frames JSON</a></p></div>";'
        add='s+="<div class=\'card\'><h2>Decoded Telemetry</h2><button onclick=\'loadTelemetry()\'>Refresh</button><pre id=\'telemetry\'>Loading...</pre></div>";s+="<div class=\'card\'><h2>MQTT WiFi</h2><button onclick=\'loadMqtt()\'>Refresh</button><pre id=\'mqtt\'>Loading...</pre></div>";'
        if marker in s:
            s=s.replace(marker, marker+add)
    if "function loadMqtt" not in s:
        s=s.replace("async function loadCan(){const r=await fetch('/api/lilygo/can');document.getElementById('can').textContent=JSON.stringify(await r.json(),null,2)}",
                    "async function loadCan(){const r=await fetch('/api/lilygo/can');document.getElementById('can').textContent=JSON.stringify(await r.json(),null,2)}async function loadTelemetry(){const r=await fetch('/api/telemetry');document.getElementById('telemetry').textContent=JSON.stringify(await r.json(),null,2)}async function loadMqtt(){const r=await fetch('/api/lilygo/mqtt');document.getElementById('mqtt').textContent=JSON.stringify(await r.json(),null,2)}")
        s=s.replace("loadNetwork();loadModem();loadGps();loadCan();setInterval(loadNetwork,5000);setInterval(loadGps,3000);setInterval(loadCan,3000);",
                    "loadNetwork();loadModem();loadGps();loadCan();loadTelemetry();loadMqtt();setInterval(loadNetwork,5000);setInterval(loadGps,3000);setInterval(loadCan,3000);setInterval(loadTelemetry,3000);setInterval(loadMqtt,5000);")
    write(web,s)

print("MOT v1.1.0 LilyGO CAN decoder + MQTT WiFi patch applied.")
