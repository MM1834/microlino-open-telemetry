import importlib.util
import json
import sys
import types
import unittest
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "preference_api.py"


class FakeTable:
    def __init__(self, item=None):
        self.item = dict(item or {})

    def get_item(self, **_kwargs):
        return {"Item": dict(self.item)} if self.item else {}

    def put_item(self, Item):
        self.item = dict(Item)

    def update_item(self, Key, UpdateExpression, ConditionExpression,
                    ExpressionAttributeValues):
        assert UpdateExpression == "SET emailConfirmed=:confirmed"
        assert ConditionExpression == "emailSubscriptionArn=:subscription"
        assert self.item.get("emailSubscriptionArn") == ExpressionAttributeValues[":subscription"]
        self.item["emailConfirmed"] = ExpressionAttributeValues[":confirmed"]


class FakeSns:
    def __init__(self):
        self.subscriptions = []
        self.subscription_attributes = {}

    def subscribe(self, **kwargs):
        self.subscriptions.append(kwargs)
        return {"SubscriptionArn": "pending"}

    def get_subscription_attributes(self, SubscriptionArn):
        return {"Attributes": dict(self.subscription_attributes.get(
            SubscriptionArn, {"PendingConfirmation": "true"}
        ))}


def load_module(preference_item=None, read_only_vehicle_ids=""):
    preference = FakeTable(preference_item)
    access = FakeTable({"status": "ACTIVE"})
    sns = FakeSns()
    boto3 = types.ModuleType("boto3")
    boto3.resource = lambda _name: types.SimpleNamespace(
        Table=lambda name: preference if name == "preferences" else access
    )
    boto3.client = lambda _name: sns
    previous = sys.modules.get("boto3")
    sys.modules["boto3"] = boto3
    module = importlib.util.module_from_spec(
        importlib.util.spec_from_file_location("journey_preference_api", SOURCE)
    )
    old_environment = __import__("os").environ.copy()
    __import__("os").environ.update({
        "PREFERENCE_TABLE_NAME": "preferences",
        "ACCESS_TABLE_NAME": "access",
        "EMAIL_TOPIC_ARN": "arn:email",
        "READ_ONLY_VEHICLE_IDS": read_only_vehicle_ids,
    })
    try:
        module.__spec__.loader.exec_module(module)
    finally:
        __import__("os").environ.clear()
        __import__("os").environ.update(old_environment)
        if previous is None:
            sys.modules.pop("boto3", None)
        else:
            sys.modules["boto3"] = previous
    return module, preference, sns


def event(method="GET", body=None, vehicle_id="pioneer"):
    return {
        "requestContext": {
            "http": {"method": method},
            "authorizer": {"jwt": {"claims": {"sub": "user-a"}}},
        },
        "pathParameters": {"vehicleId": vehicle_id},
        "body": json.dumps(body) if body is not None else None,
    }


class JourneyPreferenceApiTests(unittest.TestCase):
    def test_read_only_vehicle_get_is_disabled_without_preference_lookup(self):
        module, preference, sns = load_module(
            {"emailEnabled": True, "email": "old@example.com"},
            read_only_vehicle_ids="demo-pioneer",
        )
        result = module.handler(event(vehicle_id="demo-pioneer"), None)
        body = json.loads(result["body"])
        self.assertEqual(200, result["statusCode"])
        self.assertTrue(body["readOnly"])
        self.assertFalse(body["emailEnabled"])
        self.assertEqual([], sns.subscriptions)

    def test_read_only_vehicle_put_cannot_subscribe_or_write(self):
        module, preference, sns = load_module(read_only_vehicle_ids="demo-pioneer")
        result = module.handler(event("PUT", {
            "enabled": True, "threshold": 80, "emailEnabled": True,
            "email": "victim@example.com",
        }, vehicle_id="demo-pioneer"), None)
        self.assertEqual(403, result["statusCode"])
        self.assertEqual("notifications_read_only", json.loads(result["body"])["error"])
        self.assertEqual({}, preference.item)
        self.assertEqual([], sns.subscriptions)

    def test_default_is_off(self):
        module, _, _ = load_module()
        result = module.handler(event(), None)
        self.assertEqual(200, result["statusCode"])
        self.assertFalse(json.loads(result["body"])["journeyEmailEnabled"])

    def test_get_reconciles_confirmed_sns_subscription(self):
        previous = {
            "vehicleId": "pioneer", "userSub": "user-a", "enabled": True,
            "threshold": 80, "emailEnabled": True,
            "email": "driver@example.com", "emailConfirmed": False,
            "emailSubscriptionArn": "arn:confirmed",
        }
        module, preference, sns = load_module(previous)
        sns.subscription_attributes["arn:confirmed"] = {"PendingConfirmation": "false"}
        result = module.handler(event(), None)
        self.assertTrue(json.loads(result["body"])["emailConfirmed"])
        self.assertTrue(preference.item["emailConfirmed"])

    def test_get_keeps_pending_sns_subscription_unconfirmed(self):
        previous = {
            "vehicleId": "pioneer", "userSub": "user-a", "enabled": True,
            "threshold": 80, "emailEnabled": True,
            "email": "driver@example.com", "emailConfirmed": False,
            "emailSubscriptionArn": "arn:pending",
        }
        module, preference, _ = load_module(previous)
        result = module.handler(event(), None)
        self.assertFalse(json.loads(result["body"])["emailConfirmed"])
        self.assertFalse(preference.item["emailConfirmed"])

    def test_journey_email_requires_email_channel(self):
        module, _, _ = load_module()
        result = module.handler(event("PUT", {
            "enabled": False,
            "threshold": 80,
            "emailEnabled": False,
            "journeyEmailEnabled": True,
        }), None)
        self.assertEqual(400, result["statusCode"])
        self.assertEqual("journey_email_requires_email", json.loads(result["body"])["error"])

    def test_opt_in_is_persisted_and_subscribes_new_email(self):
        module, preference, sns = load_module()
        result = module.handler(event("PUT", {
            "enabled": False,
            "threshold": 80,
            "emailEnabled": True,
            "journeyEmailEnabled": True,
            "email": "driver@example.com",
        }), None)
        self.assertEqual(200, result["statusCode"])
        self.assertTrue(preference.item["journeyEmailEnabled"])
        self.assertTrue(json.loads(result["body"])["journeyEmailEnabled"])
        self.assertEqual(1, len(sns.subscriptions))

    def test_old_client_preserves_opt_in_until_email_channel_is_disabled(self):
        previous = {
            "vehicleId": "pioneer", "userSub": "user-a", "enabled": False,
            "threshold": 80, "emailEnabled": True, "journeyEmailEnabled": True,
            "email": "driver@example.com", "emailConfirmed": True,
        }
        module, preference, _ = load_module(previous)
        module.handler(event("PUT", {
            "enabled": False, "threshold": 80, "emailEnabled": True,
            "email": "driver@example.com",
        }), None)
        self.assertTrue(preference.item["journeyEmailEnabled"])

        module.handler(event("PUT", {
            "enabled": False, "threshold": 80, "emailEnabled": False,
            "email": "driver@example.com",
        }), None)
        self.assertFalse(preference.item["journeyEmailEnabled"])


if __name__ == "__main__":
    unittest.main()
