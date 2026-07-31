#!/usr/bin/env python3
from pathlib import Path

path = Path("firmware/esp32-wroom/src/web/web_ui.cpp")
text = path.read_text()

include = '#include "../ota/ota_web.h"\n'
if include not in text:
    marker = '#include "../network/wifi_manager.h"\n'
    if marker in text:
        text = text.replace(marker, marker + include, 1)
    else:
        text = include + text

route = '    setupOtaRoutes(server);\n'
if route not in text:
    marker = '    server.onNotFound([]()'
    if marker in text:
        text = text.replace(marker, route + marker, 1)
    else:
        marker2 = '    server.begin();'
        text = text.replace(marker2, route + marker2, 1)

loop_call = '    otaWebLoop();\n'
if loop_call not in text:
    marker = '    server.handleClient();\n'
    if marker in text:
        text = text.replace(marker, marker + loop_call, 1)
    else:
        raise SystemExit("Could not find server.handleClient() in web_ui.cpp")

# Add /update link to a common footer if possible.
old = "<a href='/config'>Config</a> · <a href='/api/status'>JSON API</a>"
new = "<a href='/config'>Config</a> · <a href='/update'>OTA Update</a> · <a href='/api/status'>JSON API</a>"
if old in text and new not in text:
    text = text.replace(old, new, 1)

path.write_text(text)
print("OTA Web UI patch applied to", path)
