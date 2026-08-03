from argparse import Namespace
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "tools/aws/admin_onboard_beta_user.py"
SPEC = importlib.util.spec_from_file_location("admin_onboard_beta_user", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class FakeAws:
    def __init__(
        self, *, user=None, owners=None, existing=None, vehicle=True,
        incomplete_scan=False, put_failures=0,
    ):
        self.user = user
        self.owners = owners or set()
        self.existing = existing
        self.vehicle = vehicle
        self.incomplete_scan = incomplete_scan
        self.put_failures = put_failures
        self.calls = []

    def run(self, args):
        self.calls.append(args)
        service, operation = args[:2]
        if (service, operation) == ("cloudformation", "describe-stacks"):
            return {"Stacks": [{"Outputs": [
                {"OutputKey": "CognitoUserPoolId", "OutputValue": "pool"},
                {"OutputKey": "UserVehicleAccessTableName", "OutputValue": "access"},
                {"OutputKey": "VehicleStateTableName", "OutputValue": "state"},
            ]}]}
        if (service, operation) == ("dynamodb", "query"):
            return {"Items": [{"vehicleId": {"S": "alpha"}}] if self.vehicle else []}
        if (service, operation) == ("cognito-idp", "list-users"):
            return {"Users": [self.user] if self.user else []}
        if (service, operation) == ("dynamodb", "scan"):
            result = {"Items": [{"userSub": {"S": sub}} for sub in self.owners]}
            if self.incomplete_scan:
                result["LastEvaluatedKey"] = {"userSub": {"S": "cursor"}}
            return result
        if (service, operation) == ("dynamodb", "get-item"):
            return {"Item": self.existing} if self.existing else {}
        if (service, operation) == ("cognito-idp", "admin-create-user"):
            self.user = {"Attributes": [{"Name": "sub", "Value": "new-sub"}]}
            return {"User": self.user}
        if (service, operation) == ("dynamodb", "put-item"):
            if self.put_failures:
                self.put_failures -= 1
                raise MODULE.OnboardingError("simulated assignment failure")
            return {}
        raise AssertionError(args)


def args(*, apply=False):
    return Namespace(
        stack_name="stack", region="eu-north-1", email="beta@example.org",
        vehicle_id="alpha", source="test", apply=apply,
    )


class ControlledOnboardingTests(unittest.TestCase):
    def test_default_plan_has_no_mutating_calls(self):
        aws = FakeAws()
        result = MODULE.onboard(args(), aws)
        self.assertEqual("invite-required", result["user"])
        self.assertEqual("create-required", result["assignment"])
        operations = {(call[0], call[1]) for call in aws.calls}
        self.assertNotIn(("cognito-idp", "admin-create-user"), operations)
        self.assertNotIn(("dynamodb", "put-item"), operations)

    def test_apply_invites_and_assigns_without_returning_identity(self):
        aws = FakeAws()
        result = MODULE.onboard(args(apply=True), aws)
        self.assertEqual("invited", result["user"])
        self.assertEqual("created-active-owner", result["assignment"])
        self.assertNotIn("email", result)
        self.assertNotIn("userSub", result)

    def test_existing_active_assignment_is_idempotent(self):
        user = {"Attributes": [{"Name": "sub", "Value": "user-a"}]}
        existing = {"status": {"S": "ACTIVE"}}
        aws = FakeAws(user=user, owners={"user-a"}, existing=existing)
        result = MODULE.onboard(args(apply=True), aws)
        self.assertEqual("already-active", result["assignment"])
        self.assertFalse(any(call[:2] == ["dynamodb", "put-item"] for call in aws.calls))

    def test_invitation_assignment_failure_is_resumable(self):
        aws = FakeAws(put_failures=1)
        with self.assertRaisesRegex(MODULE.OnboardingError, "assignment failure"):
            MODULE.onboard(args(apply=True), aws)

        result = MODULE.onboard(args(apply=True), aws)
        self.assertEqual("existing", result["user"])
        self.assertEqual("created-active-owner", result["assignment"])
        create_calls = [
            call for call in aws.calls
            if call[:2] == ["cognito-idp", "admin-create-user"]
        ]
        self.assertEqual(1, len(create_calls))

    def test_other_active_owner_fails_closed(self):
        aws = FakeAws(owners={"other-user"})
        with self.assertRaisesRegex(MODULE.OnboardingError, "another ACTIVE owner"):
            MODULE.onboard(args(apply=True), aws)

    def test_revoked_assignment_requires_review(self):
        user = {"Attributes": [{"Name": "sub", "Value": "user-a"}]}
        existing = {"status": {"S": "REVOKED"}}
        aws = FakeAws(user=user, existing=existing)
        with self.assertRaisesRegex(MODULE.OnboardingError, "review required"):
            MODULE.onboard(args(apply=True), aws)

    def test_unknown_vehicle_fails_before_mutation(self):
        aws = FakeAws(vehicle=False)
        with self.assertRaisesRegex(MODULE.OnboardingError, "no telemetry state"):
            MODULE.onboard(args(apply=True), aws)
        self.assertFalse(any(call[:2] == ["cognito-idp", "admin-create-user"] for call in aws.calls))

    def test_incomplete_owner_scan_fails_closed(self):
        aws = FakeAws(incomplete_scan=True)
        with self.assertRaisesRegex(MODULE.OnboardingError, "scan incomplete"):
            MODULE.onboard(args(apply=True), aws)

    def test_existing_user_without_subject_fails_closed(self):
        aws = FakeAws(user={"Attributes": []})
        with self.assertRaisesRegex(MODULE.OnboardingError, "has no sub"):
            MODULE.onboard(args(apply=True), aws)

    def test_personal_or_malformed_source_is_rejected(self):
        invalid = args()
        invalid.source = "operator@example.org"
        with self.assertRaisesRegex(MODULE.OnboardingError, "source reference"):
            MODULE.onboard(invalid, FakeAws())


if __name__ == "__main__":
    unittest.main()
