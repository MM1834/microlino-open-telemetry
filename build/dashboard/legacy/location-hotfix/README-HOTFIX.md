# Dashboard Location Timestamp Hotfix

## Änderung

- AWS-Standortzeit wird nur noch aus `snapshot.metadata[*].receivedAt` gelesen.
- `Date.now()` wird nur noch für echte Legacy-MQTT-Standortnachrichten verwendet.
- Standortstatus zeigt `Aktueller Standort` oder `Letzter Standort` anhand von `locationFreshnessMs`.
- Die Anzeige enthält relative Zeit sowie Datum und Uhrzeit des letzten echten GPS-Updates.
- GPS-Alias-Timestamps (`gps/latitude`, `gps/longitude`) werden ebenfalls unterstützt.

## Installation

Ersetze im Repository:

```text
dashboard/js/app.js
```

mit der beigefügten `app.js`.

## Test

1. Dashboard öffnen und Browsercache leeren.
2. Fahrzeug mit gültigem GPS-Fix verbinden.
3. Prüfen, dass `Aktueller Standort` und der echte Fix-Zeitpunkt angezeigt werden.
4. Internetverbindung der Hardware trennen.
5. Prüfen, dass die Zeit nicht mit der Browserzeit weiterläuft.
6. Nach Ablauf von `locationFreshnessMs` muss `Letzter Standort` erscheinen.
