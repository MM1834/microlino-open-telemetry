import importlib.util
import json
import os
import sys
import types
import unittest
from pathlib import Path


SOURCE = Path(__file__).resolve().parents[1] / "sms_verification_api.py"


class FakeTable:
    def __init__(self, keyed=None):
        self.items = dict(keyed or {})

    @staticmethod
    def _key(key):
        return tuple(sorted(key.items()))

    def get_item(self, Key, **_kwargs):
        item = self.items.get(self._key(Key))
        return {"Item": dict(item)} if item else {}

    def put_item(self, Item):
        key_name = "destinationFingerprint" if "destinationFingerprint" in Item else "vehicleId"
        key = {key_name: Item[key_name]}
        if key_name == "vehicleId":
            key["userSub"] = Item["userSub"]
        self.items[self._key(key)] = dict(Item)

    def update_item(self, Key, UpdateExpression, ExpressionAttributeValues,
                    ExpressionAttributeNames=None, ConditionExpression=None):
        item = self.items.setdefault(self._key(Key), dict(Key))
        if ConditionExpression == "phoneE164=:phone" and item.get("phoneE164") != ExpressionAttributeValues[":phone"]:
            raise AssertionError("phone changed")
        expression = UpdateExpression.replace("SET ", "", 1)
        set_part, _, remove_part = expression.partition(" REMOVE ")
        for assignment in set_part.split(","):
            name, value = (part.strip() for part in assignment.split("=", 1))
            name = (ExpressionAttributeNames or {}).get(name, name)
            item[name] = ExpressionAttributeValues[value]
        for name in remove_part.split(",") if remove_part else []:
            item.pop(name.strip(), None)


class FakeSms:
    def __init__(self):
        self.created = []
        self.sent = []
        self.verified = []

    def create_verified_destination_number(self, **kwargs):
        self.created.append(kwargs)
        return {"VerifiedDestinationNumberId": "vdn-1"}

    def send_destination_number_verification_code(self, **kwargs):
        self.sent.append(kwargs)

    def verify_destination_number(self, **kwargs):
        self.verified.append(kwargs)


def load_module(preference=None, destination=None, approval=None, read_only=""):
    key = (("userSub", "user-a"), ("vehicleId", "pioneer"))
    phone = "+41791234567"
    fingerprint = __import__("hashlib").sha256(phone.encode()).hexdigest()
    tables = {
        "preferences": FakeTable({key: preference} if preference else {}),
        "access": FakeTable({key: {"status": "ACTIVE"}}),
        "approvals": FakeTable({key: approval} if approval else {}),
        "destinations": FakeTable({(("destinationFingerprint", fingerprint),): destination} if destination else {}),
    }
    sms = FakeSms()
    boto3 = types.ModuleType("boto3")
    boto3.resource = lambda _name: types.SimpleNamespace(Table=lambda name: tables[name])
    boto3.client = lambda _name: sms
    previous = sys.modules.get("boto3")
    previous_botocore = sys.modules.get("botocore")
    previous_exceptions = sys.modules.get("botocore.exceptions")
    sys.modules["boto3"] = boto3
    botocore = types.ModuleType("botocore")
    exceptions = types.ModuleType("botocore.exceptions")
    exceptions.ClientError = type("ClientError", (Exception,), {})
    sys.modules["botocore"] = botocore
    sys.modules["botocore.exceptions"] = exceptions
    old_environment = os.environ.copy()
    os.environ.update({
        "PREFERENCE_TABLE_NAME": "preferences", "ACCESS_TABLE_NAME": "access",
        "SMS_APPROVAL_TABLE_NAME": "approvals", "SMS_DESTINATION_TABLE_NAME": "destinations",
        "SMS_SENDER_ARN_CH": "arn:sender-ch", "SMS_SENDER_ARN_DE": "arn:sender-de",
        "SMS_CONFIGURATION_SET": "mot-dev-sms",
        "READ_ONLY_VEHICLE_IDS": read_only,
    })
    module = importlib.util.module_from_spec(importlib.util.spec_from_file_location("sms_verification_api_test", SOURCE))
    try:
        module.__spec__.loader.exec_module(module)
    finally:
        os.environ.clear()
        os.environ.update(old_environment)
        if previous is None:
            sys.modules.pop("boto3", None)
        else:
            sys.modules["boto3"] = previous
        if previous_botocore is None:
            sys.modules.pop("botocore", None)
        else:
            sys.modules["botocore"] = previous_botocore
        if previous_exceptions is None:
            sys.modules.pop("botocore.exceptions", None)
        else:
            sys.modules["botocore.exceptions"] = previous_exceptions
    return module, tables, sms, fingerprint


def event(method="GET", route="GET /api/vehicles/{vehicleId}/notifications/sms", body=None):
    return {
        "requestContext": {"http": {"method": method}, "authorizer": {"jwt": {"claims": {"sub": "user-a"}}}},
        "pathParameters": {"vehicleId": "pioneer"}, "routeKey": route,
        "body": json.dumps(body) if body is not None else None,
    }


class SmsVerificationApiTests(unittest.TestCase):
    def test_rejects_unsupported_number_before_provider_call(self):
        module, _, sms, _ = load_module()
        result = module.handler(event("POST", "POST x/request", {"phoneE164": "+331701234567"}), None)
        self.assertEqual(400, result["statusCode"])
        self.assertEqual([], sms.created)

    def test_requests_code_and_stores_only_fingerprint_in_registry(self):
        module, tables, sms, fingerprint = load_module()
        result = module.handler(event("POST", "POST x/request", {"phoneE164": "+41 79 123 45 67"}), None)
        self.assertEqual(200, result["statusCode"])
        self.assertEqual(1, len(sms.created))
        self.assertNotIn("Tags", sms.created[0])
        self.assertEqual(1, len(sms.sent))
        registry = tables["destinations"].items[(("destinationFingerprint", fingerprint),)]
        self.assertNotIn("phoneE164", registry)
        self.assertEqual("PENDING", registry["status"])

    def test_reuses_shared_verified_destination_without_second_code(self):
        module, tables, sms, _ = load_module(destination={
            "destinationFingerprint": "ignored", "verifiedDestinationNumberId": "vdn-1", "status": "VERIFIED"
        })
        result = module.handler(event("POST", "POST x/request", {"phoneE164": "+41791234567"}), None)
        self.assertEqual(200, result["statusCode"])
        self.assertEqual([], sms.created)
        self.assertEqual([], sms.sent)
        saved = next(iter(tables["preferences"].items.values()))
        self.assertTrue(saved["smsConfirmed"])

    def test_status_requires_matching_per_association_approval(self):
        now = 2_000_000_000
        module, _, _, fingerprint = load_module(
            preference={"vehicleId": "pioneer", "userSub": "user-a", "phoneE164": "+41791234567"},
            destination={"status": "VERIFIED"},
            approval={"status": "ACTIVE", "destinationFingerprint": "wrong", "isoCountryCode": "CH", "originator": "MOT", "expiresAt": now},
        )
        result = json.loads(module.handler(event(), None)["body"])
        self.assertTrue(result["verificationStatus"] == "VERIFIED")
        self.assertFalse(result["smsApproved"])

    def test_read_only_vehicle_cannot_start_verification(self):
        module, _, sms, _ = load_module(read_only="pioneer")
        result = module.handler(event("POST", "POST x/request", {"phoneE164": "+41791234567"}), None)
        self.assertEqual(403, result["statusCode"])
        self.assertEqual([], sms.created)


if __name__ == "__main__":
    unittest.main()
