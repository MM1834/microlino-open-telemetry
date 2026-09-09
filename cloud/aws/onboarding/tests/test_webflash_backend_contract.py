from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[4]
TEMPLATE = (ROOT / "cloud/aws/onboarding/template.yaml").read_text(encoding="utf-8")
HANDLER = (ROOT / "cloud/aws/onboarding/src/handler.py").read_text(encoding="utf-8")


class WebflashBackendContractTests(unittest.TestCase):
    def test_presigned_download_uses_regional_s3_endpoint_without_browser_redirect(self):
        self.assertIn('AWS_REGION = os.environ.get("AWS_REGION", "eu-north-1")', HANDLER)
        self.assertIn('endpoint_url=f"https://s3.{AWS_REGION}.amazonaws.com"', HANDLER)

    def test_artifact_bucket_is_private_encrypted_versioned_and_retained(self):
        section = TEMPLATE.split("  FirmwareArtifactBucket:", 1)[1].split("  OnboardingRole:", 1)[0]
        self.assertIn("DeletionPolicy: Retain", section)
        self.assertIn("UpdateReplacePolicy: Retain", section)
        self.assertIn("BlockPublicAcls: true", section)
        self.assertIn("RestrictPublicBuckets: true", section)
        self.assertIn("SSEAlgorithm: AES256", section)
        self.assertIn("VersioningConfiguration: {Status: Enabled}", section)
        self.assertIn("NoncurrentDays: 30", section)

    def test_grants_are_ttl_encrypted_and_exact_to_user_and_target(self):
        section = TEMPLATE.split("  FirmwareGrantsTable:", 1)[1].split("  FirmwareArtifactBucket:", 1)[0]
        self.assertIn("BillingMode: PAY_PER_REQUEST", section)
        self.assertIn("AttributeName: userSub", section)
        self.assertIn("AttributeName: target", section)
        self.assertIn("TimeToLiveSpecification: {AttributeName: ttl, Enabled: true}", section)
        self.assertIn("SSESpecification: {SSEEnabled: true}", section)

    def test_every_webflash_route_requires_the_jwt_authorizer(self):
        for route in (
            "POST /api/firmware/grants",
            "POST /api/firmware/grants/revoke",
            "GET /api/firmware/access",
            "POST /api/firmware/download",
            "POST /api/firmware/result",
        ):
            route_section = TEMPLATE.split(f'RouteKey: "{route}"', 1)[1].split("  ", 1)[0]
            self.assertIn(route, TEMPLATE)
        self.assertGreaterEqual(TEMPLATE.count("AuthorizationType: JWT"), 7)

    def test_every_password_recovery_route_requires_the_jwt_authorizer(self):
        for route in (
            "POST /api/password-recovery/grants",
            "POST /api/password-recovery/grants/revoke",
            "GET /api/password-recovery/access",
            "POST /api/password-recovery/start",
            "POST /api/password-recovery/result",
        ):
            self.assertIn(f'RouteKey: "{route}"', TEMPLATE)
        self.assertGreaterEqual(TEMPLATE.count("AuthorizationType: JWT"), 12)

    def test_password_recovery_is_a_separate_expiring_action_without_secret_audit(self):
        self.assertIn('PASSWORD_RECOVERY_TARGET = "local-admin-password"', HANDLER)
        self.assertIn('"PASSWORD_RECOVERY_GRANT_CREATED"', HANDLER)
        self.assertIn('"PASSWORD_RECOVERY_AUTHORIZED"', HANDLER)
        self.assertNotIn('body.get("password"', HANDLER)

    def test_download_is_short_lived_and_release_is_supported_c6_application_only(self):
        self.assertIn("MaxValue: 300", TEMPLATE)
        self.assertIn('AllowedValues: [nanoesp32c6-n16, xiao-esp32c6]', TEMPLATE)
        self.assertIn('FIRMWARE_TARGET: !Ref FirmwareTarget', TEMPLATE)
        self.assertIn('FIRMWARE_SECONDARY_TARGET: !Ref FirmwareSecondaryTarget', TEMPLATE)
        self.assertIn('${FirmwareArtifactBucket.Arn}/${FirmwareSecondaryArtifactKey}', TEMPLATE)
        self.assertIn('"chipFamily": "ESP32-C6"', HANDLER)
        self.assertIn('"flashSizeBytes": flash_size', HANDLER)
        self.assertIn('"offset": 0x10000', HANDLER)
        self.assertIn('"factoryErase": False', HANDLER)
        self.assertIn("generate_presigned_url", HANDLER)

    def test_admin_resolution_is_server_side_and_group_restricted(self):
        self.assertIn("cognito-idp:AdminGetUser", TEMPLATE)
        self.assertIn("cognito-idp:ListUsers", TEMPLATE)
        self.assertIn("if ADMIN_GROUP not in _groups(claims)", HANDLER)
        self.assertIn("cognito_client.admin_get_user", HANDLER)

    def test_active_grant_inventory_is_jwt_and_admin_protected(self):
        self.assertIn('RouteKey: "GET /api/admin/grants"', TEMPLATE)
        self.assertIn('if route == "GET /api/admin/grants"', HANDLER)
        self.assertIn("return active_grants(claims)", HANDLER)
        self.assertIn("grant_list_too_large", HANDLER)


if __name__ == "__main__":
    unittest.main()
