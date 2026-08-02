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
        return self.name, value


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

    def post_to_connection(self, **kwargs):
        self.posts.append(kwargs)


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
        self.module = load_lambda(
            "VehicleApiFunction",
            {"state": VehicleStateTable(), "access": access},
            {"TABLE_NAME": "state", "ACCESS_TABLE_NAME": "access"},
        )

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
            {"state": VehicleStateTable(), "connections": connections, "access": access},
            {
                "TABLE_NAME": "state", "CONNECTION_TABLE_NAME": "connections",
                "ACCESS_TABLE_NAME": "access", "WEBSOCKET_API_ID": "api",
                "WEBSOCKET_STAGE": "$default", "AWS_REGION": "eu-north-1",
            },
            management,
        )
        module.time = types.SimpleNamespace(time=lambda: 1000)
        result = module.broadcast_live_update(
            "alpha", "mot/alpha/status/online", "status/online", True, "boolean", 1
        )
        self.assertEqual({"sent": 1, "removed": 2}, result)
        self.assertEqual(["active"], [post["ConnectionId"] for post in management.posts])


class TemplateStructureTests(unittest.TestCase):
    def test_inline_python_compiles(self):
        for resource in (
            "StateIngestFunction", "VehicleApiFunction",
            "LiveAuthorizerFunction", "LiveHandlerFunction",
        ):
            compile(inline_code(resource), resource, "exec")


if __name__ == "__main__":
    unittest.main()
