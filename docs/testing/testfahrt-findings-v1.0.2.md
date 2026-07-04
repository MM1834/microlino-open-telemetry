# Testfahrt Findings – v1.0.2

## Charging / Rekuperation
`Lädt` ist noch nicht sauber dekodiert und kann aktuell Rekuperation mit abbilden. Ziel: echtes Laden am Stecker und Rekuperation trennen.

## Energy / Power
Der Energiewert ist aktuell nur ein Indikator. Der Faktor muss später kalibriert werden. Bis dahin als `estimated` behandeln.

## Location / Map
Default/Manual Location funktioniert, aber der Kartenausschnitt ist zu grob. Ziel: stärkerer Zoom, später echtes GPS.

## Speed History
Speed wird korrekt angezeigt, aber bei Fahrt wurden zu wenige Werte gespeichert. Ziel: bei `speedKmh > 0` alle 5 Sekunden speichern, sonst 60 Sekunden.

## WebUI Refresh
Die lokale Firmware-WebUI aktualisiert zu langsam. Ziel: Status-Refresh auf ca. 2 Sekunden reduzieren.
