import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / "cloud/aws/onboarding/schemas/lifecycle-operation-record.schema.json"
AUDIT_SCHEMA = ROOT / "cloud/aws/onboarding/schemas/audit-event.schema.json"


class OnboardingLifecycleSchemaTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        cls.props = cls.schema["properties"]
        cls.audit = json.loads(AUDIT_SCHEMA.read_text(encoding="utf-8"))

    def test_schema_is_versioned_and_closed(self):
        self.assertFalse(self.schema["additionalProperties"])
        self.assertEqual(1, self.props["schemaVersion"]["const"])
        self.assertIn("operationId", self.schema["required"])
        self.assertIn("version", self.schema["required"])
        self.assertIn("phase", self.schema["required"])

    def test_all_lifecycle_operations_are_explicit(self):
        self.assertEqual(
            {
                "REPLACE_ADAPTER", "TRANSFER_OWNERSHIP", "REPORT_LOST",
                "RECOVER_ADAPTER", "FACTORY_RESET", "RETIRE_IDENTITY",
            },
            set(self.props["operationType"]["enum"]),
        )

    def test_operation_is_retryable_without_secret_material(self):
        self.assertIn("FAILED", self.props["status"]["enum"])
        self.assertIn("IN_PROGRESS", self.props["status"]["enum"])
        forbidden = {"email", "password", "proof", "privateKey", "certificate", "token", "telemetry"}
        self.assertTrue(forbidden.isdisjoint(self.props))

    def test_transfer_and_replacement_identifiers_are_separate(self):
        for name in (
            "currentDeviceId", "replacementDeviceId", "currentThingName",
            "replacementThingName", "currentCertificateId",
            "replacementCertificateId", "sourceOwnerSub", "targetOwnerSub",
        ):
            self.assertIn(name, self.props)

    def test_cross_service_reconciliation_has_checkpoints(self):
        phases = set(self.props["phase"]["enum"])
        self.assertTrue(
            {
                "PROVISIONING_STARTED", "NEW_CREDENTIAL_RECORDED",
                "DOMAIN_STATE_COMMITTED", "EFFECTIVE_STATE_VERIFIED",
            } <= phases
        )
        self.assertEqual(
            "^[A-Fa-f0-9]{64}$",
            self.props["replacementCertificateId"]["pattern"],
        )

    def test_lifecycle_mutations_have_structured_audit_events(self):
        events = set(self.audit["properties"]["eventType"]["enum"])
        self.assertTrue(
            {
                "ADAPTER_REPLACEMENT_STARTED", "ADAPTER_REPLACED",
                "ADAPTER_REPORTED_LOST", "ADAPTER_RECOVERED",
                "IDENTITY_RETIRED", "LIFECYCLE_OPERATION_FAILED",
            } <= events
        )


if __name__ == "__main__":
    unittest.main()
