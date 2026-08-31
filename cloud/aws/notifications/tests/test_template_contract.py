import unittest
from pathlib import Path


TEMPLATE = (Path(__file__).resolve().parents[1] / "template.yaml").read_text()
PREFERENCE_API = (Path(__file__).resolve().parents[1] / "preference_api.py").read_text()
SMS_VERIFICATION_API = (Path(__file__).resolve().parents[1] / "sms_verification_api.py").read_text()
HANDLER = (Path(__file__).resolve().parents[1] / "handler.py").read_text()


class TemplateContractTests(unittest.TestCase):
    def test_read_only_vehicle_ids_reach_preference_api(self):
        self.assertIn("ReadOnlyVehicleIds:", TEMPLATE)
        self.assertIn("READ_ONLY_VEHICLE_IDS: !Ref ReadOnlyVehicleIds", TEMPLATE)
        self.assertIn("notifications_read_only", PREFERENCE_API)

    def test_notification_stack_is_additive_and_authorized(self):
        self.assertIn("PreferenceTable:", TEMPLATE)
        self.assertIn("SessionTable:", TEMPLATE)
        self.assertIn("EventTable:", TEMPLATE)
        self.assertIn("AuthorizationType: JWT", TEMPLATE)
        self.assertIn("AccessTableArn", TEMPLATE)

    def test_rule_only_consumes_four_level_mot_topics(self):
        self.assertIn("FROM 'mot/+/+/+'", TEMPLATE)

    def test_rule_filters_before_invoking_notification_lambda(self):
        rule = TEMPLATE.split("  NotificationRule:", 1)[1].split("  JourneyFinalizerRule:", 1)[0]
        self.assertIn("WHERE", rule)
        self.assertIn("topic(3) = 'charging'", rule)
        self.assertIn("topic(4) IN ['plugged', 'is_charging', 'power_signed']", rule)
        self.assertIn("topic(3) = 'display'", rule)
        self.assertIn("topic(4) IN ['soc', 'odometer_km', 'speed_kmh']", rule)
        self.assertIn("topic(3) = 'journey'", rule)

    def test_sms_publish_and_email_subscribe_are_separate_permissions(self):
        self.assertIn("Action: [sns:Publish]", TEMPLATE)
        self.assertIn("Resource: !Ref EmailTopic", TEMPLATE)
        self.assertNotIn('Resource: [!Ref EmailTopic, "*"]', TEMPLATE)
        self.assertIn("Action: [sns:Subscribe, sns:GetSubscriptionAttributes]", TEMPLATE)
        self.assertIn("dynamodb:GetItem, dynamodb:PutItem, dynamodb:UpdateItem", TEMPLATE)

    def test_sms_dispatcher_has_all_fail_closed_gates(self):
        self.assertIn("SmsDeliveryEnabled:", TEMPLATE)
        self.assertIn("SmsRateTable:", TEMPLATE)
        self.assertIn("SMS_DELIVERY_ENABLED: !Ref SmsDeliveryEnabled", TEMPLATE)
        self.assertIn("sms-voice:DescribeSpendLimits", TEMPLATE)
        self.assertIn("cloudwatch:DescribeAlarms", TEMPLATE)
        for gate in (
            "KILL_SWITCH", "DESTINATION", "MESSAGE_FORMAT", "APPROVAL",
            "VERIFICATION", "SPEND_ALARM", "SPEND_LIMIT", "RATE_LIMIT",
        ):
            self.assertIn(f'"{gate}"', HANDLER)
        self.assertIn("sms_voice.send_text_message", HANDLER)
        self.assertNotIn("PhoneNumber=preference", HANDLER)

    def test_journey_dispatch_remains_email_only(self):
        journey = HANDLER.split("def dispatch_journey", 1)[1].split("def finalize_journey", 1)[0]
        self.assertNotIn("sms_delivery", journey)
        self.assertNotIn("send_text_message", journey)

    def test_sms_admin_approval_is_separate_from_user_preference_api(self):
        self.assertIn("SmsApprovalTable:", TEMPLATE)
        self.assertIn("SmsApprovalAuditTable:", TEMPLATE)
        self.assertIn("SmsApprovalAdminRole:", TEMPLATE)
        self.assertIn("Condition: HasSmsAdminPrincipal", TEMPLATE)
        self.assertIn("SmsApprovalTableName:", TEMPLATE)
        self.assertNotIn("SMS_APPROVAL_TABLE", PREFERENCE_API)
        self.assertNotIn("smsApproved", PREFERENCE_API)

    def test_sms_approval_and_audit_are_encrypted_bounded_tables(self):
        self.assertIn("${ProjectName}-${Environment}-sms-approvals", TEMPLATE)
        self.assertIn("${ProjectName}-${Environment}-sms-approval-audit", TEMPLATE)
        self.assertGreaterEqual(TEMPLATE.count("TimeToLiveSpecification: {AttributeName: expiresAt, Enabled: true}"), 3)
        self.assertIn("SmsApprovalAuditRetentionDays", TEMPLATE)

    def test_sms_verification_is_separate_and_ch_de_only(self):
        self.assertIn("SmsDestinationTable:", TEMPLATE)
        self.assertIn("SmsVerificationRole:", TEMPLATE)
        self.assertIn("SmsVerificationFunction:", TEMPLATE)
        self.assertIn('GET /api/vehicles/{vehicleId}/notifications/sms', TEMPLATE)
        self.assertIn('POST /api/vehicles/{vehicleId}/notifications/sms/request', TEMPLATE)
        self.assertIn('POST /api/vehicles/{vehicleId}/notifications/sms/confirm', TEMPLATE)
        self.assertIn("Action: [sms-voice:SendTextMessage]", TEMPLATE)
        self.assertIn("sender-id/MOT/CH", TEMPLATE)
        self.assertIn("SmsVerifiedDestinationNumberArn", TEMPLATE)
        self.assertIn("configuration-set/mot-dev-sms", TEMPLATE)
        self.assertIn('PHONE = re.compile(r"^\\+(41|49)', SMS_VERIFICATION_API)
        verification_role = TEMPLATE.split("  SmsVerificationRole:", 1)[1].split("  SmsVerificationFunction:", 1)[0]
        self.assertNotIn("SmsApprovalTable.Arn\n                  - !GetAtt PreferenceTable", verification_role)
        self.assertNotIn("SmsApprovalTable.Arn\n                  - !GetAtt SmsDestinationTable", verification_role)

    def test_verified_destination_can_be_reused_without_plaintext_registry(self):
        self.assertIn('destination.get("status") == "VERIFIED"', SMS_VERIFICATION_API)
        self.assertIn('"destinationFingerprint": destination_fingerprint', SMS_VERIFICATION_API)
        self.assertNotIn('"phoneE164": phone,\n            "verifiedDestinationNumberId"', SMS_VERIFICATION_API)

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

    def test_charging_stop_uses_durable_delayed_queue(self):
        self.assertIn("ChargingStopQueue:", TEMPLATE)
        self.assertIn("ChargingStopDeadLetterQueue:", TEMPLATE)
        self.assertIn("ChargingStopEventSource:", TEMPLATE)
        self.assertIn("CHARGING_STOP_QUEUE_URL: !Ref ChargingStopQueue", TEMPLATE)
        self.assertIn("sqs:SendMessage", TEMPLATE)

    def test_charging_stop_preference_is_separate_and_defaults_off(self):
        self.assertIn('"chargingStopEmailEnabled": False', PREFERENCE_API)
        self.assertIn('"chargingStopThreshold": 80', PREFERENCE_API)
        self.assertIn('"charging_stop_email_requires_email"', PREFERENCE_API)

    def test_charging_stop_delivery_is_bounded_to_one_per_session(self):
        state_source = (
            Path(__file__).resolve().parents[1] / "notification_state.py"
        ).read_text()
        self.assertIn("CHARGING_START_QUALIFICATION_MS = 45 * 1000", state_source)
        self.assertIn("f'{state.session_id}'", HANDLER)
        self.assertNotIn(
            "f'{state.session_id}|{state.stop_candidate_at}|{threshold}'", HANDLER
        )


if __name__ == "__main__":
    unittest.main()
