"""Localized MOT notification email subjects and plain-text bodies."""

SUPPORTED_LANGUAGES = {"de", "en", "fr", "it"}
DEFAULT_LANGUAGE = "de"


def language(preference):
    value = str((preference or {}).get("notificationLanguage") or DEFAULT_LANGUAGE).lower()
    return value if value in SUPPORTED_LANGUAGES else DEFAULT_LANGUAGE


def number(value, digits=1, lang="de"):
    rendered = f"{float(value):.{digits}f}"
    return rendered.replace(".", ",") if lang in {"de", "fr", "it"} else rendered


def _unknown(lang):
    return {"de": "unbekannt", "en": "unknown", "fr": "inconnu", "it": "sconosciuto"}[lang]


def soc_target(preference, vehicle_id, vehicle_name, threshold, reached_soc):
    lang = language(preference)
    values = {
        "de": (f"MOT - {vehicle_id} hat {threshold:g}% erreicht", f"MOT: {vehicle_name} ({vehicle_id}) hat beim Laden {reached_soc:g}% SOC erreicht (Ziel {threshold:g}%). Info, keine Ladesteuerung."),
        "en": (f"MOT - {vehicle_id} reached {threshold:g}%", f"MOT: {vehicle_name} ({vehicle_id}) reached {reached_soc:g}% SOC while charging (target {threshold:g}%). Information only; no charging control."),
        "fr": (f"MOT - {vehicle_id} a atteint {threshold:g} %", f"MOT : {vehicle_name} ({vehicle_id}) a atteint {reached_soc:g} % de SOC pendant la recharge (objectif {threshold:g} %). Information uniquement ; aucune commande de recharge."),
        "it": (f"MOT - {vehicle_id} ha raggiunto il {threshold:g}%", f"MOT: {vehicle_name} ({vehicle_id}) ha raggiunto il {reached_soc:g}% di SOC durante la ricarica (obiettivo {threshold:g}%). Solo informazione; nessun controllo della ricarica."),
    }
    return values[lang]


def charging_stop(preference, vehicle_id, vehicle_name, stopped_soc, threshold):
    lang = language(preference)
    values = {
        "de": (f"MOT - Ladestopp bei {stopped_soc:g}% ({vehicle_id})", f"MOT: Der Ladevorgang von {vehicle_name} ({vehicle_id}) ist bei {stopped_soc:g}% SOC seit mindestens 60 Sekunden gestoppt (Ladestopp-Ziel {threshold:g}%). Das Fahrzeug ist weiterhin eingesteckt. Dies kann auch ein manueller Stopp oder externes Lastmanagement sein. Info, keine Ladesteuerung."),
        "en": (f"MOT - Charging stopped at {stopped_soc:g}% ({vehicle_id})", f"MOT: Charging of {vehicle_name} ({vehicle_id}) has been stopped at {stopped_soc:g}% SOC for at least 60 seconds (target {threshold:g}%). The vehicle remains plugged in. This may also be a manual stop or external load management. Information only; no charging control."),
        "fr": (f"MOT - Recharge arrêtée à {stopped_soc:g} % ({vehicle_id})", f"MOT : la recharge de {vehicle_name} ({vehicle_id}) est arrêtée à {stopped_soc:g} % de SOC depuis au moins 60 secondes (objectif {threshold:g} %). Le véhicule reste branché. Il peut aussi s'agir d'un arrêt manuel ou d'une gestion de charge externe. Information uniquement ; aucune commande de recharge."),
        "it": (f"MOT - Ricarica interrotta al {stopped_soc:g}% ({vehicle_id})", f"MOT: la ricarica di {vehicle_name} ({vehicle_id}) è ferma al {stopped_soc:g}% di SOC da almeno 60 secondi (obiettivo {threshold:g}%). Il veicolo è ancora collegato. Può trattarsi anche di un arresto manuale o di gestione esterna del carico. Solo informazione; nessun controllo della ricarica."),
    }
    return values[lang]


def sms_soc_target(preference, vehicle_id, reached_soc, threshold):
    lang = language(preference)
    return {
        "de": f"MOT: {vehicle_id} hat beim Laden {reached_soc:g}% SOC erreicht (Ziel {threshold:g}%). Info, keine Ladesteuerung.",
        "en": f"MOT: {vehicle_id} reached {reached_soc:g}% SOC while charging (target {threshold:g}%). Information only; no charging control.",
        "fr": f"MOT: {vehicle_id} a atteint {reached_soc:g}% SOC en recharge (objectif {threshold:g}%). Information uniquement; aucune commande de recharge.",
        "it": f"MOT: {vehicle_id} ha raggiunto {reached_soc:g}% SOC in ricarica (obiettivo {threshold:g}%). Solo informazione; nessun controllo ricarica.",
    }[lang]


def sms_charging_stop(preference, vehicle_id, stopped_soc, threshold):
    lang = language(preference)
    return {
        "de": f"MOT: {vehicle_id} Ladestopp bei {stopped_soc:g}% (Ziel {threshold:g}%). Seit 60s nicht am Laden, weiterhin eingesteckt. Info, keine Ladesteuerung.",
        "en": f"MOT: {vehicle_id} charging stopped at {stopped_soc:g}% (target {threshold:g}%). Not charging for 60s, still plugged in. Information only.",
        "fr": f"MOT: recharge de {vehicle_id} arretee a {stopped_soc:g}% (objectif {threshold:g}%). Sans recharge depuis 60s, vehicule branche. Information uniquement.",
        "it": f"MOT: ricarica di {vehicle_id} ferma al {stopped_soc:g}% (obiettivo {threshold:g}%). Non carica da 60s, ancora collegato. Solo informazione.",
    }[lang]


def charging_summary(preference, vehicle_id, name, state, duration, reason,
                     capacity_kwh=None, coverage_percent=None,
                     soc_estimate_kwh=None):
    lang = language(preference); unknown = _unknown(lang)
    start = unknown if state.start_soc is None else f"{state.start_soc:g}%"
    end = unknown if state.last_soc is None else f"{state.last_soc:g}%"
    delta = unknown if state.start_soc is None or state.last_soc is None else f"{state.last_soc-state.start_soc:+g}"
    n = lambda value, digits: number(value, digits, lang)
    incomplete = coverage_percent is not None and coverage_percent < 95
    measured_labels = {
        "de": "Gemessene geladene Energie (Mindestwert)" if incomplete else "Gemessene geladene Energie",
        "en": "Measured energy charged (minimum)" if incomplete else "Measured energy charged",
        "fr": "Énergie rechargée mesurée (minimum)" if incomplete else "Énergie rechargée mesurée",
        "it": "Energia caricata misurata (minimo)" if incomplete else "Energia caricata misurata",
    }
    estimate_labels = {"de": "SOC-basierte Schätzung", "en": "SOC-based estimate",
                       "fr": "Estimation basée sur le SOC", "it": "Stima basata sul SOC"}
    coverage_labels = {"de": "Datenabdeckung Leistung", "en": "Power-data coverage",
                       "fr": "Couverture des données de puissance", "it": "Copertura dati di potenza"}
    profile_labels = {"de": "Batterieprofil", "en": "battery profile",
                      "fr": "profil de batterie", "it": "profilo batteria"}
    quality_notes = {
        "de": "Unvollständig wegen Telemetrieunterbruch." if incomplete else "Leistungsdaten weitgehend vollständig.",
        "en": "Incomplete due to a telemetry interruption." if incomplete else "Power data largely complete.",
        "fr": "Incomplète en raison d’une interruption de télémétrie." if incomplete else "Données de puissance largement complètes.",
        "it": "Incompleta a causa di un’interruzione della telemetria." if incomplete else "Dati di potenza sostanzialmente completi.",
    }
    measured = f"{measured_labels[lang]}: {n(state.energy_kwh, 2)} kWh"
    estimate = (f"{estimate_labels[lang]}: {n(soc_estimate_kwh, 2)} kWh "
                f"({n(capacity_kwh, 1)} kWh {profile_labels[lang]})"
                if soc_estimate_kwh is not None and capacity_kwh is not None
                else f"{estimate_labels[lang]}: {unknown}")
    coverage = (f"{coverage_labels[lang]}: {n(coverage_percent, 0)}% - {quality_notes[lang]}"
                if coverage_percent is not None else f"{coverage_labels[lang]}: {unknown}")
    values = {
        "de": (f"MOT - Ladezusammenfassung {end} ({vehicle_id})", f"MOT Ladezusammenfassung für {name} ({vehicle_id})\n\nStart-SOC (Display-CAN): {start}\nEnd-SOC (Display-CAN): {end}\nSOC-Änderung: {delta} Prozentpunkte\nDauer: {duration} Minuten\n{measured}\n{estimate}\n{coverage}\nAbschluss: {'Fahrzeug ausgesteckt' if reason == 'unplugged' else '10 Minuten nicht mehr geladen'}\n\nPassive Telemetrie; keine Ladesteuerung."),
        "en": (f"MOT - Charging summary {end} ({vehicle_id})", f"MOT charging summary for {name} ({vehicle_id})\n\nStart SOC (Display CAN): {start}\nEnd SOC (Display CAN): {end}\nSOC change: {delta} percentage points\nDuration: {duration} minutes\n{measured}\n{estimate}\n{coverage}\nCompleted: {'vehicle unplugged' if reason == 'unplugged' else 'not charging for 10 minutes'}\n\nPassive telemetry; no charging control."),
        "fr": (f"MOT - Résumé de recharge {end} ({vehicle_id})", f"Résumé de recharge MOT pour {name} ({vehicle_id})\n\nSOC initial (Display CAN) : {start}\nSOC final (Display CAN) : {end}\nVariation du SOC : {delta} points de pourcentage\nDurée : {duration} minutes\n{measured}\n{estimate}\n{coverage}\nFin : {'véhicule débranché' if reason == 'unplugged' else 'aucune recharge pendant 10 minutes'}\n\nTélémétrie passive ; aucune commande de recharge."),
        "it": (f"MOT - Riepilogo ricarica {end} ({vehicle_id})", f"Riepilogo ricarica MOT per {name} ({vehicle_id})\n\nSOC iniziale (Display CAN): {start}\nSOC finale (Display CAN): {end}\nVariazione SOC: {delta} punti percentuali\nDurata: {duration} minuti\n{measured}\n{estimate}\n{coverage}\nConclusione: {'veicolo scollegato' if reason == 'unplugged' else 'nessuna ricarica per 10 minuti'}\n\nTelemetria passiva; nessun controllo della ricarica."),
    }
    return values[lang]


def journey_summary(preference, vehicle_id, name, summary):
    lang = language(preference); n = lambda value, digits=1: number(value, digits, lang)
    timeout = summary.completion_trigger == "telemetry_timeout"
    source_key = "firmware" if summary.source_flag == "Firmware-Zähler" else "telemetry"
    source = {
        "de": {"firmware": "Firmware-Zähler", "telemetry": "Telemetrie-Schätzung"},
        "en": {"firmware": "firmware counter", "telemetry": "telemetry estimate"},
        "fr": {"firmware": "compteur du firmware", "telemetry": "estimation télémétrique"},
        "it": {"firmware": "contatore firmware", "telemetry": "stima telemetrica"},
    }[lang][source_key]
    values = {
        "de": (f"MOT - Fahrt {summary.distance_km:.0f} km mit {name}", f"MOT: Fahrt mit {name} ({vehicle_id}) abgeschlossen.\n\nStrecke: {n(summary.distance_km, 0)} km\nFahrzeit: {summary.duration_minutes} min\nVerbrauchter SOC: {n(summary.soc_used)} %-Punkte\nEnergie bezogen: {n(summary.energy_drawn_kwh, 2)} kWh\nRekuperiert: {n(summary.energy_regen_kwh, 2)} kWh\nVerbrauchte Netto-Leistung: {n(summary.energy_net_kwh, 2)} kWh\nNettoverbrauch: {n(summary.net_kwh_per_100_km)} kWh/100 km\n\n{'Fahrtende: 30-Min.-Telemetrie-Timeout (Werte bis zum letzten empfangenen Signal)\n\n' if timeout else ''}Energiequelle: {source}\nInfo, keine Abrechnungs- oder Präzisionsmessung."),
        "en": (f"MOT - {summary.distance_km:.0f} km journey with {name}", f"MOT: Journey with {name} ({vehicle_id}) completed.\n\nDistance: {n(summary.distance_km, 0)} km\nDriving time: {summary.duration_minutes} min\nSOC used: {n(summary.soc_used)} percentage points\nEnergy drawn: {n(summary.energy_drawn_kwh, 2)} kWh\nRecuperated: {n(summary.energy_regen_kwh, 2)} kWh\nNet energy used: {n(summary.energy_net_kwh, 2)} kWh\nNet consumption: {n(summary.net_kwh_per_100_km)} kWh/100 km\n\n{'Journey end: 30-minute telemetry timeout (values up to the last received signal)\n\n' if timeout else ''}Energy source: {source}\nInformation only; not a billing or precision measurement."),
        "fr": (f"MOT - Trajet de {summary.distance_km:.0f} km avec {name}", f"MOT : trajet avec {name} ({vehicle_id}) terminé.\n\nDistance : {n(summary.distance_km, 0)} km\nDurée de conduite : {summary.duration_minutes} min\nSOC consommé : {n(summary.soc_used)} points de pourcentage\nÉnergie prélevée : {n(summary.energy_drawn_kwh, 2)} kWh\nRécupération : {n(summary.energy_regen_kwh, 2)} kWh\nÉnergie nette consommée : {n(summary.energy_net_kwh, 2)} kWh\nConsommation nette : {n(summary.net_kwh_per_100_km)} kWh/100 km\n\n{'Fin du trajet : délai de télémétrie de 30 minutes (valeurs jusqu’au dernier signal reçu)\n\n' if timeout else ''}Source d’énergie : {source}\nInformation uniquement ; aucune mesure de facturation ou de précision."),
        "it": (f"MOT - Viaggio di {summary.distance_km:.0f} km con {name}", f"MOT: viaggio con {name} ({vehicle_id}) concluso.\n\nDistanza: {n(summary.distance_km, 0)} km\nTempo di guida: {summary.duration_minutes} min\nSOC utilizzato: {n(summary.soc_used)} punti percentuali\nEnergia prelevata: {n(summary.energy_drawn_kwh, 2)} kWh\nRecuperata: {n(summary.energy_regen_kwh, 2)} kWh\nEnergia netta utilizzata: {n(summary.energy_net_kwh, 2)} kWh\nConsumo netto: {n(summary.net_kwh_per_100_km)} kWh/100 km\n\n{'Fine viaggio: timeout telemetria di 30 minuti (valori fino all’ultimo segnale ricevuto)\n\n' if timeout else ''}Fonte energia: {source}\nSolo informazione; non è una misura fiscale o di precisione."),
    }
    return values[lang]


def daily_summary(preference, report_date, summary, ongoing):
    lang = language(preference); name = str(preference.get("vehicleName") or preference["vehicleId"])[:40]
    n = lambda value, digits=1: number(value, digits, lang)
    date = report_date if lang == "en" else f"{report_date[8:10]}.{report_date[5:7]}.{report_date[:4]}"
    consumption = "--" if summary["netKwhPer100Km"] is None else f"{n(summary['netKwhPer100Km'])} kWh/100 km"
    notes = {
        "de": "Hinweis: Eine Fahrt oder ein Ladevorgang läuft noch und ist in diesen Summen nicht enthalten. Der Vorgang wird dem Tag seines Abschlusses zugerechnet.",
        "en": "Note: A journey or charging session is still active and is not included. It will be assigned to the day on which it ends.",
        "fr": "Remarque : un trajet ou une recharge est encore en cours et n'est pas inclus. Il sera attribué au jour de sa fin.",
        "it": "Nota: un viaggio o una ricarica è ancora in corso e non è incluso. Sarà attribuito al giorno in cui termina.",
    }
    note = f"\n{notes[lang]}\n" if ongoing else ""
    rows = {
        "de": ["MOT Tagesübersicht für", "Datum", "Fahrten", "Gesamtstrecke", "Gesamte Fahrzeit", "Energie bezogen", "Rekuperiert", "Nettoenergie", "Durchschnittlicher Nettoverbrauch", "Ladevorgänge", "Gesamte Ladezeit", "Geschätzte geladene Energie", "SOC-Zunahme", "Passive Telemetrie; keine Abrechnungs- oder Präzisionsmessung."],
        "en": ["MOT daily summary for", "Date", "Journeys", "Total distance", "Total driving time", "Energy drawn", "Recuperated", "Net energy", "Average net consumption", "Charging sessions", "Total charging time", "Estimated energy charged", "SOC increase", "Passive telemetry; not a billing or precision measurement."],
        "fr": ["Résumé quotidien MOT pour", "Date", "Trajets", "Distance totale", "Temps de conduite total", "Énergie prélevée", "Récupération", "Énergie nette", "Consommation nette moyenne", "Recharges", "Temps de recharge total", "Énergie rechargée estimée", "Augmentation du SOC", "Télémétrie passive ; aucune mesure de facturation ou de précision."],
        "it": ["Riepilogo giornaliero MOT per", "Data", "Viaggi", "Distanza totale", "Tempo di guida totale", "Energia prelevata", "Recuperata", "Energia netta", "Consumo netto medio", "Ricariche", "Tempo di ricarica totale", "Energia caricata stimata", "Aumento SOC", "Telemetria passiva; non è una misura fiscale o di precisione."],
    }[lang]
    point_units = {"de": "Prozentpunkte", "en": "percentage points", "fr": "points de pourcentage", "it": "punti percentuali"}
    text = (f"{rows[0]} {name} ({preference['vehicleId']})\n{rows[1]}: {date}\n\n{rows[2]}: {summary['journeyCount']}\n{rows[3]}: {n(summary['distanceKm'], 0)} km\n{rows[4]}: {summary['journeyDurationMinutes']} min\n{rows[5]}: {n(summary['energyDrawnKwh'], 2)} kWh\n{rows[6]}: {n(summary['energyRegenKwh'], 2)} kWh\n{rows[7]}: {n(summary['energyNetKwh'], 2)} kWh\n{rows[8]}: {consumption}\n\n{rows[9]}: {summary['chargingCount']}\n{rows[10]}: {summary['chargingDurationMinutes']} min\n{rows[11]}: {n(summary['energyChargedKwh'], 2)} kWh\n{rows[12]}: {n(summary['chargingSocDelta'])} {point_units[lang]}{note}\n\n{rows[13]}")
    subjects = {"de": "Tagesübersicht", "en": "Daily summary", "fr": "Résumé quotidien", "it": "Riepilogo giornaliero"}
    return f"MOT - {subjects[lang]} {report_date} ({preference['vehicleId']})", text
