from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import types
import unittest


ROOT = Path(__file__).resolve().parents[4]
HANDLER = ROOT / "cloud/aws/onboarding/src/handler.py"


class ClientError(Exception):
    pass


class Serializer:
    def serialize(self, value):
        if isinstance(value, bool):
            return {"BOOL": value}
        if isinstance(value, (int, float)):
            return {"N": str(value)}
        return {"S": str(value)}


class Key:
    def __init__(self, name):
        self.name = name

    def eq(self, value):
        return (self.name, value)


class Table:
    def __init__(self, name, fixture):
        self.name = name
        self.fixture = fixture

    def query(self, **_kwargs):
        return {"Items": self.fixture.get("state", [])}

    def scan(self, **_kwargs):
        return self.fixture.get("access_scan", {"Items": []})

    def get_item(self, **kwargs):
        key = next(iter(kwargs["Key"].values()))
        item = self.fixture.get(self.name, {}).get(key)
        return {"Item": item} if item else {}


class Resource:
    def __init__(self, fixture):
        self.fixture = fixture

    def Table(self, name):
        return Table(name, self.fixture)


class Client:
    def __init__(self):
        self.transactions = []

    def transact_write_items(self, **kwargs):
        self.transactions.append(kwargs["TransactItems"])


def load_handler():
    boto3 = types.ModuleType("boto3")
    boto3.resource = lambda _name: Resource({})
    boto3.client = lambda _name: Client()
    dynamodb = types.ModuleType("boto3.dynamodb")
    types_module = types.ModuleType("boto3.dynamodb.types")
    types_module.TypeSerializer = Serializer
    conditions = types.ModuleType("boto3.dynamodb.conditions")
    conditions.Key = Key
    botocore = types.ModuleType("botocore")
    exceptions = types.ModuleType("botocore.exceptions")
    exceptions.ClientError = ClientError
    modules = {
        "boto3": boto3, "boto3.dynamodb": dynamodb,
        "boto3.dynamodb.types": types_module,
        "boto3.dynamodb.conditions": conditions,
        "botocore": botocore, "botocore.exceptions": exceptions,
    }
    previous = {name: sys.modules.get(name) for name in modules}
    sys.modules.update(modules)
    try:
        spec = importlib.util.spec_from_file_location("onboarding_handler", HANDLER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    finally:
        for name, value in previous.items():
            if value is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = value
    module.CLAIMS_TABLE = "claims"
    module.OWNERSHIP_TABLE = "owners"
    module.ACCESS_TABLE = "access"
    module.AUDIT_TABLE = "audit"
    module.STATE_TABLE = "state-table"
    return module


def event(route, body, *, subject="user-a", groups=None):
    claims = {"sub": subject}
    if groups is not None:
        claims["cognito:groups"] = groups
    return {
        "routeKey": route,
        "body": json.dumps(body),
        "requestContext": {"authorizer": {"jwt": {"claims": claims}}},
    }


class ClaimHandlerTests(unittest.TestCase):
    def setUp(self):
        self.module = load_handler()
        self.client = Client()
        self.module.ddb_client = self.client

    def test_issue_requires_admin_group(self):
        self.module.dynamodb = Resource({"state": [{"vehicleId": "alpha"}]})
        result = self.module.handler(
            event("POST /api/onboarding/claims", {"vehicleId": "alpha"}), None
        )
        self.assertEqual(403, result["statusCode"])
        self.assertFalse(self.client.transactions)

    def test_issue_returns_plaintext_once_but_stores_only_hash(self):
        self.module.dynamodb = Resource({"state": [{"vehicleId": "alpha"}]})
        result = self.module.handler(event(
            "POST /api/onboarding/claims", {"vehicleId": "alpha"},
            subject="admin-a", groups=["mot-beta-admins"],
        ), None)
        self.assertEqual(201, result["statusCode"])
        response = json.loads(result["body"])
        self.assertIn(".", response["claim"])
        stored = self.client.transactions[0][0]["Put"]["Item"]
        self.assertIn("proofHash", stored)
        self.assertNotIn("proof", stored)

    def test_issue_fails_closed_for_existing_b1_assignment(self):
        self.module.dynamodb = Resource({
            "state": [{"vehicleId": "alpha"}],
            "access_scan": {"Items": [{"userSub": "existing-owner"}]},
        })
        result = self.module.handler(event(
            "POST /api/onboarding/claims", {"vehicleId": "alpha"},
            subject="admin-a", groups=["mot-beta-admins"],
        ), None)
        self.assertEqual(409, result["statusCode"])
        self.assertFalse(self.client.transactions)

    def test_issue_fails_closed_for_incomplete_legacy_scan(self):
        self.module.dynamodb = Resource({
            "state": [{"vehicleId": "alpha"}],
            "access_scan": {"Items": [], "LastEvaluatedKey": {"userSub": "cursor"}},
        })
        result = self.module.handler(event(
            "POST /api/onboarding/claims", {"vehicleId": "alpha"},
            subject="admin-a", groups=["mot-beta-admins"],
        ), None)
        self.assertEqual(409, result["statusCode"])
        self.assertFalse(self.client.transactions)

    def test_valid_claim_uses_one_four_item_transaction(self):
        now = 2_000_000_000
        claim_id = "A" * 24
        proof = "B" * 32
        salt = "C" * 24
        record = {
            "claimId": claim_id, "vehicleId": "alpha", "proofSalt": salt,
            "proofHash": self.module._hash(claim_id, salt, proof),
            "status": "ISSUED", "expiresAt": now + 100,
            "failedAttempts": 0, "maxAttempts": 5,
        }
        self.module.time.time = lambda: now
        self.module.dynamodb = Resource({"claims": {claim_id: record}})
        result = self.module.handler(event(
            "POST /api/onboarding/claim", {"claim": f"{claim_id}.{proof}"}
        ), None)
        self.assertEqual(200, result["statusCode"])
        self.assertEqual(4, len(self.client.transactions[0]))
        self.assertIn("Update", self.client.transactions[0][0])
        self.assertEqual("owners", self.client.transactions[0][1]["Put"]["TableName"])
        self.assertEqual("access", self.client.transactions[0][2]["Put"]["TableName"])

    def test_demo_owner_cannot_consume_claim(self):
        self.module.CLAIM_READ_ONLY_VEHICLE_IDS = {"demo-pioneer"}
        self.module.dynamodb = Resource({
            "access": {
                "user-a": {"vehicleId": "demo-pioneer", "status": "ACTIVE"},
            }
        })
        result = self.module.handler(event(
            "POST /api/onboarding/claim", {"claim": f"{'A' * 24}.{'B' * 32}"}
        ), None)
        self.assertEqual(403, result["statusCode"])
        self.assertEqual("onboarding_read_only", json.loads(result["body"])["error"])
        self.assertFalse(self.client.transactions)

    def test_inactive_demo_assignment_does_not_lock_claims(self):
        self.module.CLAIM_READ_ONLY_VEHICLE_IDS = {"demo-pioneer"}
        self.module.dynamodb = Resource({
            "access": {
                "user-a": {"vehicleId": "demo-pioneer", "status": "REVOKED"},
            }
        })
        result = self.module.handler(event(
            "POST /api/onboarding/claim", {"claim": "invalid"}
        ), None)
        self.assertEqual(409, result["statusCode"])

    def test_bad_proof_is_generic_and_audited_with_attempt(self):
        now = 2_000_000_000
        claim_id = "A" * 24
        record = {
            "claimId": claim_id, "vehicleId": "alpha", "proofSalt": "C" * 24,
            "proofHash": "sha256:" + "D" * 43, "status": "ISSUED",
            "expiresAt": now + 100, "failedAttempts": 0, "maxAttempts": 5,
        }
        self.module.time.time = lambda: now
        self.module.dynamodb = Resource({"claims": {claim_id: record}})
        result = self.module.handler(event(
            "POST /api/onboarding/claim", {"claim": f"{claim_id}.{'B' * 32}"}
        ), None)
        self.assertEqual(409, result["statusCode"])
        self.assertEqual(
            {"error": "claim_invalid_or_unavailable"}, json.loads(result["body"])
        )
        self.assertEqual(2, len(self.client.transactions[0]))


if __name__ == "__main__":
    unittest.main()
