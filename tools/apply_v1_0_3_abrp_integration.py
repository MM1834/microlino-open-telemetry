from pathlib import Path

root = Path.cwd()

def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(s, encoding="utf-8")

main = root / "firmware/esp32-wroom/src/main.cpp"
if main.exists():
    s = read(main)
    if '#include "common/abrp/abrp_client.h"' not in s:
        s = s.replace('#include "system/device_id.h"\n', '#include "system/device_id.h"\n#include "common/abrp/abrp_client.h"\n')
    if 'setupAbrp();' not in s:
        s = s.replace('    setupMqtt();\n', '    setupMqtt();\n    setupAbrp();\n')
    if 'abrpLoop();' not in s:
        s = s.replace('    webUiLoop();\n', '    webUiLoop();\n    abrpLoop();\n')
    write(main, s)

web = root / "firmware/esp32-wroom/src/web/web_ui.cpp"
if not web.exists():
    raise SystemExit("firmware/esp32-wroom/src/web/web_ui.cpp not found")

s = read(web)

if '#include "common/abrp/abrp_client.h"' not in s:
    s = s.replace('#include "app_config.h"\n', '#include "app_config.h"\n#include "common/abrp/abrp_client.h"\n')

handlers = '''
static void handleAbrpStatus()
{
    server.send(200, "application/json", abrpStatusJson());
}

static void handleAbrpTest()
{
    bool ok = sendAbrpTelemetryNow();
    server.send(ok ? 200 : 503, "application/json", abrpStatusJson());
}

'''
if 'static void handleAbrpStatus()' not in s:
    if 'static void handleConfigExport()\n' in s:
        s = s.replace('static void handleConfigExport()\n', handlers + 'static void handleConfigExport()\n', 1)
    else:
        s = s.replace('static void handleFactoryReset()\n', handlers + 'static void handleFactoryReset()\n', 1)

if 'server.on("/api/abrp/status"' not in s:
    if 'server.on("/api/system-health", HTTP_GET, handleSystemHealth);' in s:
        s = s.replace(
            '    server.on("/api/system-health", HTTP_GET, handleSystemHealth);\n',
            '    server.on("/api/system-health", HTTP_GET, handleSystemHealth);\n'
            '    server.on("/api/abrp/status", HTTP_GET, handleAbrpStatus);\n'
            '    server.on("/api/abrp/test", HTTP_POST, handleAbrpTest);\n',
            1
        )
    else:
        s = s.replace(
            '    server.on("/factory-reset", HTTP_POST, handleFactoryReset);\n',
            '    server.on("/api/abrp/status", HTTP_GET, handleAbrpStatus);\n'
            '    server.on("/api/abrp/test", HTTP_POST, handleAbrpTest);\n'
            '    server.on("/factory-reset", HTTP_POST, handleFactoryReset);\n',
            1
        )

abrp_block = '''
    s += "<div class='card'><h2>ABRP Status</h2>";
    s += "<p class='muted'>ABRP sends telemetry only when API key and user token are configured.</p>";
    s += "<button type='button' onclick='testAbrp()'>Test ABRP Send</button>";
    s += "<pre id='abrp-status' style='white-space:pre-wrap;margin-top:1rem'>Loading...</pre>";
    s += "<script>";
    s += "async function loadAbrp(){try{const r=await fetch('/api/abrp/status');const d=await r.json();document.getElementById('abrp-status').textContent=`Enabled: ${d.enabled}\\\\nLast success: ${d.lastSuccess}\\\\nHTTP: ${d.lastHttpCode}\\\\nMessage: ${d.lastMessage}\\\\nPayload: ${d.lastPayload}`;}catch(e){document.getElementById('abrp-status').textContent=e.message;}}";
    s += "async function testAbrp(){document.getElementById('abrp-status').textContent='Sending test telemetry...';try{const r=await fetch('/api/abrp/test',{method:'POST'});const d=await r.json();document.getElementById('abrp-status').textContent=`Enabled: ${d.enabled}\\\\nLast success: ${d.lastSuccess}\\\\nHTTP: ${d.lastHttpCode}\\\\nMessage: ${d.lastMessage}\\\\nPayload: ${d.lastPayload}`;}catch(e){document.getElementById('abrp-status').textContent=e.message;}}";
    s += "loadAbrp();";
    s += "</script></div>";
'''

if "ABRP Status" not in s:
    if '    s += "<div class=\'card\'><h2>Configuration Management</h2>";' in s:
        s = s.replace('    s += "<div class=\'card\'><h2>Configuration Management</h2>";', abrp_block + '\n    s += "<div class=\'card\'><h2>Configuration Management</h2>";', 1)
    elif '    s += "<div class=\'card\'><h2>Factory Reset</h2>' in s:
        s = s.replace('    s += "<div class=\'card\'><h2>Factory Reset</h2>', abrp_block + '\n    s += "<div class=\'card\'><h2>Factory Reset</h2>', 1)

write(web, s)

print("MOT v1.0.3 ABRP integration patch applied.")
