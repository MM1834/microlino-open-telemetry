from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = (ROOT / "cloud/aws/onboarding/template.yaml").read_text(encoding="utf-8")
HANDLER = (ROOT / "cloud/aws/onboarding/src/handler.py").read_text(encoding="utf-8")


class OnboardingTemplateTests(unittest.TestCase):
    def test_template_is_separate_and_packaged(self):
        self.assertIn("ArtifactBucket:", TEMPLATE)
        self.assertIn("S3Bucket: !Ref ArtifactBucket", TEMPLATE)
        self.assertNotIn("ZipFile:", TEMPLATE)

    def test_tables_are_on_demand_encrypted_and_claims_use_ttl(self):
        self.assertEqual(3, TEMPLATE.count("Type: AWS::DynamoDB::Table"))
        self.assertEqual(3, TEMPLATE.count("BillingMode: PAY_PER_REQUEST"))
        self.assertGreaterEqual(TEMPLATE.count("SSEEnabled: true"), 3)
        self.assertIn("TimeToLiveSpecification: {AttributeName: ttl, Enabled: true}", TEMPLATE)
        self.assertIn("AuditRetentionDays:", TEMPLATE)
        audit_parameter = TEMPLATE.split("AuditRetentionDays:", 1)[1].split("LogRetentionDays:", 1)[0]
        self.assertNotIn("Default:", audit_parameter)

    def test_both_routes_require_jwt_and_api_is_throttled(self):
        self.assertIn('RouteKey: "POST /api/onboarding/claims"', TEMPLATE)
        self.assertIn('RouteKey: "POST /api/onboarding/claim"', TEMPLATE)
        self.assertEqual(2, TEMPLATE.count("AuthorizationType: JWT"))
        self.assertIn("ThrottlingRateLimit: 2", TEMPLATE)
        self.assertNotIn("AllowOrigins: ['*']", TEMPLATE)

    def test_admin_group_is_managed_but_has_no_automatic_membership(self):
        self.assertIn("Type: AWS::Cognito::UserPoolGroup", TEMPLATE)
        self.assertIn("CognitoUserPoolId:", TEMPLATE)
        self.assertNotIn("AWS::Cognito::UserPoolUserToGroupAttachment", TEMPLATE)

    def test_claim_consumption_is_one_atomic_transaction(self):
        compile(HANDLER, "onboarding-handler", "exec")
        self.assertIn("transact_write_items", HANDLER)
        self.assertIn('"TableName": OWNERSHIP_TABLE', HANDLER)
        self.assertIn('"TableName": ACCESS_TABLE', HANDLER)
        self.assertIn("secrets.compare_digest", HANDLER)
        self.assertNotIn("print(", HANDLER)


if __name__ == "__main__":
    unittest.main()
