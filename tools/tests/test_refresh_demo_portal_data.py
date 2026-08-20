from argparse import Namespace
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
PATH = ROOT / "tools/aws/refresh_demo_portal_data.py"
SPEC = importlib.util.spec_from_file_location("refresh_demo_portal_data", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def state(vehicle="source", suffix="display/soc", received=1_000_000, value=None):
    value = value or {"N": "50"}
    return {
        "vehicleId": {"S": vehicle}, "topicSuffix": {"S": suffix},
        "fullTopic": {"S": f"mot/{vehicle}/{suffix}"},
        "category": {"S": suffix.split("/")[0]}, "receivedAt": {"N": str(received)},
        "value": value, "valueType": {"S": "number"},
        "payloadText": {"S": MODULE.text_value(value)}, "payloadBytes": {"N": "2"},
    }


def history(vehicle="source", signal="soc", sampled=1_000_000):
    seconds = sampled // 1000
    return {
        "vehicleId": {"S": vehicle}, "sampleKey": {"S": f"{signal}#{seconds:010d}"},
        "signal": {"S": signal}, "sampledAt": {"N": str(sampled)},
        "receivedAt": {"N": str(sampled + 20)}, "expiresAt": {"N": "99"},
        "value": {"N": "50"}, "valueType": {"S": "number"},
    }


class FakeAws:
    def __init__(self):
        self.calls = []
        self.source_state = [state(), state(suffix="system/device_id"), state(suffix="location/latitude")]
        self.source_history = [history()]

    def run(self, args):
        self.calls.append(args)
        op = tuple(args[:2])
        if op == ("cloudformation", "describe-stacks"):
            return {"Stacks": [{"Outputs": [
                {"OutputKey": "CognitoUserPoolId", "OutputValue": "pool"},
                {"OutputKey": "UserVehicleAccessTableName", "OutputValue": "access"},
                {"OutputKey": "VehicleStateTableName", "OutputValue": "state"},
                {"OutputKey": "VehicleHistoryTableName", "OutputValue": "history"},
            ]}]}
        if op == ("dynamodb", "query"):
            table = args[args.index("--table-name") + 1]
            values = args[args.index("--expression-attribute-values") + 1]
            target = '"demo-pioneer"' in values
            return {"Items": [] if target else (self.source_state if table == "state" else self.source_history)}
        if op == ("cognito-idp", "list-users"):
            return {"Users": []}
        if op == ("cognito-idp", "admin-create-user"):
            return {"User": {"Attributes": [{"Name": "sub", "Value": "demo-sub"}]}}
        if op == ("dynamodb", "get-item"):
            return {}
        if op in (("dynamodb", "put-item"), ("dynamodb", "batch-write-item")):
            return {}
        raise AssertionError(args)


def args(apply=False, target="demo-pioneer"):
    return Namespace(
        stack_name="stack", region="eu-north-1", email="demo@example.org",
        source_vehicle="source", target_vehicle=target, history_days=30,
        latitude=47.4, longitude=8.1, apply=apply,
    )


class DemoRefreshTests(unittest.TestCase):
    def test_plan_is_read_only_and_reports_sanitized_counts(self):
        aws = FakeAws()
        result = MODULE.refresh(args(), aws, now_ms=2_000_000)
        self.assertEqual(3, result["stateItems"])
        self.assertEqual(1, result["historyItems"])
        mutations = {("dynamodb", "put-item"), ("dynamodb", "batch-write-item"), ("cognito-idp", "admin-create-user")}
        self.assertFalse(any(tuple(call[:2]) in mutations for call in aws.calls))

    def test_state_removes_identifiers_and_replaces_location(self):
        items = MODULE.transform_state(
            [state(), state(suffix="system/device_id"), state(suffix="gps/latitude")],
            "source", "demo-pioneer", 1000, 2_000_000, 47.4, 8.1,
        )
        suffixes = {MODULE.string(item, "topicSuffix") for item in items}
        self.assertEqual({"display/soc", "location/latitude", "location/longitude"}, suffixes)
        self.assertTrue(all(MODULE.string(item, "vehicleId") == "demo-pioneer" for item in items))

    def test_history_preserves_shape_and_shifts_latest_to_now(self):
        items = MODULE.transform_history([history(sampled=1_000_000)], "demo-pioneer", 1_000_000, 2_000_000, 30)
        self.assertEqual(1_900_000, MODULE.number(items[0], "sampledAt"))
        self.assertEqual("soc#0000001900", MODULE.string(items[0], "sampleKey"))
        self.assertEqual(2_000 + 31 * MODULE.DAY_SECONDS, MODULE.number(items[0], "expiresAt"))

    def test_apply_invites_without_password_and_assigns(self):
        aws = FakeAws()
        result = MODULE.refresh(args(apply=True), aws, now_ms=2_000_000)
        self.assertEqual("invited", result["user"])
        self.assertEqual("created-active-owner", result["assignment"])
        create = next(call for call in aws.calls if call[:2] == ["cognito-idp", "admin-create-user"])
        self.assertNotIn("--temporary-password", create)

    def test_motion_samples_are_averaged_into_five_minute_buckets(self):
        first = history(signal="speed", sampled=1_000_000)
        first["value"] = {"N": "10"}
        second = history(signal="speed", sampled=1_100_000)
        second["value"] = {"N": "20"}
        items = MODULE.transform_history([first, second], "demo-pioneer", 1_100_000, 2_000_000, 30)
        self.assertEqual(1, len(items))
        self.assertEqual("15", items[0]["value"]["N"])

    def test_non_demo_target_fails_before_aws(self):
        aws = FakeAws()
        with self.assertRaisesRegex(MODULE.DemoDataError, "must start"):
            MODULE.refresh(args(target="production"), aws)
        self.assertEqual([], aws.calls)


if __name__ == "__main__":
    unittest.main()
