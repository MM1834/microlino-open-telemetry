from pathlib import Path
import re

root = Path.cwd()

def read(p): return p.read_text(encoding="utf-8")
def write(p, s): p.write_text(s, encoding="utf-8")

# 1) Replace ABRP client with corrected implementation
src = root / "firmware/common/abrp/abrp_client.cpp"
if not src.exists():
    raise SystemExit("firmware/common/abrp/abrp_client.cpp not found. Copy package files first.")
# The package copy already placed the corrected file at that path.

# 2) Improve ABRP WebUI status rendering if present
web = root / "firmware/esp32-wroom/src/web/web_ui.cpp"
if web.exists():
    s = read(web)

    old1 = "Enabled: ${d.enabled}\\\\nLast success: ${d.lastSuccess}\\\\nHTTP: ${d.lastHttpCode}\\\\nMessage: ${d.lastMessage}\\\\nPayload: ${d.lastPayload}"
    new1 = "Enabled: ${d.enabled}\\\\nTime valid: ${d.timeValid}\\\\nUTC: ${d.utc}\\\\nLast success: ${d.lastSuccess}\\\\nHTTP: ${d.lastHttpCode}\\\\nMessage: ${d.lastMessage}\\\\nPayload: ${d.lastPayload}"
    s = s.replace(old1, new1)

    write(web, s)

# 3) Firmware version: update known 0.9.1-dev occurrences to v1.0.3
for p in [
    root / "firmware/esp32-wroom/src/version.h",
    root / "firmware/esp32-wroom/include/version.h",
    root / "firmware/esp32-wroom/src/app_config.h",
    root / "firmware/esp32-wroom/src/web/web_ui.cpp",
    root / "firmware/esp32-wroom/src/main.cpp",
]:
    if not p.exists():
        continue
    s = read(p)
    ns = s.replace("0.9.1-dev", "1.0.3-dev")
    ns = ns.replace("v0.9.1-dev", "v1.0.3-dev")
    if ns != s:
        write(p, ns)
        print(f"updated version in {p}")

# 4) If no version file exists, create one in include/ for future use.
inc = root / "firmware/esp32-wroom/include/version.h"
if not inc.exists():
    inc.write_text('#pragma once\n#define MOT_FIRMWARE_VERSION "1.0.3-dev"\n', encoding="utf-8")
    print("created firmware/esp32-wroom/include/version.h")

print("ABRP client UTC/NTP and firmware version patch applied.")
