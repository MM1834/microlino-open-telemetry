from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = (ROOT / "cloud/aws/foundation/template.yaml").read_text()


class CognitoSesEmailContractTests(unittest.TestCase):
    def test_ses_switch_defaults_fail_safe(self):
        self.assertIn("CognitoEmailSendingAccount:", TEMPLATE)
        self.assertIn("Default: COGNITO_DEFAULT", TEMPLATE)
        self.assertIn("AllowedValues: [COGNITO_DEFAULT, DEVELOPER]", TEMPLATE)
        self.assertIn("CognitoUsesSes: !Equals", TEMPLATE)

    def test_domain_identity_uses_easy_dkim_and_tags(self):
        self.assertIn("CognitoEmailIdentity:", TEMPLATE)
        self.assertIn("Type: AWS::SES::EmailIdentity", TEMPLATE)
        self.assertIn("NextSigningKeyLength: RSA_2048_BIT", TEMPLATE)
        for suffix in ("Name1", "Value1", "Name2", "Value2", "Name3", "Value3"):
            self.assertIn(f"CognitoEmailDkim{suffix}:", TEMPLATE)

    def test_developer_mode_has_aligned_sender_and_reply_to(self):
        self.assertIn("Default: MOT Portal <support@microlino-open-telemetry.ch>", TEMPLATE)
        self.assertIn("Default: support@microlino-open-telemetry.ch", TEMPLATE)
        self.assertIn("EmailSendingAccount: DEVELOPER", TEMPLATE)
        self.assertIn("SourceArn: !Sub", TEMPLATE)
        self.assertIn("- !Ref AWS::NoValue", TEMPLATE)


if __name__ == "__main__":
    unittest.main()
