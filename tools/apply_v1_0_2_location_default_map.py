from pathlib import Path

root = Path.cwd()

index = root / "dashboard/index.html"
app = root / "dashboard/js/app.js"

if not index.exists():
    raise SystemExit("dashboard/index.html not found")
if not app.exists():
    raise SystemExit("dashboard/js/app.js not found")

html = index.read_text(encoding="utf-8")

if "css/location-map.css" not in html:
    html = html.replace("</head>", '  <link rel="stylesheet" href="css/location-map.css">\n</head>')

if 'id="location-map-frame"' not in html:
    marker = '<div class="location-text">'
    iframe = '''<iframe id="location-map-frame" class="location-map-frame" loading="lazy" referrerpolicy="no-referrer-when-downgrade"></iframe>
          <a id="location-map-link" class="location-map-link" href="#" target="_blank" rel="noopener">OpenStreetMap öffnen</a>
          '''
    if marker in html:
        html = html.replace(marker, iframe + marker, 1)
    else:
        raise SystemExit("location-text marker not found in dashboard/index.html")

index.write_text(html, encoding="utf-8")

js = app.read_text(encoding="utf-8")

helper = '''
  function updateLocationMap(lat, lon) {
    const nLat = Number(lat);
    const nLon = Number(lon);
    if (!Number.isFinite(nLat) || !Number.isFinite(nLon)) return;

    const delta = 0.0032;
    const bbox = [
      (nLon - delta).toFixed(6),
      (nLat - delta).toFixed(6),
      (nLon + delta).toFixed(6),
      (nLat + delta).toFixed(6)
    ].join(',');

    const marker = `${nLat.toFixed(6)},${nLon.toFixed(6)}`;
    const iframe = $('location-map-frame');
    const link = $('location-map-link');

    const src = `https://www.openstreetmap.org/export/embed.html?bbox=${encodeURIComponent(bbox)}&layer=mapnik&marker=${encodeURIComponent(marker)}`;
    const href = `https://www.openstreetmap.org/?mlat=${nLat.toFixed(6)}&mlon=${nLon.toFixed(6)}#map=17/${nLat.toFixed(6)}/${nLon.toFixed(6)}`;

    if (iframe && iframe.src !== src) iframe.src = src;
    if (link) link.href = href;
  }

'''

if "function updateLocationMap" not in js:
    js = js.replace("  function updateCoords(source = 'mqtt') {", helper + "  function updateCoords(source = 'mqtt') {", 1)

if "updateLocationMap(lat, lon);" not in js:
    js = js.replace(
        "      setText('location-coords', `${fmtCoord(lat, 'N', 'S')} · ${fmtCoord(lon, 'E', 'W')}`);\n",
        "      setText('location-coords', `${fmtCoord(lat, 'N', 'S')} · ${fmtCoord(lon, 'E', 'W')}`);\n"
        "      updateLocationMap(lat, lon);\n",
        1
    )

app.write_text(js, encoding="utf-8")

print("Default/live location map patch applied.")
