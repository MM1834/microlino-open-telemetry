from pathlib import Path

root = Path.cwd()

def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(s, encoding="utf-8")

abrp = root / "firmware/common/abrp/abrp_client.cpp"
if not abrp.exists():
    raise SystemExit("firmware/common/abrp/abrp_client.cpp not found")

s = read(abrp)

if "static bool validUnixTime" not in s:
    s = s.replace(
        "static String abrpTelemetryJson()\n",
        "static bool validUnixTime(time_t t)\n{\n    return t > 1700000000; // roughly 2023-11-14\n}\n\nstatic String abrpTelemetryJson()\n",
        1
    )

s = s.replace(
    '    addUInt("utc", (uint32_t)time(nullptr));',
    '    time_t now = time(nullptr);\n'
    '    if (validUnixTime(now)) {\n'
    '        addUInt("utc", (uint32_t)now);\n'
    '    }'
)

write(abrp, s)

candidates = [
    root / "firmware/esp32-wroom/src/network/wifi.cpp",
    root / "firmware/esp32-wroom/src/network/network.cpp",
    root / "firmware/esp32-wroom/src/main.cpp",
]

patched = False
for p in candidates:
    if not p.exists():
        continue

    t = read(p)
    if "configTime(0, 0" in t:
        patched = True
        continue

    if "#include <time.h>" not in t:
        if "#include <WiFi.h>" in t:
            t = t.replace("#include <WiFi.h>\n", "#include <WiFi.h>\n#include <time.h>\n", 1)
        elif "#include <Arduino.h>" in t:
            t = t.replace("#include <Arduino.h>\n", "#include <Arduino.h>\n#include <time.h>\n", 1)

    ntp = 'configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");'
    replacements = [
        ('Serial.println("WiFi connected");',
         'Serial.println("WiFi connected");\n    ' + ntp + '\n    Serial.println("NTP: time sync requested");'),
        ('Serial.println("WiFi: connected");',
         'Serial.println("WiFi: connected");\n    ' + ntp + '\n    Serial.println("NTP: time sync requested");'),
        ('networkOnlineFlag = true;',
         'networkOnlineFlag = true;\n    ' + ntp + '\n    Serial.println("NTP: time sync requested");'),
    ]

    for old, new in replacements:
        if old in t:
            t = t.replace(old, new, 1)
            write(p, t)
            patched = True
            print(f"patched NTP in {p}")
            break

if not patched:
    print("WARNING: NTP hook not inserted automatically.")
    print('Add this after WiFi connects: configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");')

print("ABRP UTC/NTP fix applied.")
