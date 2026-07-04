from pathlib import Path
import re

root = Path.cwd()

def read(p):
    return p.read_text(encoding='utf-8')

def write(p, s):
    p.write_text(s, encoding='utf-8')

for base in [root / 'firmware/esp32-wroom/src', root / 'firmware/esp32-wroom/include']:
    if not base.exists():
        continue
    for p in base.rglob('*'):
        if not p.is_file() or p.suffix not in ['.cpp', '.h', '.hpp', '.ino']:
            continue
        s = read(p)
        ns = re.sub(r'v?0\.9\.\d+-dev', '1.0.3-hotfix', s)
        if ns != s:
            write(p, ns)
            print(f'version updated in {p}')

inc = root / 'firmware/esp32-wroom/include/version.h'
inc.parent.mkdir(parents=True, exist_ok=True)
if inc.exists():
    s = read(inc)
    if 'MOT_FIRMWARE_VERSION' in s:
        s = re.sub(r'#define\s+MOT_FIRMWARE_VERSION\s+".*"', '#define MOT_FIRMWARE_VERSION "1.0.3-hotfix"', s)
    else:
        s += '\n#define MOT_FIRMWARE_VERSION "1.0.3-hotfix"\n'
    write(inc, s)
else:
    write(inc, '#pragma once\n#define MOT_FIRMWARE_VERSION "1.0.3-hotfix"\n')

web = root / 'firmware/esp32-wroom/src/web/web_ui.cpp'
if web.exists():
    s = read(web)
    s = s.replace("<form method='POST' action='/factory-reset'>", "<form method='POST' action='/factory-reset' onsubmit=\"return confirm('Factory Reset wirklich ausführen? Alle Einstellungen werden gelöscht.');\">")
    s = s.replace('<form method="POST" action="/factory-reset">', '<form method="POST" action="/factory-reset" onsubmit="return confirm(\'Factory Reset wirklich ausführen? Alle Einstellungen werden gelöscht.\');">')
    write(web, s)

guard = "\n  String cleanHost = host;\n  cleanHost.trim();\n  if (cleanHost.isEmpty()) {\n    r.message = \"MQTT disabled: no host configured\";\n    r.durationMs = millis() - started;\n    return r;\n  }"

for p in [
    root / 'firmware/common/src/MqttDiagnostics.cpp',
    root / 'firmware/esp32-wroom/src/common/src/MqttDiagnostics.cpp',
    root / 'firmware/esp32-wroom/src/MqttDiagnostics.cpp',
]:
    if not p.exists():
        continue
    s = read(p)
    if 'MQTT disabled: no host configured' not in s:
        marker = '  const uint32_t started = millis();'
        if marker in s:
            s = s.replace(marker, marker + guard, 1)
            s = s.replace('WiFi.hostByName(host.c_str(), ip)', 'WiFi.hostByName(cleanHost.c_str(), ip)')
            s = s.replace('tcpClient.connect(host.c_str(), port)', 'tcpClient.connect(cleanHost.c_str(), port)')
            s = s.replace('mqtt.setServer(host.c_str(), port)', 'mqtt.setServer(cleanHost.c_str(), port)')
            write(p, s)
            print(f'empty MQTT host guard patched in {p}')
        else:
            print(f'WARNING: marker not found in {p}')

print('MOT v1.0.3 hotfix patch applied.')
