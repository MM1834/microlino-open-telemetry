# Onboarding von Benutzer, Fahrzeug und Adapter

> **Status:** Aktuelles Verfahren für den kontrollierten Beta-Betrieb
>
> **Zielgruppe:** Administrator, Geräte-Provisionierer, Support und eingeladene Benutzer
>
> **Letzte Prüfung:** 2026-08-25

## Zweck und Geltungsbereich

Diese Anleitung beschreibt das vollständige Onboarding eines neuen Benutzers,
einer neuen logischen Fahrzeugidentität und eines physischen Telemetrieadapters.
Sie gilt für den kontrollierten Portalbetrieb unter `/dashboard/`.

Es gibt keine öffentliche Selbstregistrierung. Ein Administrator lädt den Benutzer
ein und gibt genau die vorbereitete Fahrzeugidentität frei. Das Portal überträgt
keine AWS-IoT-Zertifikate und konfiguriert den Adapter nicht. Lokale
Adapterkonfiguration, Cloud-Geräteidentität und Portalberechtigung sind getrennte
Sicherheitsgrenzen.

## Rollen und Identitäten

| Gegenstand | Schlüssel | Verantwortliche Stelle |
|---|---|---|
| Benutzerkonto | Cognito `sub` | Amazon Cognito |
| Fahrzeug im Portal und Telemetrie-Namensraum | `vehicleId` | MOT-Backend |
| Physischer Adapter | `deviceId` | Geräteinventar |
| Cloud-Gerät | IoT Thing und individuelles Zertifikat | AWS IoT |

Diese Schlüssel dürfen nicht gegeneinander ausgetauscht werden. Insbesondere sind
E-Mail-Adresse und Mobilnummer Benachrichtigungsziele, aber keine
Berechtigungsschlüssel; ein Thing-Name ist keine Fahrzeugzuweisung.

## Voraussetzungen

Vor Beginn müssen folgende Punkte feststehen:

- freigegebener Auftrag oder Beta-Vorgang mit nicht personenbezogener Referenz;
- E-Mail-Adresse des einzuladenden Benutzers über einen geschützten Kanal;
- optional: Mobilnummer im internationalen Format für SMS-Benachrichtigungen;
- eindeutige, kleingeschriebene `vehicleId` aus Buchstaben, Ziffern und Bindestrichen;
- eindeutige Gerätekennzeichnung und aus dem Gerät ausgelesene `deviceId`;
- Hardwaretyp, CAN-Transceiver, Kabelbaum, GPS-Variante und Firmwareziel;
- freigegebenes Firmware-Artefakt mit Git-Revision und SHA-256-Prüfsumme;
- Ziel-AWS-Stack und Region; im aktuellen Entwicklungsbetrieb normalerweise
  `mot-aws-3-1` und `eu-north-1`;
- Zugriff eines autorisierten Provisionierers auf AWS IoT und eines Administrators
  auf die Portalgruppe `mot-beta-admins`.

Passwörter, Token, Claim-Codes, private Schlüssel und Zertifikatsdateien gehören
nie in Git, Tickets, Screenshots, E-Mails oder ungeschützte Support-Chats.

## Ablaufübersicht

1. **MOT Portal Account beantragen und aktivieren (falls noch nicht vorhanden).**
   Die Einladung und Aktivierung können bereits vor der Gerätebereitstellung
   erfolgen.
2. Fahrzeug- und Adapteridentitäten festlegen.
3. Adapter hardwareseitig prüfen.
4. Individuelle AWS-IoT-Identität erstellen und installieren.
5. Firmware und lokale Administration konfigurieren.
6. Telemetrie der vorgesehenen `vehicleId` nachweisen.
7. Einmaligen Fahrzeug-Claim ausstellen und geschützt übergeben.
8. Benutzer schließt Anmeldung und Claim im Portal ab.
9. Optional Benachrichtigungskanäle registrieren und pro Fahrzeug aktivieren.
10. Administrator prüft Identität, Berechtigung, Telemetrie und Isolation.
11. Übergabe dokumentieren und Geheimnisse wieder entfernen.

## Validierter Referenzablauf vom 2026-08-08

Der vollständige Ablauf wurde mit einem frisch vorbereiteten Seeed XIAO ESP32-C6
unter einer neuen, nicht personenbezogenen Fahrzeugidentität validiert:

1. Ein neuer Cognito-Benutzer wurde eingeladen, bestätigte sein Konto und erhielt
   zunächst keine Fahrzeugberechtigung.
2. Der angeschlossene XIAO wurde über USB eindeutig identifiziert; seine beiden
   CAN-Eingänge und GPS liefen vor der Cloud-Provisionierung fehlerfrei.
3. Ein neues AWS IoT Thing, eine gerätespezifische Least-Privilege-Policy und ein
   neues individuelles Zertifikat wurden für genau diesen Adapter erstellt.
4. `device.json`, CA, Zertifikat und privater Schlüssel wurden als LittleFS-Image
   auf den XIAO geladen. Die laufende AWS-Firmware erkannte die Zugangsdaten.
5. Nach WLAN-Konfiguration publizierte der Adapter zuerst System-, Netzwerk- und
   GPS-Zustand. Erst damit existierte die `vehicleId` in `VehicleState` und wurde
   für die Claim-Ausstellung zulässig.
6. Ein Administrator stellte im Portal einen kurzlebigen Einmal-Claim aus. Der
   bestätigte Benutzer verbrauchte ihn im Portal.
7. Die Abschlussprüfung bestätigte genau einen `ACTIVE`/`OWNER`-Eintrag in
   `VehicleOwnership` und `UserVehicleAccess`, den bestätigten Cognito-`sub`,
   Thing-/Zertifikatsbindung und Portaltelemetrie.
8. Ein kurzer CAN-Test an einem anderen Fahrzeug wurde anschließend
   datenschutzgerecht bereinigt: ausschließlich die betroffenen `bms/*`,
   `charging/*` und `display/*` State-Einträge wurden gelöscht. Konto,
   Eigentumszuordnung, Thing, Zertifikat sowie `location/*`, `system/*` und
   `status/*` blieben bestehen. Für die Testidentität lagen keine History-Einträge
   vor.

Der Referenzlauf bestätigt insbesondere, dass die Claim-Funktion keine leere
Fahrzeugidentität anlegt. Ein HTTP 404 bei der Claim-Ausstellung bedeutet im
aktuellen Backend, dass für die eingegebene `vehicleId` noch kein Telemetriezustand
existiert. Ein künstlicher Platzhalter in DynamoDB ist kein zulässiger Ersatz für
die reale Adapter-Provisionierung und erste AWS-Publikation.

## 1. Fahrzeug und Adapter im Inventar anlegen

Der Provisionierer erfasst vor jeder Cloud- oder Benutzeränderung:

- stabile `vehicleId` und einen lesbaren Fahrzeugnamen;
- physische `deviceId` und Inventar-/Gehäuselabel;
- Board, Transceiver, Kabelbaum, Stromversorgung und Gehäuserevision;
- vorhandene GPS-Hardware;
- vorgesehenen Thing-Namen, Firmwarestand und Benutzerzuordnung;
- Entscheidung, ob der Adapter eine neue Portal-Fahrzeugidentität erhält.

Eine `vehicleId` bleibt normalerweise über einen späteren Adaptertausch hinweg
stabil. Parallel betriebene Betaadapter dürfen bewusst getrennte `vehicleId`s
erhalten. Die Entscheidung wird dokumentiert und nicht aus ähnlicher Telemetrie
oder einer Werksrücksetzung abgeleitet.

## 2. Hardware prüfen

Vor dem Anschluss an das Fahrzeug:

1. CAN-H, CAN-L, Masse, Versorgung und Pinbelegung gegen Board und Kabelbaum prüfen.
2. Spannungsverträglichkeit des Transceivers und Terminierung des bestehenden
   Fahrzeugbusses prüfen.
3. Passive CAN-Nutzung und fehlendes anwendungsseitiges Senden bestätigen.
4. Isolation, Zugentlastung, Gehäuse und geregelte Stromversorgung kontrollieren.
5. Bei GPS-Varianten UART, Versorgung und Antennenposition prüfen.

Die Hardwareprüfung allein autorisiert noch keinen Anschluss an das Fahrzeug.
Hardwaretests erfolgen erst im freigegebenen Vorgang.

## 3. AWS-IoT-Geräteidentität provisionieren

Für jeden physischen Adapter wird eine eigene Cloud-Identität verwendet:

1. Ein eindeutiges AWS IoT Thing erstellen.
2. Ein neues Zertifikat mit privatem Schlüssel erstellen und aktivieren.
3. Zertifikat genau diesem Thing zuordnen.
4. Eine Least-Privilege-Policy für Thing und vorgesehenen
   `mot/<vehicleId>/...`-Namensraum zuordnen.
5. `device.json` mit Endpoint, Port, Thing-Name, `vehicleId` und Topic-Präfix
   vorbereiten.
6. Amazon Root CA, Gerätezertifikat, privaten Schlüssel und `device.json` über den
   kontrollierten LittleFS-Provisionierungsweg auf genau einen Adapter laden.
7. Nicht geheime Zertifikats-ID und Zuordnungen im geschützten Inventar erfassen.
8. Prüfen, dass kein Zertifikat und kein privater Schlüssel von einem anderen
   Gerät wiederverwendet wird.

Der private Schlüssel wird weder dem Portalbenutzer ausgehändigt noch im Portal
gespeichert. Temporäre lokale Staging-Dateien sind nach erfolgreicher Prüfung aus
dem Arbeitsbereich zu entfernen; `git check-ignore` ist vor jedem Upload erneut zu
prüfen.

## 4. Firmware und lokale Administration einrichten

1. Das für Board und AWS-Pfad freigegebene Firmware-Artefakt flashen.
2. Gemeldete Version, Boardtyp und `deviceId` mit dem Inventar vergleichen.
3. Beim ersten Auftreten die Abkürzung **AP** einmal als **WLAN/WiFi Access Point**
   ausschreiben und den lokalen Einstieg konkret erklären:
   - Vom Laptop, Smartphone oder Tablet mit dem WLAN `MOT-xxxx` verbinden;
     das Passwort steht auf dem geschützten Inventarblatt.
   - Im Browser [http://192.168.4.1](http://192.168.4.1) öffnen.
   - Mit Benutzername `setup` und demselben Passwort vom Inventarblatt anmelden.
4. Beim kontrollierten ersten Setup ein eindeutiges lokales
   Administratorpasswort mit 12 bis 63 Zeichen setzen und zur Vermeidung eines
   Tippfehlers ein zweites Mal identisch eingeben. Es schützt danach sowohl den
   lokalen Benutzer `admin` als auch den Geräte-Hotspot `MOT-xxxx`.
   Nach dem Speichern gilt der initiale Benutzer `setup` nicht mehr. Für alle
   folgenden Anmeldungen wird Benutzer `admin` mit dem soeben gesetzten lokalen
   Administratorpasswort verwendet. Bei Verlust kann es nur über den physischen
   USB-Konsolenbefehl `admin recover` ersetzt werden.
5. Home-WLAN und, beim C6-Pfad falls vorgesehen, das zweite mobile
   Hotspot-Profil konfigurieren.
6. Provisionierte `vehicleId`, Gerätename und CAN-Profil kontrollieren; eine
   bestehende `vehicleId` nicht eigenmächtig ändern.
7. AWS IoT aktivieren. Legacy MQTT und ABRP bleiben deaktiviert, sofern der
   konkrete Vorgang sie nicht ausdrücklich vorsieht.
8. Lokale OTA-Funktion nach der Provisionierung deaktiviert lassen.
9. Konfiguration sichern. Das Backup geschützt behandeln, auch wenn aktuelle
   C6-Backups geheimnisbereinigte Felder verwenden.

Das lokale Administratorpasswort wird nur über den genehmigten Secret-/Supportkanal
übergeben. Es gehört nicht auf das Geräteetikett oder in die allgemeine
Übergabedokumentation.

Die aktuelle C6-Firmware führt nach dem einmaligen Sicherheits-Setup automatisch
durch den lokalen Wizard. WLAN, CAN und Dienste werden direkt in dessen Schritten
bearbeitet. Der zuletzt erreichte Schritt bleibt über erforderliche Neustarts
erhalten. Während der Wizard noch nicht abgeschlossen ist, bleibt der geschützte
Hotspot `MOT-xxxx` auch bei erfolgreicher Verbindung mit Home oder Mobile/WiFi2
unter `http://192.168.4.1` erreichbar.

Der WLAN-Schritt erklärt diesen temporären Hotspot. Bei bestehender Verbindung
zeigt die Abschlussseite das tatsächlich aktive Profil (Home oder Mobile/WiFi2),
dessen WLAN-Namen und die aktuelle lokale IP-Adresse an. Nach dem expliziten
Abschluss erfolgt der Zugriff normalerweise über dieses WLAN; der zusätzliche AP
wird nach stabiler Verbindung beendet. Wenn weder Home noch Mobile/WiFi2
erreichbar ist, wird der geschützte Hotspot wieder aktiv und die lokale
Konfiguration ist über `MOT-xxxx` und `http://192.168.4.1` mit Benutzer `admin`
erreichbar. WLAN- oder Administratorpasswörter werden auf diesen Seiten nicht
angezeigt. Eine physische Erstanwendungsprüfung dieser neuen Führung steht noch
aus.

Im CAN-Schritt wird nur dann neu gestartet, wenn sich mindestens eines der beiden
Decoderprofile tatsächlich geändert hat. Werden die angezeigten Profile
unverändert bestätigt, speichert das Gerät lediglich den Wizard-Fortschritt und
wechselt unmittelbar zum nächsten Schritt.

## 5. Adapter und Fahrzeug technisch validieren

Vor Ausstellung eines Claims muss das Backend bereits Telemetriezustand für die
`vehicleId` kennen. Folgende Nachweise sind erforderlich:

1. Gerät startet mit erwarteter Version und Identität.
2. Geschützter Setup-/Fallback-AP und authentifizierte lokale WebUI funktionieren.
3. WLAN-Verbindung, Zeitbezug und Wiederanlauf funktionieren.
4. CAN-Zähler und erwartete Fahrzeugwerte werden am vorgesehenen Fahrzeug geprüft;
   CAN-Fehler bleiben null oder werden begründet.
5. AWS IoT verbindet mit dem vorgesehenen Thing und publiziert ausschließlich in
   den freigegebenen Fahrzeug-Namensraum.
6. Das Backend liefert einen aktuellen Snapshot für genau diese `vehicleId`.
7. GPS-Verhalten entspricht der Hardwarevariante.
8. Wiederanlauf nach Spannungsunterbrechung sowie der freigegebene lokale
   Recovery-/OTA-Pfad sind geprüft.

Ohne vorhandenen Telemetriezustand lehnt die Onboarding-API die Claim-Ausstellung
als `vehicle_not_provisionable` ab.

## 6. Benutzerkonto einladen

Die aktuelle Beta ist einladungsbasiert. Vor der Einladung prüft der Administrator
den Ziel-Stack, User Pool und die E-Mail-Adresse. Der Benutzer wird über Cognito
eingeladen; der Administrator vergibt und kennt kein Benutzerpasswort.

Die Einladung ist nicht von einem bereits provisionierten oder online erreichbaren
Adapter abhängig und darf deshalb am Anfang des Onboardings oder parallel zur
Gerätevorbereitung erfolgen. Claim, History-Freigabe sowie E-Mail- und SMS-
Aktivierung folgen erst, sobald ihre jeweiligen technischen Voraussetzungen
erfüllt sind. Ein bestätigtes Konto ohne Fahrzeugberechtigung ist dabei ein
zulässiger Zwischenzustand.

Für einen neuen Benutzer ohne direkte Fahrzeugzuweisung wird das Konto über den
genehmigten Cognito-Administratorweg angelegt und die Cognito-Einladung ausgelöst.
Nach Erhalt öffnet der Benutzer ausschließlich den offiziellen Portal-/Cognito-Link,
setzt das verlangte eigene Passwort und schließt gegebenenfalls die
E-Mail-Verifikation ab.

Die stabile Cognito-Identität ist der erzeugte `sub`. Eine spätere Änderung der
E-Mail-Adresse oder eine normale Cognito-Kontowiederherstellung ändert nicht
automatisch Fahrzeugbesitz oder Zugriffsrechte.

## 7. Einmaligen Claim ausstellen — empfohlener B2-Pfad

1. Administrator meldet sich am kanonischen Portal unter
   `https://www.microlino-open-telemetry.ch/dashboard/` an.
2. Prüfen, dass der Bereich **Beta-Onboarding verwalten** sichtbar ist. Er darf nur
   Mitgliedern der Cognito-Gruppe `mot-beta-admins` angezeigt werden.
3. Die zuvor geprüfte `vehicleId` eingeben und **Claim erstellen** wählen.
4. Ausgegebene `vehicleId` und Ablaufzeit kontrollieren.
5. Den Claim-Code genau einmal über einen geschützten, kurzlebigen Kanal an den
   vorgesehenen Benutzer übergeben.
6. Claim nicht in eine URL einfügen, nicht fotografieren und nicht in einem Ticket
   speichern. Nach bestätigter Übergabe **Claim-Anzeige leeren** wählen.

Ein Claim kann nur für eine inventarisierte `vehicleId` mit vorhandener Telemetrie
und ohne aktive Eigentümerzuordnung ausgestellt werden. Er ist zeitlich begrenzt,
nur einmal verwendbar und gegen wiederholte Fehlversuche begrenzt. Der Server
speichert nur einen gesalzenen Hash des Proofs.

## 8. Benutzer verbindet das Fahrzeug im Portal

1. Der Benutzer öffnet die kanonische `/dashboard/`-Adresse und wählt
   **Anmelden**.
2. Anmeldung über die von Cognito bereitgestellte Seite abschließen.
3. Bei einem Konto ohne Fahrzeug erscheint **Fahrzeug verbinden**. Den vollständigen
   Claim-Code in das Feld **Claim-Code** einfügen und **Fahrzeug verbinden** wählen.
4. Bei einem Konto mit bestehender Zuordnung zuerst **Fahrzeug hinzufügen** wählen
   und dann den neuen Claim-Code eingeben.
5. Auf **Fahrzeug erfolgreich zugewiesen** warten. Das Portal leert das Eingabefeld
   und lädt die freigegebene Fahrzeugliste neu.
6. Fahrzeug auswählen und Plausibilität von Name, Online-Status und aktuellen
   Telemetriewerten prüfen.
7. Abmelden, mit der Browser-Zurück-Funktion prüfen, dass keine Fahrzeugdaten
   wieder erscheinen, und erneut anmelden.

Bei `Claim ungültig oder nicht mehr verfügbar` werden aus Sicherheitsgründen falsche,
abgelaufene, bereits verwendete, widerrufene und konfliktbehaftete Claims nicht
unterschieden. Der Benutzer versucht nicht wiederholt weiter, sondern kontaktiert
den Support. Der Administrator prüft den Zustand und stellt nur nach geklärtem
Inventar- und Eigentumsstatus einen neuen Claim aus.

## 9. Optional: Benachrichtigungen registrieren und einstellen

Dieser Schritt ist optional und darf die Anmeldung, Fahrzeugzuweisung oder
Adapterinbetriebnahme nicht blockieren. Er kann während des gemeinsamen
Onboardings oder später durch den Benutzer abgeschlossen werden.

Die gewünschten optionalen Dienste werden mit dem MOT-Administrator abgestimmt:

- **History:** wird für die Fahrzeugidentität administrativ freigegeben;
- **E-Mail-Benachrichtigungen:** werden pro Fahrzeug vom Benutzer aktiviert; eine
  technische Bestätigung der Zieladresse beziehungsweise Subscription muss
  abgeschlossen sein und der Administrator prüft den wirksamen Zustand;
- **SMS:** die Nummer wird durch den Benutzer bestätigt, im Pilot zusätzlich
  administrativ freigegeben und danach pro Fahrzeug vom Benutzer aktiviert.

Diese Freigaben können nach dem eigentlichen Onboarding erfolgen. Ein aktives
Portal-Konto und eine erfolgreiche Fahrzeugverknüpfung hängen nicht von ihnen ab.

### E-Mail

1. Im Dashboard das gewünschte Fahrzeug auswählen und die
   Benachrichtigungseinstellungen öffnen.
2. Den E-Mail-Kanal für dieses Fahrzeug aktivieren und **Speichern** wählen.
3. Falls eine Bestätigungsnachricht des Versanddienstes eintrifft, den darin
   enthaltenen Link öffnen. Erst danach können Benachrichtigungen zugestellt
   werden.

### SMS im kontrollierten Pilotbetrieb

1. Mobilnummer im internationalen Format mit `+41` oder `+49` eingeben.
2. **Bestätigungscode senden** wählen, den per SMS erhaltenen Code eingeben und
   bestätigen.
3. Der Administrator gibt im aktuellen Pilotverfahren die bestätigte Nummer für
   die vorgesehene Nutzung frei. Diese Abhängigkeit ist eine befristete
   Schutzmassnahme des Piloten und nicht das Zielbild des definitiven Onboardings.
4. Nach angezeigter Bestätigung und administrativer Freigabe SMS für das
   ausgewählte Fahrzeug aktivieren und **Speichern** wählen.
5. Die Seite neu laden und kontrollieren, dass die Einstellung weiterhin aktiv
   ist.

Eine Mobilnummer darf für mehrere Fahrzeuge oder auch für mehrere Benutzer
verwendet werden. Im Pilot werden Verifikation und Freigabe für die jeweilige
Benutzer-/Nummern-Zuordnung geführt; die Aktivierung wird separat pro Fahrzeug
gespeichert. Das Ändern einer Mobilnummer überträgt keine Fahrzeugrechte.

SMS und E-Mail melden das Erreichen der SOC-Limite sowie einen qualifizierten
Ladestopp. Pro durchgehend eingesteckter Ladesession wird höchstens ein Ladestopp
erkannt; erst Aus- und erneutes Einstecken beginnt eine neue Session. Die
Fahrtzusammenfassung wird nur per E-Mail und nicht per SMS versendet. Alle
Benachrichtigungen sind informativ und ersetzen keine Fahrzeuganzeige.

## Zielbild für das definitive Onboarding

Die fachliche Berechtigung zur Nutzung eines Benachrichtigungskanals wird künftig
von der Registrierung einer konkreten E-Mail-Adresse oder Mobilnummer getrennt:

- Fahrzeugzugriff bleibt ausschliesslich an Cognito-`sub` und Fahrzeugzuweisung
  gebunden.
- Ein Administrator erteilt bei Bedarf eine kanalbezogene Berechtigung, aber
  erfasst oder genehmigt nicht routinemässig jede Zieladresse.
- Der Benutzer registriert, verifiziert, ändert und entfernt seine eigenen
  Benachrichtigungsziele selbst.
- Die Aktivierung bleibt eine bewusste Einstellung pro Fahrzeug; dieselbe
  verifizierte Nummer kann für mehrere berechtigte Zuordnungen verwendet werden.
- Missbrauchsschutz, Länder-/Absenderfreigabe, Ausgabenlimit, Alarme und Audit
  bleiben zentrale Betriebs- und Sicherheitskontrollen.

Bis dieses Modell implementiert und validiert ist, gilt die oben beschriebene
nummernspezifische Admin-Freigabe für den Pilotbetrieb.

## 10. Abschlussprüfung durch Administrator und Benutzer

Nach erfolgreichem Claim wird der Vorgang erst geschlossen, wenn alle Punkte
bestätigt sind:

- Cognito-Benutzer ist bestätigt; der notierte Schlüssel ist sein `sub`;
- `VehicleOwnership` enthält genau einen aktiven Eigentümer für die `vehicleId`;
- `UserVehicleAccess` enthält `status=ACTIVE` und `role=OWNER` für genau dieses
  Paar aus `sub` und `vehicleId`;
- `GET /api/vehicles` zeigt nur die zugewiesenen Fahrzeuge;
- Snapshot und Live-WebSocket liefern Daten des ausgewählten Fahrzeugs;
- ein kontrolliertes zweites Benutzerkonto kann die `vehicleId` weder erraten noch
  per REST oder WebSocket abonnieren;
- Thing, Zertifikat, `deviceId` und `vehicleId` stimmen mit dem Inventar überein;
- Claim steht auf `CONSUMED` und kann nicht wiederverwendet werden;
- privacy-sichere Auditereignisse für Ausstellung und Verbrauch sind vorhanden;
- Abmeldung entfernt die authentifizierte Fahrzeugansicht;
- falls E-Mail gewünscht: Kanal ist bestätigt, pro Fahrzeug gespeichert und mit
  einer erwarteten Benachrichtigung geprüft;
- falls SMS gewünscht: Nummer ist verifiziert und administrativ freigegeben, die
  Aktivierung ist pro Fahrzeug gespeichert und eine Testzustellung wurde geprüft;
- falls Benachrichtigungen übersprungen wurden: dies ist als optionale, später
  nachholbare Entscheidung dokumentiert und blockiert den Abschluss nicht;
- Übergabedatum, Git-Revision, Firmware-Artefakt, Stack, Region und Prüfergebnis
  sind ohne Geheimnisse dokumentiert.

## Temporäre Testdaten vor der Übergabe bereinigen

Wurde ein neuer Adapter zur Funktionsprüfung vorübergehend an einem anderen
Fahrzeug betrieben, muss vor der Pilotübergabe entschieden werden, welche Daten
zur Zielidentität gehören. Eine Bereinigung erfolgt nur nach exakter Bestandsaufnahme:

1. Aktuelle State-Schlüssel der Ziel-`vehicleId` konsistent auslesen.
2. History-Tabelle separat auf Einträge dieser `vehicleId` prüfen.
3. Nur nachweislich fremde Fahrzeugdaten löschen; beim aktuellen Topic-Vertrag
   sind dies typischerweise `bms/*`, `charging/*` und `display/*`.
4. `location/*` nur löschen, wenn auch die Testposition entfernt werden soll. Ohne
   ausdrückliche Freigabe bleiben Standortdaten ebenso wie `system/*` und
   `status/*` bestehen.
5. Benutzer-, Eigentums-, Thing-, Policy- und Zertifikatsdatensätze niemals als
   Teil einer Telemetriebereinigung verändern.
6. Nach mindestens einem Publish-Intervall prüfen, ob ein noch angeschlossener
   Adapter die gelöschten Werte erneut erzeugt.
7. Gelöschte Schlüssel, nicht betroffene Datenklassen und History-Ergebnis ohne
   Messwerte oder Geheimnisse im Betriebsvorgang festhalten.

Die Bereinigung ist eine kontrollierte Administratoroperation und kein
Endbenutzer-Feature. Sie darf keine erfundenen State-Einträge erzeugen und keine
allgemeine Löschfunktion über eine frei eingegebene `vehicleId` bereitstellen.

## Spätere Verwendung einer echten Pilot-E-Mail-Adresse

Die Cognito-Identität und Fahrzeugberechtigung hängen am stabilen `sub`, nicht am
Text der E-Mail-Adresse:

- Wird die E-Mail-Adresse innerhalb desselben geprüften Cognito-Kontos geändert,
  bleibt der `sub` und damit die Fahrzeugzuordnung erhalten. Die neue Adresse muss
  gemäß Cognito-Richtlinie verifiziert werden.
- Wird stattdessen ein neues Cognito-Konto angelegt, handelt es sich um eine
  Eigentumsübertragung. Sie darf nicht durch einen normalen B2-Claim oder eine
  zweite `OWNER`-Zuweisung simuliert werden und bleibt bis zur B3-Implementierung
  eine kontrollierte Maintainer-Operation.

## Direkte Einladung und Zuweisung — kontrollierter B1-Ausnahmeweg

Der ältere B1-Pfad lädt einen Benutzer ein und erzeugt sofort eine
`ACTIVE`/`OWNER`-Zuweisung; der Benutzer benötigt dann keinen Claim. Er bleibt für
kleine kontrollierte Beta- oder Wiederaufnahmefälle verfügbar, darf aber nicht
parallel zum Claim-Pfad für dasselbe Fahrzeug ausgeführt werden.

Zuerst ausschließlich den Plan ausführen:

```bash
python3 tools/aws/admin_onboard_beta_user.py \
  --email <BETA_USER_EMAIL> \
  --vehicle-id <VEHICLE_ID> \
  --source <FREIGABE_REFERENZ>
```

Der Administrator prüft die geheimnisbereinigte Ausgabe, Stack, Region,
Benutzerzustand, vorhandenen Telemetriezustand und Eigentümerkonflikte. Nur im
genehmigten Vorgang folgt derselbe Befehl mit `--apply`.

Das Werkzeug verweigert eine fremde aktive Eigentümerschaft und eine implizite
Reaktivierung von `REVOKED`. Scheitert die Zuweisung nach erfolgreicher Einladung,
bleibt der sichere Zustand „eingeladener Benutzer ohne Zugriff“. Nach Prüfung kann
der idempotente Ablauf wiederholt werden; ein vorhandener Datensatz wird nicht
blind überschrieben.

## Abbruch- und Eskalationsfälle

Onboarding stoppen und an den Maintainer eskalieren, wenn:

- `deviceId`, Thing, Zertifikat oder `vehicleId` nicht zum Inventar passen;
- Telemetrie unter einer unerwarteten `vehicleId` erscheint;
- bereits ein anderer aktiver Eigentümer existiert;
- der Benutzer Fahrzeuge eines anderen Kontos sehen kann;
- Zertifikat oder privater Schlüssel möglicherweise offengelegt oder mehrfach
  installiert wurde;
- der Claim nach mehreren Eingaben nicht akzeptiert wird;
- CAN-Verbindung, Stromversorgung oder Terminierung nicht zweifelsfrei sicher ist;
- ein Adapter ersetzt, verloren, gestohlen, zurückgesetzt, übertragen oder
  stillgelegt werden soll.

## Noch nicht automatisierte Lebenszyklusfälle

Adaptertausch, Verlust, Wiederherstellung, Eigentumsübertragung und Stilllegung
sind in ONB-001.B3 entworfen, aber nicht implementiert. Sie sind keine normale
Claim- oder Werksreset-Operation.

- Ein gewöhnlicher Adaptertausch behält standardmäßig die `vehicleId`, erzeugt aber
  eine neue `deviceId`, ein neues Thing und ein neues Zertifikat.
- Ein verlorenes Gerät erfordert die sofortige Deaktivierung seines Zertifikats.
- Ein wiedergefundenes Gerät erhält ein neues Zertifikat; der alte private
  Schlüssel wird nie reaktiviert oder wiederverwendet.
- Ein Factory Reset ändert keinen Besitzer und darf keine zweite Cloud-Identität
  erzeugen. Bei unklarer Schlüsselbewahrung werden Zugangsdaten rotiert.
- Eine Eigentumsübertragung widerruft zuerst den alten Zugriff, rotiert das
  Gerätezertifikat und weist dann den neuen Eigentümer zu.
- Stilllegung markiert Identitäten als `RETIRED`; Auditnachweise werden nicht
  stillschweigend gelöscht.

Bis B3 implementiert und validiert ist, führt der Maintainer diese Fälle nur nach
separater Bestandsaufnahme, Freigabe und effektiver AWS-Zustandsprüfung aus.

## Verwandte Dokumente

- [Autorisierungsgrundlage](../architecture/onboarding-authorization.md)
- [Claim-Datenmodell](../architecture/onboarding-claim-data-model.md)
- [Geplanter Geräte-Lebenszyklus](../architecture/onboarding-device-lifecycle.md)
- [Administrator-Zuweisungen](../auth/admin/user-vehicle-assignments.md)
- [AWS-IoT-Zugangsdaten](../security/aws-iot-credentials.md)
- [Geräte-Provisionierungscheckliste](../beta/device-provisioning-checklist.md)
- [ONB-001.B Arbeitsumfang](../project/sprints/ONB-001-B.md)
