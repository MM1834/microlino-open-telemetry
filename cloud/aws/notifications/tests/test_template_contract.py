import unittest
from pathlib import Path


TEMPLATE = (Path(__file__).resolve().parents[1] / "template.yaml").read_text()
PREFERENCE_API = (Path(__file__).resolve().parents[1] / "preference_api.py").read_text()
SMS_VERIFICATION_API = (Path(__file__).resolve().parents[1] / "sms_verification_api.py").read_text()
HANDLER = (Path(__file__).resolve().parents[1] / "handler.py").read_text()
FLEET = (Path(__file__).resolve().parents[1] / "fleet_efficiency.py").read_text()
EFFICIENCY_API = (Path(__file__).resolve().parents[1] / "efficiency_api.py").read_text()


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

    def test_fleet_efficiency_is_durable_stream_driven_and_least_privilege(self):
        self.assertIn("VehicleProfileTable:", TEMPLATE)
        self.assertIn("FleetEfficiencyTable:", TEMPLATE)
        self.assertIn("FleetJourneyMarkerTable:", TEMPLATE)
        self.assertIn("StreamSpecification: {StreamViewType: NEW_IMAGE}", TEMPLATE)
        self.assertIn("FleetEfficiencyEventSource:", TEMPLATE)
        self.assertIn("Handler: fleet_efficiency.handler", TEMPLATE)
        self.assertIn("PointInTimeRecoveryEnabled: true", TEMPLATE)
        fleet_role = TEMPLATE.split("  FleetEfficiencyRole:", 1)[1].split("  FleetEfficiencyFunction:", 1)[0]
        self.assertIn("dynamodb:GetItem", fleet_role)
        self.assertIn("dynamodb:PutItem, dynamodb:UpdateItem", fleet_role)
        self.assertNotIn("dynamodb:Scan", fleet_role)
        self.assertIn('ConditionExpression": "attribute_not_exists(markerId)"', FLEET)
        self.assertNotIn('"vehicleId": values', FLEET)

    def test_personal_community_comparison_is_separate_and_jwt_authorized(self):
        self.assertIn("UserEfficiencyTable:", TEMPLATE)
        self.assertIn("EfficiencyApiFunction:", TEMPLATE)
        self.assertIn('GET /api/vehicles/{vehicleId}/efficiency-comparison', TEMPLATE)
        self.assertIn("AuthorizationType: JWT", TEMPLATE)
        self.assertIn("FleetEfficiencyFinalizerSchedule:", TEMPLATE)
        self.assertIn('ScheduleExpression: "cron(15 0 1 * ? *)"', TEMPLATE)
        self.assertIn("averageNetKwhPer100Km", FLEET)
        self.assertNotIn("soc", EFFICIENCY_API.lower())
        self.assertNotIn("battery", EFFICIENCY_API.lower())

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

    def test_charging_summary_is_email_only_and_uses_standard_state(self):
        summary = HANDLER.split("def dispatch_charging_summary", 1)[1].split("def send_charging_summaries", 1)[0]
        self.assertIn("sns.publish", summary)
        self.assertNotIn("sms_delivery", summary)
        self.assertNotIn("debug", summary.lower())
        self.assertIn("energyCoveragePercent", summary)
        self.assertIn("socEstimatedEnergyKwh", summary)
        self.assertIn("VEHICLE_PROFILE_TABLE_NAME: !Ref VehicleProfileTable", TEMPLATE)
        self.assertIn("Resource: !GetAtt VehicleProfileTable.Arn", TEMPLATE)
        self.assertIn("chargingSummaryEmailEnabled", PREFERENCE_API)
        self.assertIn("CHARGING_SUMMARY_DELAY_SECONDS", HANDLER)

    def test_dashboard_charge_balance_is_separate_from_email_session(self):
        self.assertIn('item.get("chargingDisplay")', HANDLER)
        self.assertIn('"chargingDisplay": _ddb(asdict(display_after))', HANDLER)
        state_update = HANDLER.split("def update_charging_states", 1)[1].split(
            "def update_session", 1
        )[0]
        self.assertNotIn("send_charging_summaries", state_update)
        self.assertNotIn("sns.publish", state_update)
        self.assertIn("update_charging_states(", HANDLER)

    def test_drive_since_charge_totals_are_persisted_with_journey_finalization(self):
        self.assertIn('item.get("driveSinceCharge")', HANDLER)
        finalizer = HANDLER.split("def finalize_journey", 1)[1].split(
            "def finalize_due_journeys", 1
        )[0]
        self.assertIn("accumulate_drive_since_charge", finalizer)
        self.assertIn("_charge_reference_at(vehicle_id)", finalizer)
        self.assertIn("aggregate,", finalizer)

    def test_daily_summary_is_email_only_zurich_scheduled_and_idempotent(self):
        self.assertIn("DailySummarySchedule:", TEMPLATE)
        self.assertIn('ScheduleExpression: "cron(5 0-8 * * ? *)"', TEMPLATE)
        self.assertIn("ScheduleExpressionTimezone: Europe/Zurich", TEMPLATE)
        self.assertIn("scheduler.amazonaws.com", TEMPLATE)
        self.assertIn('event.get("type") == "daily_summary"', HANDLER)
        daily = HANDLER.split("def send_daily_summaries", 1)[1].split("def dispatch_journey", 1)[0]
        self.assertIn("sns.publish", daily)
        self.assertNotIn("sms_delivery", daily)
        self.assertIn("dailySummaryEmailEnabled", PREFERENCE_API)
        self.assertIn("attribute_not_exists(eventId)", HANDLER)

    def test_legacy_plain_counter_id_is_narrowly_accepted(self):
        self.assertIn("allow_plain_counter_id", HANDLER)
        self.assertIn('r"[A-Za-z0-9_-]{1,80}"', HANDLER)
        self.assertIn(
            'allow_plain_counter_id=(suffix == "journey/energy_counter_id")',
            HANDLER,
        )

    def test_stable_stop_is_finalized_before_resumed_movement(self):
        self.assertIn("resumed_after_stable_stop", HANDLER)
        self.assertIn("finalize_stable_stop=True", HANDLER)
        first = HANDLER.index("finalize_stable_stop=True")
        second = HANDLER.index(
            "update_journey(vehicle_id, suffix, value, received_at)", first
        )
        self.assertLess(first, second)

    def test_journey_state_isolated_from_charging_session_contention(self):
        self.assertIn('JOURNEY_SESSION_PREFIX = "journey#"', HANDLER)
        self.assertIn("JOURNEY_UPDATE_ATTEMPTS = 12", HANDLER)
        self.assertIn('"journeyVehicleId": vehicle_id', HANDLER)
        self.assertIn("_journey_retry_delay(attempt)", HANDLER)

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
