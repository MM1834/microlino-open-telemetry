(function () {
  'use strict';

  const STORAGE_KEY = 'mot-dashboard-language';
  const SUPPORTED = ['de', 'en', 'fr'];
  const LOCALES = { de: 'de-CH', en: 'en-GB', fr: 'fr-CH' };

  const en = {
    'Telemetrie Dashboard': 'Telemetry Dashboard', 'Übersicht': 'Overview', 'Standort': 'Location',
    'Batterie': 'Battery', 'Fahrzeug': 'Vehicle', 'Laden': 'Charging', 'Temperaturen': 'Temperatures',
    'Zellen': 'Cells', 'Einstellungen': 'Settings', 'OBD2 letztes Update': 'Last OBD2 update',
    'Verbinden…': 'Connecting…', 'AWS Vehicle API wird verbunden': 'Connecting to AWS Vehicle API',
    'Live-Kanal': 'Live channel', 'Nicht verbunden': 'Not connected', 'WebSocket wartet': 'WebSocket waiting',
    'Keine Daten': 'No data', 'Noch kein Update empfangen': 'No update received yet',
    'Netzwerk: -- · WebUI: --': 'Network: -- · Web UI: --', 'Nicht angemeldet': 'Not signed in',
    'Angemeldet bleiben': 'Stay signed in', 'Anmelden': 'Sign in', 'Abmelden': 'Sign out',
    'Fahrzeug hinzufügen': 'Add vehicle', 'Fahrzeug verbinden': 'Connect vehicle', 'Claim-Code': 'Claim code',
    'Deinem Konto ist noch kein Fahrzeug zugewiesen. Gib den einmaligen Claim-Code ein, den du mit deinem Adapter erhalten hast.': 'No vehicle is assigned to your account yet. Enter the one-time claim code supplied with your adapter.',
    'Dein Microlino. Immer im Blick.': 'Your Microlino. Always in view.', 'History ansehen →': 'View history →',
    'Reichweite': 'Range', 'Nach SoC · Basis 140 km': 'By SOC · 140 km basis', 'Stand: --': 'As of: --',
    'Geschwindigkeit': 'Speed', 'Leistung': 'Power', 'Ladestatus': 'Charging status',
    'Leistungsfluss': 'Power flow', 'Gesamtkilometer': 'Odometer', 'OpenStreetMap öffnen': 'Open OpenStreetMap',
    'Standort nicht verfügbar': 'Location unavailable', 'Verbindungsstatus': 'Connection status',
    'Spannung': 'Voltage', 'Strom': 'Current', 'Fahrzeugleistung': 'Vehicle power',
    'Reichweitenprognose': 'Range forecast', 'Noch keine ausreichende Fahrhistorie': 'Not enough driving history yet',
    'Nach SoC: -- km': 'By SOC: -- km', 'Verbrauch': 'Consumption', 'Fahrzeit': 'Driving time',
    'Ladestrom': 'Charging current', 'Ladespannung': 'Charging voltage', 'Energie': 'Energy',
    'Batterie Min': 'Battery min', 'Batterie Max': 'Battery max', 'Batterie Ø': 'Battery avg',
    'Umgebung': 'Ambient', 'Zellspannungs-Kandidaten': 'Cell-voltage candidates',
    'Zwei plausible Standard-CAN-Werte; packweite Extrema noch nicht bestätigt.': 'Two plausible standard-CAN values; pack-wide extremes are not yet confirmed.',
    'AWS · fahrzeugbezogen · 31 Tage': 'AWS · vehicle-specific · 31 days', 'SoC Verlauf': 'SOC history',
    'Noch keine SOC-History vorhanden.': 'No SOC history available yet.', 'Ø Speed Verlauf': 'Average speed history',
    'Noch keine Speed-History vorhanden.': 'No speed history available yet.', 'Ø Nettoleistung': 'Average net power',
    '− Verbrauch · + Laden/Rekuperation': '− consumption · + charging/recuperation',
    'Noch keine Leistungs-History vorhanden.': 'No power history available yet.',
    'Ladezustand & Ladekabel': 'Charging state & cable', 'Lädt': 'Charging', 'Kabel angeschlossen': 'Cable connected',
    'Noch keine Lade-/Kabel-History vorhanden.': 'No charging/cable history available yet.',
    'Ladezustand und Ladekabel als Ein-Aus-Stufenlinien': 'Charging state and cable as on/off step lines',
    'Persönlich · fahrzeugbezogen': 'Personal · vehicle-specific', 'Reichweite bei 100 % SOC': 'Range at 100% SOC',
    'Gewünschte SOC-Reserve': 'Desired SOC reserve',
    'Die angezeigte Reichweite endet bei dieser Reserve. Beispiel: 15 % zeigt die noch fahrbare Strecke bis 15 % SOC.': 'The displayed range ends at this reserve. Example: 15% shows the remaining distance until 15% SOC.',
    'Ladeziel-Benachrichtigung aktivieren': 'Enable charge-target notification', 'Ziel-SOC': 'Target SOC',
    'E-Mail-Kanal aktivieren': 'Enable email channel', 'E-Mail-Adresse': 'Email address', 'Ladestopp': 'Charging stop',
    'E-Mail, wenn das Laden nach mindestens 45 Sekunden Ladezeit vor dem Zielwert mindestens 60 Sekunden stoppt': 'Email when charging stops for at least 60 seconds before the target after at least 45 seconds of charging',
    'Ladestopp-Ziel-SOC': 'Charging-stop target SOC',
    'Nur wenn das Fahrzeug weiterhin eingesteckt ist. Ein manueller Stopp, Lastmanagement oder Solar-Nulleinspeisung kann dieselbe Meldung auslösen. Pro durchgehend eingesteckter Ladesession wird höchstens eine Ladestopp-Meldung versendet; erst Ausstecken und erneutes Einstecken startet eine neue Session.': 'Only while the vehicle remains plugged in. A manual stop, load management or solar zero-export may trigger the same message. At most one charging-stop message is sent per continuous plugged session; unplugging and reconnecting starts a new session.',
    'Fahrtzusammenfassung': 'Journey summary', 'Zusammenfassung geeigneter Fahrten per E-Mail': 'Email summaries of eligible journeys',
    'Aktiv für geeignete Fahrten · Energiequelle wird in der E-Mail ausgewiesen.': 'Active for eligible journeys · the energy source is shown in the email.',
    'SMS (Schweiz / Deutschland)': 'SMS (Switzerland / Germany)', 'Mobilnummer im Format +41… oder +49…': 'Mobile number in +41… or +49… format',
    'Bestätigungscode senden': 'Send verification code', 'Bestätigungscode': 'Verification code', 'Code bestätigen': 'Confirm code',
    'Mobilnummer noch nicht bestätigt.': 'Mobile number not yet verified.', 'SMS-Kanal für dieses Fahrzeug aktivieren': 'Enable SMS channel for this vehicle',
    'Die Nummer kann für mehrere Fahrzeuge verwendet werden. Die administrative Freigabe erfolgt für jede Benutzer–Fahrzeug-Zuordnung separat.': 'The number can be used for several vehicles. Administrative approval is granted separately for each user-vehicle association.',
    'Bestätigung erforderlich:': 'Confirmation required:',
    'Nach dem Speichern sendet': 'After saving,', 'eine technische Bestätigungsmail. Öffnen Sie diese und wählen Sie': 'sends a technical confirmation email. Open it and select',
    'Die AWS-Mail erwähnt MOT möglicherweise nicht; prüfen Sie deshalb auch den Spam-Ordner. Ohne Bestätigung bleiben E-Mail-Benachrichtigungen inaktiv.': 'The AWS email may not mention MOT; also check your spam folder. Email notifications remain inactive until confirmed.',
    'Die Meldung ist rein informativ und steuert den Ladevorgang nicht.': 'The notification is informational only and does not control charging.',
    'Speichern': 'Save', 'Beta-Onboarding verwalten': 'Manage beta onboarding',
    'Erstellt einen einmaligen Claim für eine kontrollierte Fahrzeug-ID. Der Code wird nur hier angezeigt.': 'Creates a one-time claim for a controlled vehicle ID. The code is shown only here.',
    'Fahrzeug-ID': 'Vehicle ID', 'Claim erstellen': 'Create claim', 'Claim-Anzeige leeren': 'Clear claim display',
    'Device': 'Device', 'Uptime': 'Uptime', 'Sprache': 'Language', 'Deutsch': 'German', 'Englisch': 'English', 'Französisch': 'French',
    'Online': 'Online', 'Daten veraltet': 'Data stale', 'Offline': 'Offline', 'Aktuell': 'Current',
    'Nicht aktuell': 'Not current', 'Ja': 'Yes', 'Nein': 'No', 'Ein': 'On', 'Aus': 'Off',
    'Abbrechen': 'Cancel', 'Weiteres Fahrzeug hinzufügen': 'Add another vehicle',
    'Bitte Claim-Code eingeben.': 'Please enter a claim code.', 'Fahrzeug wird zugewiesen…': 'Assigning vehicle…',
    'Fahrzeug erfolgreich zugewiesen.': 'Vehicle assigned successfully.', 'Onboarding fehlgeschlagen': 'Onboarding failed',
    'Claim wird erstellt…': 'Creating claim…', 'Claim-Ausgabe fehlgeschlagen': 'Claim creation failed',
    'Claim-Anzeige wurde geleert.': 'Claim display cleared.', 'Weiterleitung…': 'Redirecting…',
    'Login fehlgeschlagen': 'Sign-in failed', 'Abmeldung…': 'Signing out…', 'Logout fehlgeschlagen': 'Sign-out failed',
    'Cognito nicht konfiguriert': 'Cognito not configured', 'Angemeldet': 'Signed in', 'Rekuperation': 'Recuperation',
    'noch kein Messpunkt': 'no measurement yet', 'letzter Messpunkt': 'last measurement',
    'Stillstand oder offline': 'stationary or offline', 'History-Aktualisierung fehlgeschlagen · letzte Daten bleiben sichtbar': 'History update failed · last data remains visible',
    'Auflösung': 'Resolution', 'Punkte': 'points', 'letzte': 'last', 'Kabel': 'cable', 'Intervalle': 'intervals',
    'Anmeldung erforderlich': 'Sign-in required', 'Stand: unbekannt': 'As of: unknown',
    'Kein Standort': 'No location', 'Noch keine Standortdaten': 'No location data yet',
    'Default Standort': 'Default location', 'Default Standort aus config.js': 'Default location from config.js',
    'Letzter Standort': 'Last location', 'Aktueller Standort': 'Current location',
    'Zeitpunkt nicht verfügbar': 'Timestamp unavailable', 'History': 'History', 'Topic': 'Topic',
    'Nicht am Laden': 'Not charging', 'Eingesteckt': 'Plugged in', 'Bereit': 'Ready',
    'Verbunden': 'Connected', 'Getrennt': 'Disconnected', 'MQTT getrennt': 'MQTT disconnected',
    'Verbunden mit MQTT': 'Connected to MQTT', 'Verbunden mit AWS Vehicle API': 'Connected to AWS Vehicle API',
    'Lokale Geräte-WebUI öffnen': 'Open local device web UI',
    'Diese IP ist nur im gleichen lokalen Netzwerk erreichbar': 'This IP is reachable only on the same local network',
    'Diese IP ist nur im gleichen lokalen WLAN erreichbar': 'This IP is reachable only on the same local Wi-Fi',
    'Noch keine Geräte-IP über MQTT empfangen': 'No device IP received through MQTT yet',
    'Persönliche Prognose': 'Personal forecast', 'bis': 'to', 'Fahrt': 'journey', 'Fahrten': 'journeys',
    'aktuell': 'current', 'veraltet': 'stale', 'Letztes Update': 'Last update',
    'Gib den einmaligen Claim-Code des zusätzlichen Fahrzeugs ein. Bestehende Fahrzeuge bleiben zugewiesen.': 'Enter the additional vehicle’s one-time claim code. Existing vehicles remain assigned.',
    'Claim für': 'Claim for', 'erstellt; gültig bis': 'created; valid until',
    'E-Mail-Adresse bestätigt': 'Email address confirmed', 'Bestätigung ausstehend': 'Confirmation pending',
    'E-Mail deaktiviert': 'Email disabled', 'SMS-Status vorübergehend nicht verfügbar.': 'SMS status temporarily unavailable.',
    'Bestätigungscode ausstehend.': 'Verification code pending.',
    'Mobilnummer bestätigt und administrativ freigegeben.': 'Mobile number verified and administratively approved.',
    'Mobilnummer bestätigt; administrative Freigabe ausstehend.': 'Mobile number verified; administrative approval pending.',
    'Neue E-Mail-Adresse muss bestätigt werden': 'The new email address must be confirmed',
    'SMS-Status nicht verfügbar': 'SMS status unavailable',
    'Ein Teil der Einstellungen ist vorübergehend nicht verfügbar.': 'Some settings are temporarily unavailable.',
    'Für Fahrtzusammenfassungen zuerst den E-Mail-Kanal aktivieren.': 'Enable the email channel before journey summaries.',
    'Für Ladestopp-Meldungen zuerst den E-Mail-Kanal aktivieren.': 'Enable the email channel before charging-stop notifications.',
    'Bestätigungscode wird angefordert…': 'Requesting verification code…',
    'Bereits bestätigte Mobilnummer übernommen.': 'Previously verified mobile number adopted.',
    'Bestätigungscode gesendet.': 'Verification code sent.',
    'Bitte mindestens 60 Sekunden bis zum nächsten Code warten.': 'Please wait at least 60 seconds before requesting another code.',
    'Bestätigungscode konnte nicht gesendet werden.': 'Verification code could not be sent.',
    'Code wird geprüft…': 'Checking code…', 'Mobilnummer bestätigt.': 'Mobile number verified.',
    'Code konnte nicht bestätigt werden.': 'Code could not be confirmed.', 'Sitzung wird geprüft…': 'Checking session…',
    'Keine aktive Fahrzeugzuordnung': 'No active vehicle assignment', 'Kein Fahrzeug ausgewählt': 'No vehicle selected',
    'Fahrzeug ist bereits zugewiesen oder nicht verfügbar': 'Vehicle is already assigned or unavailable',
    'Claim ungültig oder nicht mehr verfügbar': 'Claim invalid or no longer available',
    'Ungültige WebSocket-Nachricht:': 'Invalid WebSocket message:',
    'mqtt.min.js fehlt oder ist ungültig': 'mqtt.min.js is missing or invalid',
    'Abonniert:': 'Subscribed:', 'WebSocket verbunden': 'WebSocket connected',
    'Wiederverbinden…': 'Reconnecting…', 'Deaktiviert': 'Disabled', 'gerade eben': 'just now',
    'Stand:': 'As of:', 'Letzte Aktualisierung': 'Last update', 'Basierend auf': 'Based on', 'Basis': 'basis',
    'Nach SoC:': 'By SOC:',
    'Demo-Zugang: Benachrichtigungen sind deaktiviert.': 'Demo access: notifications are disabled.'
  };

  const fr = {
    'Telemetrie Dashboard': 'Tableau de bord télémétrique', 'Übersicht': 'Vue d’ensemble', 'Standort': 'Position',
    'Batterie': 'Batterie', 'Fahrzeug': 'Véhicule', 'Laden': 'Recharge', 'Temperaturen': 'Températures',
    'Zellen': 'Cellules', 'Einstellungen': 'Réglages', 'OBD2 letztes Update': 'Dernière mise à jour OBD2',
    'Verbinden…': 'Connexion…', 'AWS Vehicle API wird verbunden': 'Connexion à l’API AWS Vehicle',
    'Live-Kanal': 'Canal en direct', 'Nicht verbunden': 'Non connecté', 'WebSocket wartet': 'WebSocket en attente',
    'Keine Daten': 'Aucune donnée', 'Noch kein Update empfangen': 'Aucune mise à jour reçue',
    'Netzwerk: -- · WebUI: --': 'Réseau : -- · Interface web : --', 'Nicht angemeldet': 'Non connecté',
    'Angemeldet bleiben': 'Rester connecté', 'Anmelden': 'Se connecter', 'Abmelden': 'Se déconnecter',
    'Fahrzeug hinzufügen': 'Ajouter un véhicule', 'Fahrzeug verbinden': 'Associer un véhicule', 'Claim-Code': 'Code de rattachement',
    'Deinem Konto ist noch kein Fahrzeug zugewiesen. Gib den einmaligen Claim-Code ein, den du mit deinem Adapter erhalten hast.': 'Aucun véhicule n’est encore associé à votre compte. Saisissez le code de rattachement à usage unique fourni avec votre adaptateur.',
    'Dein Microlino. Immer im Blick.': 'Votre Microlino. Toujours en vue.', 'History ansehen →': 'Voir l’historique →',
    'Reichweite': 'Autonomie', 'Nach SoC · Basis 140 km': 'Selon le SOC · base 140 km', 'Stand: --': 'État : --',
    'Geschwindigkeit': 'Vitesse', 'Leistung': 'Puissance', 'Ladestatus': 'État de charge',
    'Leistungsfluss': 'Flux de puissance', 'Gesamtkilometer': 'Kilométrage total', 'OpenStreetMap öffnen': 'Ouvrir OpenStreetMap',
    'Standort nicht verfügbar': 'Position indisponible', 'Verbindungsstatus': 'État de connexion',
    'Spannung': 'Tension', 'Strom': 'Courant', 'Fahrzeugleistung': 'Puissance du véhicule',
    'Reichweitenprognose': 'Prévision d’autonomie', 'Noch keine ausreichende Fahrhistorie': 'Historique de conduite encore insuffisant',
    'Nach SoC: -- km': 'Selon le SOC : -- km', 'Verbrauch': 'Consommation', 'Fahrzeit': 'Temps de conduite',
    'Ladestrom': 'Courant de charge', 'Ladespannung': 'Tension de charge', 'Energie': 'Énergie',
    'Batterie Min': 'Batterie min.', 'Batterie Max': 'Batterie max.', 'Batterie Ø': 'Batterie moy.',
    'Umgebung': 'Ambiante', 'Zellspannungs-Kandidaten': 'Valeurs candidates des cellules',
    'Zwei plausible Standard-CAN-Werte; packweite Extrema noch nicht bestätigt.': 'Deux valeurs CAN standard plausibles ; les valeurs extrêmes du pack ne sont pas encore confirmées.',
    'AWS · fahrzeugbezogen · 31 Tage': 'AWS · par véhicule · 31 jours', 'SoC Verlauf': 'Historique du SOC',
    'Noch keine SOC-History vorhanden.': 'Aucun historique SOC disponible.', 'Ø Speed Verlauf': 'Historique de la vitesse moyenne',
    'Noch keine Speed-History vorhanden.': 'Aucun historique de vitesse disponible.', 'Ø Nettoleistung': 'Puissance nette moyenne',
    '− Verbrauch · + Laden/Rekuperation': '− consommation · + charge/récupération',
    'Noch keine Leistungs-History vorhanden.': 'Aucun historique de puissance disponible.',
    'Ladezustand & Ladekabel': 'État de charge et câble', 'Lädt': 'En charge', 'Kabel angeschlossen': 'Câble branché',
    'Noch keine Lade-/Kabel-History vorhanden.': 'Aucun historique de charge/câble disponible.',
    'Ladezustand und Ladekabel als Ein-Aus-Stufenlinien': 'État de charge et câble sous forme de courbes en escalier marche/arrêt',
    'Persönlich · fahrzeugbezogen': 'Personnel · par véhicule', 'Reichweite bei 100 % SOC': 'Autonomie à 100 % de SOC',
    'Gewünschte SOC-Reserve': 'Réserve SOC souhaitée',
    'Die angezeigte Reichweite endet bei dieser Reserve. Beispiel: 15 % zeigt die noch fahrbare Strecke bis 15 % SOC.': 'L’autonomie affichée s’arrête à cette réserve. Exemple : 15 % indique la distance restante jusqu’à 15 % de SOC.',
    'Ladeziel-Benachrichtigung aktivieren': 'Activer la notification d’objectif de charge', 'Ziel-SOC': 'SOC cible',
    'E-Mail-Kanal aktivieren': 'Activer le canal e-mail', 'E-Mail-Adresse': 'Adresse e-mail', 'Ladestopp': 'Arrêt de charge',
    'E-Mail, wenn das Laden nach mindestens 45 Sekunden Ladezeit vor dem Zielwert mindestens 60 Sekunden stoppt': 'E-mail si la charge s’arrête pendant au moins 60 secondes avant la cible, après au moins 45 secondes de charge',
    'Ladestopp-Ziel-SOC': 'SOC cible pour l’arrêt de charge',
    'Nur wenn das Fahrzeug weiterhin eingesteckt ist. Ein manueller Stopp, Lastmanagement oder Solar-Nulleinspeisung kann dieselbe Meldung auslösen. Pro durchgehend eingesteckter Ladesession wird höchstens eine Ladestopp-Meldung versendet; erst Ausstecken und erneutes Einstecken startet eine neue Session.': 'Uniquement si le véhicule reste branché. Un arrêt manuel, la gestion de charge ou le zéro injection solaire peut déclencher le même message. Un seul message d’arrêt de charge est envoyé par session branchée continue ; débrancher puis rebrancher démarre une nouvelle session.',
    'Fahrtzusammenfassung': 'Résumé du trajet', 'Zusammenfassung geeigneter Fahrten per E-Mail': 'Résumé par e-mail des trajets admissibles',
    'Aktiv für geeignete Fahrten · Energiequelle wird in der E-Mail ausgewiesen.': 'Actif pour les trajets admissibles · la source d’énergie est indiquée dans l’e-mail.',
    'SMS (Schweiz / Deutschland)': 'SMS (Suisse / Allemagne)', 'Mobilnummer im Format +41… oder +49…': 'Numéro mobile au format +41… ou +49…',
    'Bestätigungscode senden': 'Envoyer le code de vérification', 'Bestätigungscode': 'Code de vérification', 'Code bestätigen': 'Confirmer le code',
    'Mobilnummer noch nicht bestätigt.': 'Numéro mobile pas encore vérifié.', 'SMS-Kanal für dieses Fahrzeug aktivieren': 'Activer le canal SMS pour ce véhicule',
    'Die Nummer kann für mehrere Fahrzeuge verwendet werden. Die administrative Freigabe erfolgt für jede Benutzer–Fahrzeug-Zuordnung separat.': 'Le numéro peut être utilisé pour plusieurs véhicules. L’autorisation administrative est accordée séparément pour chaque association utilisateur-véhicule.',
    'Bestätigung erforderlich:': 'Confirmation requise :', 'Nach dem Speichern sendet': 'Après l’enregistrement,',
    'eine technische Bestätigungsmail. Öffnen Sie diese und wählen Sie': 'envoie un e-mail technique de confirmation. Ouvrez-le et sélectionnez',
    'Die AWS-Mail erwähnt MOT möglicherweise nicht; prüfen Sie deshalb auch den Spam-Ordner. Ohne Bestätigung bleiben E-Mail-Benachrichtigungen inaktiv.': 'L’e-mail AWS peut ne pas mentionner MOT ; vérifiez aussi le dossier indésirable. Les notifications par e-mail restent inactives sans confirmation.',
    'Die Meldung ist rein informativ und steuert den Ladevorgang nicht.': 'La notification est purement informative et ne commande pas la charge.',
    'Speichern': 'Enregistrer', 'Beta-Onboarding verwalten': 'Gérer l’onboarding bêta',
    'Erstellt einen einmaligen Claim für eine kontrollierte Fahrzeug-ID. Der Code wird nur hier angezeigt.': 'Crée un rattachement à usage unique pour un identifiant de véhicule contrôlé. Le code n’est affiché qu’ici.',
    'Fahrzeug-ID': 'Identifiant du véhicule', 'Claim erstellen': 'Créer le rattachement', 'Claim-Anzeige leeren': 'Effacer le code affiché',
    'Device': 'Appareil', 'Uptime': 'Durée de fonctionnement', 'Sprache': 'Langue', 'Deutsch': 'Allemand', 'Englisch': 'Anglais', 'Französisch': 'Français',
    'Online': 'En ligne', 'Daten veraltet': 'Données anciennes', 'Offline': 'Hors ligne', 'Aktuell': 'À jour',
    'Nicht aktuell': 'Pas à jour', 'Ja': 'Oui', 'Nein': 'Non', 'Ein': 'Activé', 'Aus': 'Désactivé',
    'Abbrechen': 'Annuler', 'Weiteres Fahrzeug hinzufügen': 'Ajouter un autre véhicule',
    'Bitte Claim-Code eingeben.': 'Veuillez saisir un code de rattachement.', 'Fahrzeug wird zugewiesen…': 'Association du véhicule…',
    'Fahrzeug erfolgreich zugewiesen.': 'Véhicule associé avec succès.', 'Onboarding fehlgeschlagen': 'Échec de l’onboarding',
    'Claim wird erstellt…': 'Création du rattachement…', 'Claim-Ausgabe fehlgeschlagen': 'Échec de la création du rattachement',
    'Claim-Anzeige wurde geleert.': 'Code affiché effacé.', 'Weiterleitung…': 'Redirection…',
    'Login fehlgeschlagen': 'Échec de la connexion', 'Abmeldung…': 'Déconnexion…', 'Logout fehlgeschlagen': 'Échec de la déconnexion',
    'Cognito nicht konfiguriert': 'Cognito non configuré', 'Angemeldet': 'Connecté', 'Rekuperation': 'Récupération',
    'noch kein Messpunkt': 'aucune mesure', 'letzter Messpunkt': 'dernière mesure',
    'Stillstand oder offline': 'à l’arrêt ou hors ligne', 'History-Aktualisierung fehlgeschlagen · letzte Daten bleiben sichtbar': 'Échec de la mise à jour de l’historique · les dernières données restent visibles',
    'Auflösung': 'Résolution', 'Punkte': 'points', 'letzte': 'derniers', 'Kabel': 'câble', 'Intervalle': 'intervalles',
    'Anmeldung erforderlich': 'Connexion requise', 'Stand: unbekannt': 'État : inconnu',
    'Kein Standort': 'Aucune position', 'Noch keine Standortdaten': 'Aucune donnée de position',
    'Default Standort': 'Position par défaut', 'Default Standort aus config.js': 'Position par défaut selon config.js',
    'Letzter Standort': 'Dernière position', 'Aktueller Standort': 'Position actuelle',
    'Zeitpunkt nicht verfügbar': 'Horodatage indisponible', 'History': 'Historique', 'Topic': 'Sujet',
    'Nicht am Laden': 'Pas en charge', 'Eingesteckt': 'Branché', 'Bereit': 'Prêt',
    'Verbunden': 'Connecté', 'Getrennt': 'Déconnecté', 'MQTT getrennt': 'MQTT déconnecté',
    'Verbunden mit MQTT': 'Connecté à MQTT', 'Verbunden mit AWS Vehicle API': 'Connecté à l’API AWS Vehicle',
    'Lokale Geräte-WebUI öffnen': 'Ouvrir l’interface web locale de l’appareil',
    'Diese IP ist nur im gleichen lokalen Netzwerk erreichbar': 'Cette adresse IP est accessible uniquement sur le même réseau local',
    'Diese IP ist nur im gleichen lokalen WLAN erreichbar': 'Cette adresse IP est accessible uniquement sur le même Wi-Fi local',
    'Noch keine Geräte-IP über MQTT empfangen': 'Aucune adresse IP de l’appareil reçue par MQTT',
    'Persönliche Prognose': 'Prévision personnelle', 'bis': 'jusqu’à', 'Fahrt': 'trajet', 'Fahrten': 'trajets',
    'aktuell': 'à jour', 'veraltet': 'ancien', 'Letztes Update': 'Dernière mise à jour',
    'Gib den einmaligen Claim-Code des zusätzlichen Fahrzeugs ein. Bestehende Fahrzeuge bleiben zugewiesen.': 'Saisissez le code de rattachement à usage unique du véhicule supplémentaire. Les véhicules existants restent associés.',
    'Claim für': 'Rattachement pour', 'erstellt; gültig bis': 'créé ; valable jusqu’au',
    'E-Mail-Adresse bestätigt': 'Adresse e-mail confirmée', 'Bestätigung ausstehend': 'Confirmation en attente',
    'E-Mail deaktiviert': 'E-mail désactivé', 'SMS-Status vorübergehend nicht verfügbar.': 'État SMS temporairement indisponible.',
    'Bestätigungscode ausstehend.': 'Code de vérification en attente.',
    'Mobilnummer bestätigt und administrativ freigegeben.': 'Numéro mobile vérifié et approuvé administrativement.',
    'Mobilnummer bestätigt; administrative Freigabe ausstehend.': 'Numéro mobile vérifié ; approbation administrative en attente.',
    'Neue E-Mail-Adresse muss bestätigt werden': 'La nouvelle adresse e-mail doit être confirmée',
    'SMS-Status nicht verfügbar': 'État SMS indisponible',
    'Ein Teil der Einstellungen ist vorübergehend nicht verfügbar.': 'Certains réglages sont temporairement indisponibles.',
    'Für Fahrtzusammenfassungen zuerst den E-Mail-Kanal aktivieren.': 'Activez le canal e-mail avant les résumés de trajet.',
    'Für Ladestopp-Meldungen zuerst den E-Mail-Kanal aktivieren.': 'Activez le canal e-mail avant les notifications d’arrêt de charge.',
    'Bestätigungscode wird angefordert…': 'Demande du code de vérification…',
    'Bereits bestätigte Mobilnummer übernommen.': 'Numéro mobile déjà vérifié repris.',
    'Bestätigungscode gesendet.': 'Code de vérification envoyé.',
    'Bitte mindestens 60 Sekunden bis zum nächsten Code warten.': 'Veuillez attendre au moins 60 secondes avant de demander un nouveau code.',
    'Bestätigungscode konnte nicht gesendet werden.': 'Le code de vérification n’a pas pu être envoyé.',
    'Code wird geprüft…': 'Vérification du code…', 'Mobilnummer bestätigt.': 'Numéro mobile vérifié.',
    'Code konnte nicht bestätigt werden.': 'Le code n’a pas pu être confirmé.', 'Sitzung wird geprüft…': 'Vérification de la session…',
    'Keine aktive Fahrzeugzuordnung': 'Aucune association de véhicule active', 'Kein Fahrzeug ausgewählt': 'Aucun véhicule sélectionné',
    'Fahrzeug ist bereits zugewiesen oder nicht verfügbar': 'Le véhicule est déjà associé ou indisponible',
    'Claim ungültig oder nicht mehr verfügbar': 'Rattachement invalide ou plus disponible',
    'Ungültige WebSocket-Nachricht:': 'Message WebSocket invalide :',
    'mqtt.min.js fehlt oder ist ungültig': 'mqtt.min.js est absent ou invalide',
    'Abonniert:': 'Abonné :', 'WebSocket verbunden': 'WebSocket connecté',
    'Wiederverbinden…': 'Reconnexion…', 'Deaktiviert': 'Désactivé', 'gerade eben': 'à l’instant',
    'Stand:': 'État :', 'Letzte Aktualisierung': 'Dernière mise à jour', 'Basierend auf': 'Basé sur', 'Basis': 'base',
    'Nach SoC:': 'Selon le SOC :',
    'Demo-Zugang: Benachrichtigungen sind deaktiviert.': 'Accès démo : les notifications sont désactivées.'
  };

  const dictionaries = { de: {}, en, fr };
  const sourceText = new WeakMap();
  const sourceAttributes = new WeakMap();
  let applying = false;

  function preferredLanguage() {
    const configured = String(window.MOT_CONFIG?.dashboard?.language || '').slice(0, 2).toLowerCase();
    let stored = '';
    try { stored = String(localStorage.getItem(STORAGE_KEY) || ''); } catch (_error) {}
    return [stored, configured, 'de'].find(value => SUPPORTED.includes(value)) || 'de';
  }

  let language = preferredLanguage();

  function translate(source, target = language) {
    if (!source || target === 'de') return source;
    const dictionary = dictionaries[target] || {};
    if (dictionary[source]) return dictionary[source];
    let result = source;
    if (target === 'en') {
      result = result
        .replace(/\bvor (\d+) s\b/g, '$1 s ago')
        .replace(/\bvor (\d+) min\b/g, '$1 min ago')
        .replace(/\bvor (\d+) h\b/g, '$1 h ago')
        .replace(/\bvor (\d+) Tagen\b/g, '$1 days ago')
        .replace(/ · bis (\d+)%/g, ' · to $1%');
    } else if (target === 'fr') {
      result = result
        .replace(/\bvor (\d+) s\b/g, 'il y a $1 s')
        .replace(/\bvor (\d+) min\b/g, 'il y a $1 min')
        .replace(/\bvor (\d+) h\b/g, 'il y a $1 h')
        .replace(/\bvor (\d+) Tagen\b/g, 'il y a $1 jours')
        .replace(/ · bis (\d+)%/g, ' · jusqu’à $1 %');
    }
    Object.keys(dictionary).sort((a, b) => b.length - a.length).forEach(key => {
      if (key.length >= 4 && result.includes(key)) result = result.split(key).join(dictionary[key]);
    });
    return result;
  }

  function rememberAttribute(element, name) {
    let values = sourceAttributes.get(element);
    if (!values) { values = {}; sourceAttributes.set(element, values); }
    if (!(name in values)) values[name] = element.getAttribute(name);
    return values[name];
  }

  function applyElement(root = document) {
    applying = true;
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
    let node;
    while ((node = walker.nextNode())) {
      if (!node.nodeValue.trim() || ['SCRIPT', 'STYLE'].includes(node.parentElement?.tagName)) continue;
      if (!sourceText.has(node)) sourceText.set(node, node.nodeValue);
      node.nodeValue = translate(sourceText.get(node));
    }
    const elements = root.querySelectorAll ? [root, ...root.querySelectorAll('[aria-label],[title],[placeholder]')] : [];
    elements.forEach(element => ['aria-label', 'title', 'placeholder'].forEach(name => {
      if (!element?.hasAttribute?.(name)) return;
      element.setAttribute(name, translate(rememberAttribute(element, name)));
    }));
    document.documentElement.lang = language;
    const selector = document.getElementById('dashboard-language');
    if (selector) selector.value = language;
    window.setTimeout(() => { applying = false; }, 0);
  }

  function setLanguage(next) {
    if (!SUPPORTED.includes(next)) return;
    language = next;
    try { localStorage.setItem(STORAGE_KEY, next); } catch (_error) {}
    applyElement(document);
    window.dispatchEvent(new CustomEvent('mot-language-change', { detail: { language, locale: LOCALES[language] } }));
  }

  function observe() {
    const observer = new MutationObserver(records => {
      if (applying) return;
      records.forEach(record => {
        if (record.type === 'characterData') sourceText.set(record.target, record.target.nodeValue);
        record.addedNodes.forEach(node => {
          if (node.nodeType === Node.TEXT_NODE) sourceText.set(node, node.nodeValue);
        });
      });
      applyElement(document);
    });
    observer.observe(document.body, { subtree: true, childList: true, characterData: true });
  }

  window.MOT_I18N = {
    get language() { return language; },
    get locale() { return LOCALES[language]; },
    supported: [...SUPPORTED], translate, setLanguage, apply: applyElement
  };

  document.addEventListener('DOMContentLoaded', () => {
    applyElement(document);
    document.getElementById('dashboard-language')?.addEventListener('change', event => setLanguage(event.target.value));
    observe();
  });
})();
