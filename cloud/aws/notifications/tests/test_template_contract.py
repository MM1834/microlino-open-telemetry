import unittest
from pathlib import Path


TEMPLATE = (Path(__file__).resolve().parents[1] / "template.yaml").read_text()
PREFERENCE_API = (Path(__file__).resolve().parents[1] / "preference_api.py").read_text()


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
        self.assertIn("Action: [sns:Subscribe, sns:GetSubscriptionAttributes]", TEMPLATE)
        self.assertIn("dynamodb:GetItem, dynamodb:PutItem, dynamodb:UpdateItem", TEMPLATE)

    def test_email_topic_has_a_human_readable_display_name(self):
        self.assertIn("DisplayName: Microlino Open Telemetry", TEMPLATE)

    def test_journey_email_opt_in_is_additive_and_defaults_off(self):
        self.assertIn('"journeyEmailEnabled": False', PREFERENCE_API)
        self.assertIn('"journeyEmailEnabled", previous.get("journeyEmailEnabled", False)', PREFERENCE_API)
        self.assertIn('"journey_email_requires_email"', PREFERENCE_API)

    def test_journey_finalizer_is_bounded_and_can_scan_session_state(self):
        self.assertIn("JourneyFinalizerRule:", TEMPLATE)
        self.assertIn("ScheduleExpression: rate(5 minutes)", TEMPLATE)
        self.assertIn("dynamodb:GetItem, dynamodb:PutItem, dynamodb:Scan", TEMPLATE)


if __name__ == "__main__":
    unittest.main()
