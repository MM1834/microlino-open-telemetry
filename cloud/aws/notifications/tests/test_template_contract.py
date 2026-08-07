import unittest
from pathlib import Path


TEMPLATE = (Path(__file__).resolve().parents[1] / "template.yaml").read_text()


class TemplateContractTests(unittest.TestCase):
    def test_notification_stack_is_additive_and_authorized(self):
        self.assertIn("PreferenceTable:", TEMPLATE)
        self.assertIn("SessionTable:", TEMPLATE)
        self.assertIn("EventTable:", TEMPLATE)
        self.assertIn("AuthorizationType: JWT", TEMPLATE)
        self.assertIn("AccessTableArn", TEMPLATE)

    def test_rule_only_consumes_four_level_mot_topics(self):
        self.assertIn("FROM 'mot/+/+/+'", TEMPLATE)

    def test_sms_publish_and_email_subscribe_are_separate_permissions(self):
        self.assertIn("Action: [sns:Publish]", TEMPLATE)
        self.assertIn("Action: [sns:Subscribe]", TEMPLATE)


if __name__ == "__main__":
    unittest.main()
