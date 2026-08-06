import json
import os
import sys
import types
import unittest
from pathlib import Path


TEMPLATE = Path(__file__).parents[1] / "template.yaml"


def inline_code(resource_name):
    lines = TEMPLATE.read_text(encoding="utf-8").splitlines()
    resource = f"  {resource_name}:"
    start = lines.index(resource)
    zip_line = next(
        index for index in range(start, len(lines))
        if lines[index] == "        ZipFile: |"
    )
    code = []
    for line in lines[zip_line + 1:]:
        if line.strip() and not line.startswith("          "):
            break
        code.append(line[10:] if line.strip() else "")
    return "\n".join(code) + "\n"


class KeyExpression:
    def __init__(self, name):
        self.name = name

    def eq(self, value):
        return KeyCondition(self.name, "eq", value)

    def between(self, lower, upper):
        return KeyCondition(self.name, "between", (lower, upper))


class KeyCondition:
    def __init__(self, name, operator, value):
        self.name = name
        self.operator = operator
        self.value = value

    def __and__(self, other):
        return self, other

    def __getitem__(self, index):
        return (self.name, self.value)[index]


class FakeClientError(Exception):
    def __init__(self, code="Error", status=500):
        super().__init__(code)
        self.response = {
            "Error": {"Code": code},
            "ResponseMetadata": {"HTTPStatusCode": status},
        }


class FakeManagement:
    def __init__(self):
        self.posts = []
        self.deletes = []

    def post_to_connection(self, **kwargs):
        self.posts.append(kwargs)

    def delete_connection(self, **kwargs):
        self.deletes.append(kwargs["ConnectionId"])


class FakeBoto3(types.ModuleType):
    def __init__(self, tables, management=None):
        super().__init__("boto3")
        self.tables = tables
        self.management = management or FakeManagement()

    def resource(self, name):
        assert name == "dynamodb"
        tables = self.tables
        return types.SimpleNamespace(Table=lambda table_name: tables[table_name])

    def client(self, name, **_kwargs):
        assert name == "apigatewaymanagementapi"
        return self.management


def load_lambda(resource_name, tables, environment, management=None):
    boto3 = FakeBoto3(tables, management)
    old_modules = {
        name: sys.modules.get(name)
        for name in (
            "boto3", "boto3.dynamodb", "boto3.dynamodb.conditions",
            "botocore", "botocore.exceptions"
        )
    }
    sys.modules["boto3"] = boto3
    sys.modules["boto3.dynamodb"] = types.ModuleType("boto3.dynamodb")
    conditions = types.ModuleType("boto3.dynamodb.conditions")
    conditions.Key = KeyExpression
    sys.modules["boto3.dynamodb.conditions"] = conditions
    sys.modules["botocore"] = types.ModuleType("botocore")
    exceptions = types.ModuleType("botocore.exceptions")
    exceptions.ClientError = FakeClientError
    sys.modules["botocore.exceptions"] = exceptions
    previous_environment = os.environ.copy()
    os.environ.update(environment)
    module = types.ModuleType(f"test_{resource_name}")
    try:
        exec(compile(inline_code(resource_name), resource_name, "exec"), module.__dict__)
    finally:
        os.environ.clear()
        os.environ.update(previous_environment)
        for name, previous_module in old_modules.items():
            if previous_module is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = previous_module
    return module


class AccessTable:
    def __init__(self, assignments):
        self.assignments = assignments

    def query(self, **kwargs):
        user_sub = kwargs["KeyConditionExpression"][1]
        return {"Items": [
            {"vehicleId": vehicle_id, "status": status}
            for (subject, vehicle_id), status in self.assignments.items()
            if subject == user_sub
        ]}

    def get_item(self, **kwargs):
        key = kwargs["Key"]
        status = self.assignments.get((key["userSub"], key["vehicleId"]))
        return {"Item": {"status": status}} if status else {}


class VehicleStateTable:
    def query(self, **kwargs):
        vehicle_id = kwargs["KeyConditionExpression"][1]
        if vehicle_id != "alpha":
            return {"Items": []}
        return {"Items": [{
            "vehicleId": "alpha", "topicSuffix": "status/online",
            "value": True, "receivedAt": 10, "valueType": "boolean",
            "payloadBytes": 4,
        }]}


class VehicleHistoryTable:
    def __init__(self, items=None):
        self.items = list(items or [])

    def put_item(self, Item, **_kwargs):
        if any(
            item["vehicleId"] == Item["vehicleId"]
            and item["sampleKey"] == Item["sampleKey"]
            for item in self.items
        ):
            raise FakeClientError("ConditionalCheckFailedException", 400)
        self.items.append(dict(Item))

    def query(self, **kwargs):
        vehicle_condition, range_condition = kwargs["KeyConditionExpression"]
        vehicle_id = vehicle_condition.value
        lower, upper = range_condition.value
        return {"Items": [
            dict(item) for item in self.items
            if item["vehicleId"] == vehicle_id
            and lower <= item["sampleKey"] <= upper
        ]}


class ConnectionTable:
    def __init__(self, items=None):
        self.items = {item["connectionId"]: dict(item) for item in (items or [])}
        self.updates = []

    def put_item(self, Item):
        self.items[Item["connectionId"]] = dict(Item)

    def get_item(self, Key, **_kwargs):
        item = self.items.get(Key["connectionId"])
        return {"Item": dict(item)} if item else {}

    def update_item(self, Key, **kwargs):
        self.updates.append(kwargs)
        item = self.items[Key["connectionId"]]
        if ":v" in kwargs["ExpressionAttributeValues"]:
            item["vehicleId"] = kwargs["ExpressionAttributeValues"][":v"]

    def delete_item(self, Key):
        self.items.pop(Key["connectionId"], None)

    def query(self, **_kwargs):
        return {"Items": [
            {"connectionId": item["connectionId"]}
            for item in self.items.values()
        ]}


def rest_event(path, subject=None, vehicle_id=None):
    claims = {"sub": subject} if subject else {}
    return {
        "rawPath": path,
        "pathParameters": {"vehicleId": vehicle_id} if vehicle_id else {},
        "requestContext": {
            "http": {"method": "GET"},
            "authorizer": {"jwt": {"claims": claims}},
        },
    }


class VehicleApiAuthorizationTests(unittest.TestCase):
    def setUp(self):
        access = AccessTable({
            ("user-a", "alpha"): "ACTIVE",
            ("user-a", "beta"): "REVOKED",
            ("user-b", "beta"): "ACTIVE",
        })
        self.history = VehicleHistoryTable([{
            "vehicleId": "alpha", "sampleKey": "soc#1700000000",
            "signal": "soc", "sampledAt": 1_700_000_000_000,
            "receivedAt": 1_700_000_001_000, "value": 55,
        }])
        self.module = load_lambda(
            "VehicleApiFunction",
            {
                "state": VehicleStateTable(), "access": access,
                "history": self.history,
            },
            {
                "TABLE_NAME": "state", "ACCESS_TABLE_NAME": "access",
                "HISTORY_TABLE_NAME": "history",
            },
        )
        self.module.time = types.SimpleNamespace(time=lambda: 1_700_000_100)

    def test_missing_subject_is_denied(self):
        result = self.module.handler(rest_event("/api/vehicles"), None)
        self.assertEqual(401, result["statusCode"])

    def test_list_contains_only_active_assignments(self):
        result = self.module.handler(rest_event("/api/vehicles", "user-a"), None)
        body = json.loads(result["body"])
        self.assertEqual(["alpha"], [item["vehicleId"] for item in body["vehicles"]])

    def test_snapshot_of_unassigned_vehicle_is_not_disclosed(self):
        result = self.module.handler(
            rest_event("/api/vehicles/beta/snapshot", "user-a", "beta"), None
        )
        self.assertEqual(404, result["statusCode"])
        self.assertNotIn("beta", result["body"])

    def test_history_of_unassigned_vehicle_is_not_disclosed(self):
        result = self.module.handler(
            rest_event("/api/vehicles/beta/history", "user-a", "beta"), None
        )
        self.assertEqual(404, result["statusCode"])
        self.assertNotIn("beta", result["body"])

    def test_history_uses_existing_access_and_fixed_range(self):
        event = rest_event("/api/vehicles/alpha/history", "user-a", "alpha")
        event["queryStringParameters"] = {"hours": "24"}
        result = self.module.handler(event, None)
        body = json.loads(result["body"])
        self.assertEqual(200, result["statusCode"])
        self.assertEqual(300, body["resolutionSeconds"])
        self.assertEqual(55, body["points"][0]["soc"])

        event["queryStringParameters"] = {"hours": "25"}
        result = self.module.handler(event, None)
        self.assertEqual(400, result["statusCode"])

    def test_speed_history_is_averaged_per_api_resolution(self):
        self.history.items.extend([
            {
                "vehicleId": "alpha", "sampleKey": "speed#1700000100",
                "signal": "speed", "sampledAt": 1_700_000_100_000,
                "receivedAt": 1_700_000_100_000, "value": 20,
            },
            {
                "vehicleId": "alpha", "sampleKey": "speed#1700000160",
                "signal": "speed", "sampledAt": 1_700_000_160_000,
                "receivedAt": 1_700_000_160_000, "value": 40,
            },
            {
                "vehicleId": "alpha", "sampleKey": "speed#1700000220",
                "signal": "speed", "sampledAt": 1_700_000_220_000,
                "receivedAt": 1_700_000_220_000, "value": 0,
            },
        ])
        points = self.module.load_history(
            "alpha", 1_700_000_000_000, 1_700_000_299_000, 300
        )
        speed_point = next(point for point in points if "speed" in point)
        self.assertEqual(20.0, speed_point["speed"])
        self.assertEqual(0, speed_point["speedMin"])
        self.assertEqual(40, speed_point["speedMax"])

    def test_power_history_is_averaged_per_api_resolution(self):
        self.history.items.extend([
            {
                "vehicleId": "alpha", "sampleKey": "power#1700000100",
                "signal": "power", "sampledAt": 1_700_000_100_000,
                "receivedAt": 1_700_000_100_000, "value": 120,
            },
            {
                "vehicleId": "alpha", "sampleKey": "power#1700000160",
                "signal": "power", "sampledAt": 1_700_000_160_000,
                "receivedAt": 1_700_000_160_000, "value": -40,
            },
        ])
        points = self.module.load_history(
            "alpha", 1_700_000_000_000, 1_700_000_299_000, 300
        )
        power_point = next(point for point in points if "power" in point)
        self.assertEqual(40.0, power_point["power"])
        self.assertEqual(-40, power_point["powerMin"])
        self.assertEqual(120, power_point["powerMax"])


class LiveAuthorizationTests(unittest.TestCase):
    def setUp(self):
        self.connections = ConnectionTable()
        self.access = AccessTable({("user-a", "alpha"): "ACTIVE"})
        self.management = FakeManagement()
        self.module = load_lambda(
            "LiveHandlerFunction",
            {"connections": self.connections, "access": self.access},
            {
                "CONNECTION_TABLE_NAME": "connections",
                "ACCESS_TABLE_NAME": "access",
                "WEBSOCKET_API_ID": "api", "WEBSOCKET_STAGE": "$default",
                "AWS_REGION": "eu-north-1",
            },
            self.management,
        )
        self.module.time = types.SimpleNamespace(time=lambda: 1000)

    def test_connect_ttl_equals_access_token_expiry(self):
        event = {"requestContext": {
            "routeKey": "$connect", "connectionId": "c1",
            "authorizer": {"sub": "user-a", "exp": "1100"},
        }}
        self.assertEqual(200, self.module.handler(event, None)["statusCode"])
        self.assertEqual(1100, self.connections.items["c1"]["expiresAt"])

    def test_subscribe_requires_active_assignment(self):
        self.connections.items["c1"] = {
            "connectionId": "c1", "userSub": "user-a", "expiresAt": 1100
        }
        denied = self.module.handler({
            "requestContext": {"routeKey": "subscribe", "connectionId": "c1"},
            "body": json.dumps({"vehicleId": "beta"}),
        }, None)
        self.assertEqual(404, denied["statusCode"])
        self.assertNotIn("vehicleId", self.connections.items["c1"])

    def test_ping_does_not_extend_expiry(self):
        self.connections.items["c1"] = {
            "connectionId": "c1", "userSub": "user-a", "expiresAt": 1100
        }
        result = self.module.handler({
            "requestContext": {"routeKey": "ping", "connectionId": "c1"},
            "body": "{}",
        }, None)
        self.assertEqual(200, result["statusCode"])
        self.assertEqual([], self.connections.updates)
        self.assertEqual(1100, self.connections.items["c1"]["expiresAt"])

    def test_ping_disconnects_revoked_subscription(self):
        self.connections.items["c1"] = {
            "connectionId": "c1", "userSub": "user-a",
            "vehicleId": "beta", "expiresAt": 1100
        }
        result = self.module.handler({
            "requestContext": {"routeKey": "ping", "connectionId": "c1"},
            "body": "{}",
        }, None)
        self.assertEqual(401, result["statusCode"])
        self.assertNotIn("c1", self.connections.items)
        self.assertEqual(["c1"], self.management.deletes)


class IngestAuthorizationTests(unittest.TestCase):
    def test_fanout_drops_expired_and_revoked_connections(self):
        connections = ConnectionTable([
            {"connectionId": "active", "userSub": "user-a", "expiresAt": 1100},
            {"connectionId": "expired", "userSub": "user-a", "expiresAt": 999},
            {"connectionId": "revoked", "userSub": "user-b", "expiresAt": 1100},
        ])
        access = AccessTable({
            ("user-a", "alpha"): "ACTIVE",
            ("user-b", "alpha"): "REVOKED",
        })
        management = FakeManagement()
        module = load_lambda(
            "StateIngestFunction",
            {
                "state": VehicleStateTable(), "connections": connections,
                "access": access, "history": VehicleHistoryTable(),
            },
            {
                "TABLE_NAME": "state", "CONNECTION_TABLE_NAME": "connections",
                "ACCESS_TABLE_NAME": "access", "WEBSOCKET_API_ID": "api",
                "WEBSOCKET_STAGE": "$default", "AWS_REGION": "eu-north-1",
                "HISTORY_TABLE_NAME": "history", "HISTORY_ENABLED": "false",
            },
            management,
        )
        module.time = types.SimpleNamespace(time=lambda: 1000)
        result = module.broadcast_live_update(
            "alpha", "mot/alpha/status/online", "status/online", True, "boolean", 1
        )
        self.assertEqual({"sent": 1, "removed": 2}, result)
        self.assertEqual(["active"], [post["ConnectionId"] for post in management.posts])
        self.assertEqual(["expired", "revoked"], management.deletes)

    def test_history_bucket_is_bounded_and_deduplicated(self):
        history = VehicleHistoryTable()
        module = load_lambda(
            "StateIngestFunction",
            {
                "state": VehicleStateTable(), "connections": ConnectionTable(),
                "access": AccessTable({}), "history": history,
            },
            {
                "TABLE_NAME": "state", "CONNECTION_TABLE_NAME": "connections",
                "ACCESS_TABLE_NAME": "access", "WEBSOCKET_API_ID": "api",
                "WEBSOCKET_STAGE": "$default", "AWS_REGION": "eu-north-1",
                "HISTORY_TABLE_NAME": "history", "HISTORY_ENABLED": "true",
                "HISTORY_RETENTION_DAYS": "31",
                "HISTORY_MOTION_ENABLED": "false",
                "HISTORY_VEHICLE_ALLOWLIST": "alpha",
                "HISTORY_CORE_INTERVAL_SECONDS": "300",
                "HISTORY_MOTION_INTERVAL_SECONDS": "900",
            },
        )
        self.assertTrue(module.store_history(
            "alpha", "display/soc", 55, "number", 1_700_000_001_000
        ))
        self.assertFalse(module.store_history(
            "alpha", "display/soc", 56, "number", 1_700_000_002_000
        ))
        self.assertEqual(1, len(history.items))
        self.assertEqual("soc", history.items[0]["signal"])
        self.assertLessEqual(
            history.items[0]["expiresAt"] - history.items[0]["sampledAt"] // 1000,
            31 * 86400,
        )
        self.assertFalse(module.store_history(
            "alpha", "display/speed_kmh", 42, "number", 1_700_000_001_000
        ))
        self.assertFalse(module.is_history_candidate("beta", "display/soc"))
        self.assertTrue(module.is_history_candidate("alpha", "display/soc"))
        self.assertEqual(300, module.HISTORY_SIGNALS["display/soc"][1])
        self.assertEqual(300, module.HISTORY_SIGNALS["charging/plugged"][1])
        self.assertEqual(900, module.HISTORY_SIGNALS["display/speed_kmh"][1])
        self.assertEqual(900, module.HISTORY_SIGNALS["charging/power_signed"][1])
        self.assertNotIn("display/odometer_km", module.HISTORY_SIGNALS)
        self.assertTrue(module.store_history(
            "alpha", "charging/plugged", True, "boolean", 1_700_000_301_000
        ))

    def test_speed_history_samples_driving_minutes_and_one_stop(self):
        history = VehicleHistoryTable()
        module = load_lambda(
            "StateIngestFunction",
            {
                "state": VehicleStateTable(), "connections": ConnectionTable(),
                "access": AccessTable({}), "history": history,
            },
            {
                "TABLE_NAME": "state", "CONNECTION_TABLE_NAME": "connections",
                "ACCESS_TABLE_NAME": "access", "WEBSOCKET_API_ID": "api",
                "WEBSOCKET_STAGE": "$default", "AWS_REGION": "eu-north-1",
                "HISTORY_TABLE_NAME": "history", "HISTORY_ENABLED": "true",
                "HISTORY_RETENTION_DAYS": "31",
                "HISTORY_MOTION_ENABLED": "true",
                "HISTORY_VEHICLE_ALLOWLIST": "alpha",
                "HISTORY_CORE_INTERVAL_SECONDS": "300",
                "HISTORY_MOTION_INTERVAL_SECONDS": "60",
            },
        )
        self.assertTrue(module.store_history(
            "alpha", "display/speed_kmh", 30, "number", 1_700_000_001_000,
            1_699_999_930_000, 20
        ))
        self.assertFalse(module.store_history(
            "alpha", "display/speed_kmh", 35, "number", 1_700_000_020_000,
            1_700_000_001_000, 30
        ))
        self.assertTrue(module.store_history(
            "alpha", "display/speed_kmh", 0, "number", 1_700_000_025_000,
            1_700_000_020_000, 35
        ))
        self.assertFalse(module.store_history(
            "alpha", "display/speed_kmh", 0, "number", 1_700_000_040_000,
            1_700_000_025_000, 0
        ))
        self.assertEqual([30, 0], [item["value"] for item in history.items])

    def test_power_history_samples_active_minutes_and_one_zero(self):
        history = VehicleHistoryTable()
        module = load_lambda(
            "StateIngestFunction",
            {
                "state": VehicleStateTable(), "connections": ConnectionTable(),
                "access": AccessTable({}), "history": history,
            },
            {
                "TABLE_NAME": "state", "CONNECTION_TABLE_NAME": "connections",
                "ACCESS_TABLE_NAME": "access", "WEBSOCKET_API_ID": "api",
                "WEBSOCKET_STAGE": "$default", "AWS_REGION": "eu-north-1",
                "HISTORY_TABLE_NAME": "history", "HISTORY_ENABLED": "true",
                "HISTORY_RETENTION_DAYS": "31",
                "HISTORY_MOTION_ENABLED": "true",
                "HISTORY_VEHICLE_ALLOWLIST": "alpha",
                "HISTORY_CORE_INTERVAL_SECONDS": "300",
                "HISTORY_MOTION_INTERVAL_SECONDS": "60",
            },
        )
        self.assertTrue(module.store_history(
            "alpha", "charging/power_signed", -80, "number",
            1_700_000_001_000, 1_699_999_930_000, -20
        ))
        self.assertFalse(module.store_history(
            "alpha", "charging/power_signed", -90, "number",
            1_700_000_020_000, 1_700_000_001_000, -80
        ))
        self.assertTrue(module.store_history(
            "alpha", "charging/power_signed", 0, "number",
            1_700_000_025_000, 1_700_000_020_000, -90
        ))
        self.assertFalse(module.store_history(
            "alpha", "charging/power_signed", 0, "number",
            1_700_000_040_000, 1_700_000_025_000, 0
        ))
        self.assertEqual([-80, 0], [item["value"] for item in history.items])


class TemplateStructureTests(unittest.TestCase):
    def test_inline_python_compiles(self):
        for resource in (
            "StateIngestFunction", "VehicleApiFunction",
            "LiveAuthorizerFunction", "LiveHandlerFunction",
        ):
            compile(inline_code(resource), resource, "exec")

    def test_history_guardrails_are_fail_closed(self):
        template = TEMPLATE.read_text(encoding="utf-8")
        self.assertIn("EnableTelemetryHistory:", template)
        self.assertIn("EnableHistoryMotionSignals:", template)
        self.assertIn("HistoryVehicleAllowlist:", template)
        self.assertIn("HistoryCoreIntervalSeconds:", template)
        self.assertIn("HistoryMotionIntervalSeconds:", template)
        self.assertIn('Default: ""', template)
        self.assertIn("MaxValue: 31", template)
        self.assertIn("VehicleHistoryDailyWriteAlarm:", template)
        self.assertIn('RouteKey: "GET /api/vehicles/{vehicleId}/history"', template)


if __name__ == "__main__":
    unittest.main()
