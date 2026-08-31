from argparse import Namespace
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "tools/aws/admin_sms_approval.py"
SPEC = importlib.util.spec_from_file_location("admin_sms_approval", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FakeAws:
    def __init__(self, *, access=True, existing=None):
        self.access = access
        self.existing = existing
        self.calls = []
        self.assumed = False

    def assume(self, role_arn, region):
        self.assumed = True
        self.calls.append(["sts", "assume-role", role_arn, region])
        return self

    def run(self, args):
        self.calls.append(args)
        service, operation = args[:2]
        if (service, operation) == ("cloudformation", "describe-stacks"):
            return {"Stacks": [{"Outputs": [
                {"OutputKey": "SmsApprovalTableName", "OutputValue": "approvals"},
                {"OutputKey": "SmsApprovalAuditTableName", "OutputValue": "audit"},
                {"OutputKey": "SmsApprovalAuditRetentionDays", "OutputValue": "90"},
                {"OutputKey": "SmsApprovalAdminRoleArn", "OutputValue": "arn:role"},
            ]}]}
        if (service, operation) == ("sts", "get-caller-identity"):
            return {"Arn": "arn:aws:iam::123456789012:user/operator"}
        if (service, operation) == ("dynamodb", "get-item"):
            table = args[args.index("--table-name") + 1]
            if table == "access":
                return {"Item": {"status": {"S": "ACTIVE"}}} if self.access else {}
            if table == "approvals":
                return {"Item": self.existing} if self.existing else {}
        if (service, operation) == ("dynamodb", "transact-write-items"):
            return {}
        raise AssertionError(args)


def args(*, action="approve", apply=False, phone="+41791234567"):
    return Namespace(
        action=action, stack_name="stack", region="eu-north-1",
        access_table_name="access", user_sub="user-a", vehicle_id="pioneer",
        phone_e164=phone, expires_days=30, reason="sms-001-test", apply=apply,
    )


def active(version=1, fingerprint=None):
    return {
        "vehicleId": {"S": "pioneer"}, "userSub": {"S": "user-a"},
        "status": {"S": "ACTIVE"}, "version": {"N": str(version)},
        "destinationFingerprint": {"S": fingerprint or "a" * 64},
    }


class SmsApprovalAdminTests(unittest.TestCase):
    def test_plan_is_non_mutating_and_redacts_phone_to_fingerprint(self):
        aws = FakeAws()
        result = MODULE.operate(args(), aws)
        self.assertEqual("plan", result["mode"])
        self.assertEqual("ACTIVE", result["nextStatus"])
        self.assertEqual(64, len(result["destinationFingerprint"]))
        self.assertNotIn("+41791234567", json.dumps(result))
        self.assertFalse(any(call[:2] == ["dynamodb", "transact-write-items"] for call in aws.calls))

    def test_apply_writes_approval_and_audit_atomically_without_phone(self):
        aws = FakeAws()
        result = MODULE.operate(args(apply=True), aws)
        self.assertEqual("ACTIVE", result["nextStatus"])
        transaction_call = next(
            call for call in aws.calls
            if call[:2] == ["dynamodb", "transact-write-items"]
        )
        payload = transaction_call[transaction_call.index("--transact-items") + 1]
        self.assertEqual(2, len(json.loads(payload)))
        self.assertNotIn("+41791234567", payload)
        self.assertIn("destinationFingerprint", payload)

    def test_renewal_uses_optimistic_version_condition(self):
        aws = FakeAws(existing=active(version=4))
        result = MODULE.operate(args(apply=True), aws)
        self.assertEqual(5, result["nextVersion"])
        call = next(call for call in aws.calls if call[:2] == ["dynamodb", "transact-write-items"])
        payload = json.loads(call[call.index("--transact-items") + 1])
        self.assertEqual("version = :expected", payload[0]["Put"]["ConditionExpression"])
        self.assertEqual({"N": "4"}, payload[0]["Put"]["ExpressionAttributeValues"][":expected"])

    def test_revoke_requires_active_and_preserves_stored_fingerprint(self):
        aws = FakeAws(existing=active(version=2, fingerprint="b" * 64))
        result = MODULE.operate(args(action="revoke", apply=True), aws)
        self.assertEqual("REVOKED", result["nextStatus"])
        call = next(call for call in aws.calls if call[:2] == ["dynamodb", "transact-write-items"])
        payload = call[call.index("--transact-items") + 1]
        self.assertIn("b" * 64, payload)
        self.assertNotIn("+41791234567", payload)

    def test_revoke_without_active_approval_fails_closed(self):
        with self.assertRaisesRegex(MODULE.ApprovalError, "no ACTIVE approval"):
            MODULE.operate(args(action="revoke"), FakeAws())

    def test_inactive_vehicle_access_fails_before_mutation(self):
        aws = FakeAws(access=False)
        with self.assertRaisesRegex(MODULE.ApprovalError, "ACTIVE vehicle access"):
            MODULE.operate(args(apply=True), aws)
        self.assertFalse(any(call[:2] == ["dynamodb", "transact-write-items"] for call in aws.calls))

    def test_unsupported_phone_and_personal_reason_are_rejected(self):
        with self.assertRaisesRegex(MODULE.ApprovalError, "\\+41 or \\+49"):
            MODULE.operate(args(phone="+331701234567"), FakeAws())
        personal = args()
        personal.reason = "operator@example.org"
        with self.assertRaisesRegex(MODULE.ApprovalError, "non-personal"):
            MODULE.operate(personal, FakeAws())


if __name__ == "__main__":
    unittest.main()
