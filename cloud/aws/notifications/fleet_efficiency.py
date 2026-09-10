"""Privacy-minimized, idempotent monthly fleet-efficiency aggregation."""

from datetime import datetime, timezone
from decimal import Decimal
import hashlib
import os
import time
from zoneinfo import ZoneInfo

SCHEMA_VERSION = 1
MARKER_RETENTION_DAYS = 93
PERSONAL_RETENTION_DAYS = 400
ACCEPTED_CAPACITY_STATES = {"DECLARED", "PLAUSIBLE"}
ZURICH = ZoneInfo("Europe/Zurich")


def _number(value):
    return Decimal(str(value or 0))


def _month(received_at_ms):
    instant = datetime.fromtimestamp(int(received_at_ms) / 1000, tz=timezone.utc)
    return instant.astimezone(ZURICH).strftime("%Y-%m")


def _marker_id(vehicle_id, journey_id):
    material = f"{vehicle_id}|{journey_id}".encode("utf-8")
    return hashlib.sha256(material).hexdigest()


def _subject_vehicle_key(user_sub, vehicle_id):
    return hashlib.sha256(f"{user_sub}|{vehicle_id}".encode("utf-8")).hexdigest()


def aggregate_values(event, profile):
    """Return only anonymous monthly increments plus the transient marker key."""
    vehicle_id = str(event["vehicleId"])
    journey_id = str(event["journeyId"])
    distance = _number(event.get("distanceKm"))
    drawn = _number(event.get("energyDrawnKwh"))
    regen = _number(event.get("energyRegenKwh"))
    net = _number(event.get("energyNetKwh"))
    soc = _number(event.get("socUsed"))
    source = str(event.get("energySource", ""))
    capacity_state = str(profile.get("batteryCapacityVerificationStatus", "UNKNOWN"))
    capacity = _number(profile.get("batteryCapacityKwh"))
    capacity_accepted = capacity > 0 and capacity_state in ACCEPTED_CAPACITY_STATES
    return {
        "markerId": _marker_id(vehicle_id, journey_id),
        "month": _month(event["receivedAt"]),
        "journeyCount": Decimal(1),
        "distanceKm": distance,
        "energyDrawnKwh": drawn,
        "energyRegenKwh": regen,
        "energyNetKwh": net,
        "socUsedPoints": soc,
        "socEquivalentKwh": soc * capacity / Decimal(100) if capacity_accepted else Decimal(0),
        "socComparisonIncludedCount": Decimal(1) if capacity_accepted else Decimal(0),
        "socComparisonExcludedCount": Decimal(0) if capacity_accepted else Decimal(1),
        "firmwareJourneyCount": Decimal(1) if source == "firmware_counter" else Decimal(0),
        "telemetryJourneyCount": Decimal(0) if source == "firmware_counter" else Decimal(1),
    }


def _typed(values):
    from boto3.dynamodb.types import TypeSerializer

    serializer = TypeSerializer()
    return {key: serializer.serialize(value) for key, value in values.items()}


def record_event(client, marker_table, aggregate_table, profile_table, event, now=None):
    profile = profile_table.get_item(
        Key={"vehicleId": str(event["vehicleId"])}, ConsistentRead=True
    ).get("Item", {})
    values = aggregate_values(event, profile)
    now = int(now if now is not None else time.time())
    increments = {
        key: value for key, value in values.items()
        if key not in {"markerId", "month"}
    }
    names = {f"#{key}": key for key in increments}
    expression_values = _typed({f":{key}": value for key, value in increments.items()})
    expression_values.update(_typed({
        ":schema": SCHEMA_VERSION,
        ":updated": now * 1000,
    }))
    try:
        client.transact_write_items(TransactItems=[
            {
                "Put": {
                    "TableName": marker_table,
                    "Item": _typed({
                        "markerId": values["markerId"],
                        "expiresAt": now + MARKER_RETENTION_DAYS * 86400,
                    }),
                    "ConditionExpression": "attribute_not_exists(markerId)",
                }
            },
            {
                "Update": {
                    "TableName": aggregate_table,
                    "Key": _typed({"month": values["month"]}),
                    "UpdateExpression": (
                        "SET schemaVersion=:schema, updatedAt=:updated ADD "
                        + ", ".join(f"#{key} :{key}" for key in increments)
                    ),
                    "ExpressionAttributeNames": names,
                    "ExpressionAttributeValues": expression_values,
                }
            },
        ])
        return True
    except Exception as error:
        response = getattr(error, "response", {})
        if response.get("Error", {}).get("Code") == "TransactionCanceledException":
            reasons = response.get("CancellationReasons", [])
            if reasons and reasons[0].get("Code") == "ConditionalCheckFailed":
                return False
        raise


def _conditional_transaction(client, transaction):
    try:
        client.transact_write_items(TransactItems=transaction)
        return True
    except Exception as error:
        response = getattr(error, "response", {})
        if response.get("Error", {}).get("Code") == "TransactionCanceledException":
            reasons = response.get("CancellationReasons", [])
            if reasons and reasons[0].get("Code") == "ConditionalCheckFailed":
                return False
        raise


def record_active_vehicle(client, marker_table, aggregate_table, event, now=None):
    now = int(now if now is not None else time.time())
    month = _month(event["receivedAt"])
    marker = hashlib.sha256(
        f"active|{month}|{event['vehicleId']}".encode("utf-8")
    ).hexdigest()
    return _conditional_transaction(client, [
        {"Put": {
            "TableName": marker_table,
            "Item": _typed({
                "markerId": marker,
                "expiresAt": now + MARKER_RETENTION_DAYS * 86400,
            }),
            "ConditionExpression": "attribute_not_exists(markerId)",
        }},
        {"Update": {
            "TableName": aggregate_table,
            "Key": _typed({"month": month}),
            "UpdateExpression": "SET updatedAt=:updated ADD activeVehicleCount :one",
            "ExpressionAttributeValues": _typed({
                ":updated": now * 1000, ":one": Decimal(1),
            }),
        }},
    ])


def record_personal_event(client, marker_table, personal_table, event, now=None):
    if not event.get("eventId") or not event.get("userSub"):
        return False
    now = int(now if now is not None else time.time())
    month = _month(event["receivedAt"])
    marker = hashlib.sha256(f"personal|{event['eventId']}".encode("utf-8")).hexdigest()
    values = {
        ":one": Decimal(1),
        ":distance": _number(event.get("distanceKm")),
        ":net": _number(event.get("energyNetKwh")),
        ":updated": now * 1000,
        ":expiry": now + PERSONAL_RETENTION_DAYS * 86400,
    }
    return _conditional_transaction(client, [
        {"Put": {
            "TableName": marker_table,
            "Item": _typed({
                "markerId": marker,
                "expiresAt": now + MARKER_RETENTION_DAYS * 86400,
            }),
            "ConditionExpression": "attribute_not_exists(markerId)",
        }},
        {"Update": {
            "TableName": personal_table,
            "Key": _typed({
                "month": month,
                "subjectVehicleKey": _subject_vehicle_key(
                    event["userSub"], event["vehicleId"]
                ),
            }),
            "UpdateExpression": (
                "SET updatedAt=:updated, expiresAt=:expiry "
                "ADD journeyCount :one, distanceKm :distance, energyNetKwh :net"
            ),
            "ExpressionAttributeValues": _typed(values),
        }},
    ])


def finalize_month(aggregate_table, personal_table, month, now=None):
    now = int(now if now is not None else time.time())
    aggregate = aggregate_table.get_item(
        Key={"month": month}, ConsistentRead=True
    ).get("Item", {})
    if not aggregate:
        return {"month": month, "community": False, "personal": 0}
    distance = _number(aggregate.get("distanceKm"))
    net = _number(aggregate.get("energyNetKwh"))
    average = net / distance * Decimal(100) if distance > 0 else Decimal(0)
    aggregate_table.update_item(
        Key={"month": month},
        UpdateExpression=(
            "SET averageNetKwhPer100Km=:average, finalizedAt=:finalized, "
            "isFinalized=:yes"
        ),
        ExpressionAttributeValues={
            ":average": average, ":finalized": now * 1000, ":yes": True,
        },
    )
    updated = 0
    query = {}
    from boto3.dynamodb.conditions import Key

    while True:
        page = personal_table.query(
            KeyConditionExpression=Key("month").eq(month), **query
        )
        for item in page.get("Items", []):
            personal_distance = _number(item.get("distanceKm"))
            personal_net = _number(item.get("energyNetKwh"))
            personal_average = (
                personal_net / personal_distance * Decimal(100)
                if personal_distance > 0 else Decimal(0)
            )
            personal_table.update_item(
                Key={
                    "month": month,
                    "subjectVehicleKey": item["subjectVehicleKey"],
                },
                UpdateExpression=(
                    "SET averageNetKwhPer100Km=:average, finalizedAt=:finalized, "
                    "isFinalized=:yes"
                ),
                ExpressionAttributeValues={
                    ":average": personal_average,
                    ":finalized": now * 1000,
                    ":yes": True,
                },
            )
            updated += 1
        key = page.get("LastEvaluatedKey")
        if not key:
            break
        query["ExclusiveStartKey"] = key
    return {"month": month, "community": True, "personal": updated}


def previous_zurich_month(now=None):
    instant = datetime.fromtimestamp(
        now if now is not None else time.time(), tz=timezone.utc
    ).astimezone(ZURICH)
    first = instant.replace(day=1, hour=0, minute=0, second=0, microsecond=0)
    previous = (first.timestamp() - 1)
    return datetime.fromtimestamp(previous, tz=timezone.utc).astimezone(ZURICH).strftime("%Y-%m")


def _stream_item(record):
    if record.get("eventName") != "INSERT":
        return None
    image = record.get("dynamodb", {}).get("NewImage", {})
    from boto3.dynamodb.types import TypeDeserializer

    deserializer = TypeDeserializer()
    item = {key: deserializer.deserialize(value) for key, value in image.items()}
    if item.get("eventType") != "JOURNEY_SUMMARY":
        return None
    required = {"vehicleId", "journeyId", "receivedAt"}
    return item if required.issubset(item) else None


def handler(event, context):
    import boto3

    client = boto3.client("dynamodb")
    aggregates = boto3.resource("dynamodb").Table(os.environ["FLEET_AGGREGATE_TABLE_NAME"])
    personal = boto3.resource("dynamodb").Table(os.environ["USER_EFFICIENCY_TABLE_NAME"])
    if event.get("type") == "finalize_month":
        return finalize_month(
            aggregates, personal, event.get("month") or previous_zurich_month()
        )
    profiles = boto3.resource("dynamodb").Table(os.environ["VEHICLE_PROFILE_TABLE_NAME"])
    recorded = 0
    duplicates = 0
    ignored = 0
    active_added = 0
    personal_recorded = 0
    for record in event.get("Records", []):
        item = _stream_item(record)
        if item is None:
            ignored += 1
            continue
        if record_event(
            client,
            os.environ["FLEET_MARKER_TABLE_NAME"],
            os.environ["FLEET_AGGREGATE_TABLE_NAME"],
            profiles,
            item,
        ):
            recorded += 1
        else:
            duplicates += 1
        if record_active_vehicle(
            client,
            os.environ["FLEET_MARKER_TABLE_NAME"],
            os.environ["FLEET_AGGREGATE_TABLE_NAME"],
            item,
        ):
            active_added += 1
        if record_personal_event(
            client,
            os.environ["FLEET_MARKER_TABLE_NAME"],
            os.environ["USER_EFFICIENCY_TABLE_NAME"],
            item,
        ):
            personal_recorded += 1
    return {
        "recorded": recorded,
        "duplicates": duplicates,
        "ignored": ignored,
        "activeAdded": active_added,
        "personalRecorded": personal_recorded,
    }
