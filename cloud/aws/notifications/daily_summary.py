"""Pure Europe/Zurich day-window and notification aggregation helpers."""

from datetime import datetime, time, timedelta, timezone
from zoneinfo import ZoneInfo


REPORT_TIMEZONE = ZoneInfo("Europe/Zurich")
FORCE_SEND_HOUR = 8
SUMMARY_EVENT_TYPES = {"JOURNEY_SUMMARY", "CHARGING_SUMMARY"}


def report_window(now_ms):
    """Return the preceding Zurich calendar day and its UTC millisecond bounds."""
    local_now = datetime.fromtimestamp(now_ms / 1000, timezone.utc).astimezone(REPORT_TIMEZONE)
    report_date = local_now.date() - timedelta(days=1)
    start_local = datetime.combine(report_date, time.min, REPORT_TIMEZONE)
    end_local = datetime.combine(report_date + timedelta(days=1), time.min, REPORT_TIMEZONE)
    return (
        report_date.isoformat(),
        int(start_local.timestamp() * 1000),
        int(end_local.timestamp() * 1000),
        local_now.hour >= FORCE_SEND_HOUR,
    )


def _number(value, default=0.0):
    if value is None or isinstance(value, bool):
        return default
    try:
        return float(value)
    except (TypeError, ValueError, ArithmeticError):
        return default


def aggregate(items, start_ms, end_ms):
    journeys = []
    charges = []
    for item in items:
        event_type = item.get("eventType")
        received_at = int(_number(item.get("receivedAt"), -1))
        if event_type not in SUMMARY_EVENT_TYPES or not start_ms <= received_at < end_ms:
            continue
        (journeys if event_type == "JOURNEY_SUMMARY" else charges).append(item)

    distance = sum(_number(item.get("distanceKm")) for item in journeys)
    duration = sum(int(_number(item.get("durationMinutes"))) for item in journeys)
    drawn = sum(_number(item.get("energyDrawnKwh")) for item in journeys)
    regen = sum(_number(item.get("energyRegenKwh")) for item in journeys)
    net = sum(_number(item.get("energyNetKwh")) for item in journeys)
    charge_duration = sum(int(_number(item.get("durationMinutes"))) for item in charges)
    charged = sum(_number(item.get("energyChargedKwh")) for item in charges)
    soc_delta = sum(_number(item.get("socDelta")) for item in charges)
    return {
        "journeyCount": len(journeys),
        "distanceKm": distance,
        "journeyDurationMinutes": duration,
        "energyDrawnKwh": drawn,
        "energyRegenKwh": regen,
        "energyNetKwh": net,
        "netKwhPer100Km": net / distance * 100 if distance > 0 else None,
        "chargingCount": len(charges),
        "chargingDurationMinutes": charge_duration,
        "energyChargedKwh": charged,
        "chargingSocDelta": soc_delta,
    }


def has_activity(summary):
    return summary["journeyCount"] > 0 or summary["chargingCount"] > 0
