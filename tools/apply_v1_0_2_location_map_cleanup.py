from pathlib import Path
import re

root = Path.cwd()
index = root / "dashboard/index.html"
if not index.exists():
    raise SystemExit("dashboard/index.html not found")

html = index.read_text(encoding="utf-8")

if "css/location-map-cleanup.css" not in html:
    html = html.replace("</head>", '  <link rel="stylesheet" href="css/location-map-cleanup.css">\n</head>')

# Remove known static/synthetic map divs inside the location panel if present.
patterns = [
    r'\s*<div[^>]+class="[^"]*(?:location-static|location-visual|location-map-static|map-static|map-grid|map-bg|map-lines|crosshair)[^"]*"[^>]*>.*?</div>\s*',
    r'\s*<canvas[^>]+id="[^"]*(?:location|map)[^"]*"[^>]*></canvas>\s*',
]

for pat in patterns:
    html = re.sub(pat, "\n", html, flags=re.S)

index.write_text(html, encoding="utf-8")
print("Location map cleanup patch applied.")
