"""JWT-authorized personal versus anonymous community efficiency comparison."""

from datetime import datetime, timezone
from decimal import Decimal
import hashlib
import json
import os
from zoneinfo import ZoneInfo

import boto3


ZURICH = ZoneInfo("Europe/Zurich")
MIN_COMMUNITY_VEHICLES = 3
MIN_COMMUNITY_JOURNEYS = 10
MIN_COMMUNITY_DISTANCE_KM = Decimal("100")
NEUTRAL_PERCENT = Decimal("5")

dynamodb = boto3.resource("dynamodb")
access = dynamodb.Table(os.environ["ACCESS_TABLE_NAME"])
aggregates = dynamodb.Table(os.environ["FLEET_AGGREGATE_TABLE_NAME"])
personal = dynamodb.Table(os.environ["USER_EFFICIENCY_TABLE_NAME"])


def _response(status, body):
    return {
        "statusCode": status,
        "headers": {"content-type": "application/json", "cache-control": "no-store"},
        "body": json.dumps(body, separators=(",", ":")),
    }


def _number(value):
    return Decimal(str(value or 0))


def _previous_month(now=None):
    instant = datetime.fromtimestamp(
        now if now is not None else datetime.now(tz=timezone.utc).timestamp(),
        tz=timezone.utc,
    ).astimezone(ZURICH)
    previous = instant.replace(
        day=1, hour=0, minute=0, second=0, microsecond=0
    ).timestamp() - 1
    return datetime.fromtimestamp(previous, tz=timezone.utc).astimezone(ZURICH).strftime("%Y-%m")


def _key(user_sub, vehicle_id):
    return hashlib.sha256(f"{user_sub}|{vehicle_id}".encode("utf-8")).hexdigest()


def comparison(community, own, month):
    if community.get("isFinalized") is not True or own.get("isFinalized") is not True:
        return {"available": False, "reason": "month_not_finalized", "month": month}
    own_distance = _number(own.get("distanceKm"))
    own_net = _number(own.get("energyNetKwh"))
    own_journeys = int(own.get("journeyCount", 0))
    other_distance = _number(community.get("distanceKm")) - own_distance
    other_net = _number(community.get("energyNetKwh")) - own_net
    other_journeys = int(community.get("journeyCount", 0)) - own_journeys
    other_vehicles = int(community.get("activeVehicleCount", 0)) - 1
    if own_distance <= 0 or own_journeys <= 0:
        return {"available": False, "reason": "not_enough_personal_data", "month": month}
    if (
        other_vehicles < MIN_COMMUNITY_VEHICLES
        or other_journeys < MIN_COMMUNITY_JOURNEYS
        or other_distance < MIN_COMMUNITY_DISTANCE_KM
        or other_net <= 0
    ):
        return {"available": False, "reason": "not_enough_community_data", "month": month}
    own_average = _number(own.get("averageNetKwhPer100Km"))
    community_average = other_net / other_distance * Decimal(100)
    difference = own_average - community_average
    percent = difference / community_average * Decimal(100)
    flag = "+" if percent <= -NEUTRAL_PERCENT else ("-" if percent >= NEUTRAL_PERCENT else "=")
    return {
        "available": True,
        "month": month,
        "isFinalized": True,
        "flag": flag,
        "neutralPercent": float(NEUTRAL_PERCENT),
        "personal": {
            "distanceKm": float(own_distance),
            "energyNetKwh": float(own_net),
            "averageNetKwhPer100Km": float(own_average),
            "journeyCount": own_journeys,
        },
        "community": {
            "distanceKm": float(other_distance),
            "energyNetKwh": float(other_net),
            "averageNetKwhPer100Km": float(community_average),
            "journeyCount": other_journeys,
            "vehicleCount": other_vehicles,
        },
        "difference": {
            "kwhPer100Km": float(difference),
            "percent": float(percent),
        },
    }


def handler(event, context):
    claims = event.get("requestContext", {}).get("authorizer", {}).get("jwt", {}).get("claims", {})
    user_sub = str(claims.get("sub", "")).strip()
    vehicle_id = str((event.get("pathParameters") or {}).get("vehicleId", "")).strip()
    if not user_sub:
        return _response(401, {"error": "unauthorized"})
    if not vehicle_id:
        return _response(404, {"error": "vehicle_not_found"})
    assignment = access.get_item(
        Key={"userSub": user_sub, "vehicleId": vehicle_id}, ConsistentRead=True
    ).get("Item", {})
    if assignment.get("status") != "ACTIVE":
        return _response(404, {"error": "vehicle_not_found"})
    month = _previous_month()
    community = aggregates.get_item(
        Key={"month": month}, ConsistentRead=True
    ).get("Item", {})
    own = personal.get_item(
        Key={"month": month, "subjectVehicleKey": _key(user_sub, vehicle_id)},
        ConsistentRead=True,
    ).get("Item", {})
    if not community or not own:
        return _response(200, {
            "available": False, "reason": "not_enough_data", "month": month,
        })
    return _response(200, comparison(community, own, month))
