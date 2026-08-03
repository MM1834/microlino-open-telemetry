import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCHEMAS = ROOT / "cloud/aws/onboarding/schemas"


class OnboardingClaimSchemaTests(unittest.TestCase):
    def setUp(self):
        self.claim = json.loads((SCHEMAS / "claim-record.schema.json").read_text())
        self.ownership = json.loads(
            (SCHEMAS / "vehicle-ownership-record.schema.json").read_text()
        )
        self.audit = json.loads((SCHEMAS / "audit-event.schema.json").read_text())

    def test_all_schemas_are_closed_and_versioned(self):
        for schema in (self.claim, self.ownership, self.audit):
            self.assertEqual("https://json-schema.org/draft/2020-12/schema", schema["$schema"])
            self.assertFalse(schema["additionalProperties"])
            self.assertEqual(1, schema["properties"]["schemaVersion"]["const"])

    def test_claim_never_contains_plaintext_proof_or_email(self):
        properties = set(self.claim["properties"])
        self.assertNotIn("proof", properties)
        self.assertNotIn("email", properties)
        self.assertIn("proofHash", properties)
        self.assertIn("proofSalt", properties)

    def test_claim_is_expiring_attempt_limited_and_single_use(self):
        required = set(self.claim["required"])
        self.assertTrue({"expiresAt", "failedAttempts", "maxAttempts", "status"} <= required)
        statuses = set(self.claim["properties"]["status"]["enum"])
        self.assertEqual({"ISSUED", "CONSUMED", "REVOKED", "EXPIRED"}, statuses)
        self.assertLessEqual(self.claim["properties"]["maxAttempts"]["maximum"], 10)

    def test_hash_format_is_exact_sha256_base64url(self):
        pattern = self.claim["properties"]["proofHash"]["pattern"]
        digest = "A" * 43
        self.assertRegex(f"sha256:{digest}", re.compile(pattern))
        self.assertNotRegex("sha256:short", re.compile(pattern))

    def test_ownership_has_one_canonical_vehicle_key_and_version(self):
        required = set(self.ownership["required"])
        self.assertTrue({"vehicleId", "ownerUserSub", "version", "sourceClaimId"} <= required)
        self.assertNotIn("email", self.ownership["properties"])

    def test_audit_is_structured_without_free_text_or_credentials(self):
        properties = set(self.audit["properties"])
        for forbidden in ("email", "message", "details", "proof", "token", "certificate"):
            self.assertNotIn(forbidden, properties)
        self.assertIn("reasonCode", properties)
        self.assertIn("eventType", properties)


if __name__ == "__main__":
    unittest.main()
