from pathlib import Path
import re

root = Path.cwd()
index = root / "dashboard/index.html"
if not index.exists():
    raise SystemExit("dashboard/index.html not found")

html = index.read_text(encoding="utf-8")

html = re.sub(
    r'\s*<article class="card panel chart-panel">\s*<h2>Verlauf \(SoC\)</h2>\s*<div class="chart-line" id="soc-chart"></div>\s*</article>\s*',
    "\n",
    html,
    flags=re.S
)

if 'href="#history"' not in html:
    html = html.replace(
        '<a href="#location">⌖ Standort</a>',
        '<a href="#location">⌖ Standort</a>\n        <a href="#history">↗ History</a>',
        1
    )

index.write_text(html, encoding="utf-8")
print("Removed old overview SoC chart and added History nav link.")
