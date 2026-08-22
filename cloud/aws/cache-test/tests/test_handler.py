import base64
import importlib.util
import json
import os
import sys
import types
import unittest
from decimal import Decimal
from pathlib import Path
from unittest.mock import patch


class ConditionalFailure(Exception):
    def __init__(self):
        self.response = {"Error": {"Code": "ConditionalCheckFailedException"}}


class FakeTable:
    def __init__(self):
        self.items = {}

    def put_item(self, Item, ConditionExpression):
        key = (Item["vehicleId"], Item["sampleKey"])
        if key in self.items:
            raise ConditionalFailure()
        self.items[key] = Item
        return {}


class FakeIotData:
    def __init__(self):
        self.publishes = []

    def publish(self, **kwargs):
        self.publishes.append(kwargs)


def encoded(value):
    return base64.b64encode(json.dumps(value).encode()).decode()


class BackfillHandlerTests(unittest.TestCase):
    def setUp(self):
        self.table = FakeTable()
        self.iot = FakeIotData()
        fake_boto3 = types.ModuleType("boto3")
        fake_boto3.resource = lambda service: types.SimpleNamespace(
            Table=lambda name: self.table
        )
        fake_boto3.client = lambda service: self.iot

        fake_botocore = types.ModuleType("botocore")
        fake_exceptions = types.ModuleType("botocore.exceptions")
        fake_exceptions.ClientError = ConditionalFailure
        fake_botocore.exceptions = fake_exceptions

        self.module_patches = patch.dict(
            sys.modules,
            {
                "boto3": fake_boto3,
                "botocore": fake_botocore,
                "botocore.exceptions": fake_exceptions,
            },
        )
        self.module_patches.start()
        self.env_patch = patch.dict(
            os.environ,
            {
                "TOPIC_ROOT": "mot-test",
                "TEST_VEHICLE_ID": "cache-b025-n16-01",
                "HISTORY_TABLE_NAME": "test-history",
                "RETENTION_DAYS": "3",
                "MAX_BATCH_SAMPLES": "60",
                "MAX_SAMPLE_AGE_SECONDS": "259200",
            },
            clear=False,
        )
        self.env_patch.start()
        path = Path(__file__).parents[1] / "handler.py"
        spec = importlib.util.spec_from_file_location("cache_test_handler", path)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.now_ms = 1787212800000

    def tearDown(self):
        self.env_patch.stop()
        self.module_patches.stop()

    def event(self, envelope, topic="mot-test/cache-b025-n16-01/history/backfill/v1"):
        return {
            "mqttTopic": topic,
            "receivedAt": self.now_ms,
            "payloadBase64": encoded(envelope),
        }

    def envelope(self, samples=None, batch_id="boot1-0001"):
        return {
            "version": 1,
            "vehicleId": "cache-b025-n16-01",
            "batchId": batch_id,
            "samples": samples or [
                {"signal": "soc", "sampledAt": self.now_ms - 60000, "value": 71.5},
                {"signal": "speed", "sampledAt": self.now_ms - 30000, "value": 32.0},
            ],
        }

    def test_valid_batch_writes_history_and_acknowledges(self):
        result = self.module.handler(self.event(self.envelope()), None)
        self.assertTrue(result["accepted"])
        self.assertEqual(2, result["stored"])
        self.assertEqual(2, len(self.table.items))
        self.assertEqual(
            "mot-test/cache-b025-n16-01/history/backfill/ack/v1",
            self.iot.publishes[0]["topic"],
        )
        values = [item["value"] for item in self.table.items.values()]
        self.assertEqual([Decimal("71.5"), Decimal("32.0")], values)

    def test_duplicate_batch_is_idempotent(self):
        event = self.event(self.envelope())
        self.module.handler(event, None)
        result = self.module.handler(event, None)
        self.assertEqual(0, result["stored"])
        self.assertEqual(2, result["duplicates"])
        self.assertEqual(2, len(self.table.items))

    def test_operational_topic_is_rejected_without_ack(self):
        result = self.module.handler(
            self.event(self.envelope(), "mot/cache-b025-n16-01/history/backfill/v1"), None
        )
        self.assertFalse(result["accepted"])
        self.assertEqual("invalid_topic", result["reason"])
        self.assertEqual([], self.iot.publishes)

    def test_other_test_vehicle_is_rejected_without_ack(self):
        result = self.module.handler(
            self.event(self.envelope(), "mot-test/other/history/backfill/v1"), None
        )
        self.assertFalse(result["accepted"])
        self.assertEqual("vehicle_not_allowed", result["reason"])
        self.assertEqual([], self.iot.publishes)

    def test_invalid_signal_is_rejected_and_acknowledged(self):
        envelope = self.envelope([
            {"signal": "latitude", "sampledAt": self.now_ms - 1000, "value": 47.0}
        ])
        result = self.module.handler(self.event(envelope), None)
        self.assertFalse(result["accepted"])
        self.assertEqual("invalid_signal", result["reason"])
        self.assertEqual(1, len(self.iot.publishes))
        self.assertEqual(0, len(self.table.items))
        ack = json.loads(self.iot.publishes[0]["payload"])
        self.assertEqual("boot1-0001", ack["batchId"])

    def test_terminal_speed_zero_is_kept_inside_active_minute(self):
        samples = [
            {"signal": "speed", "sampledAt": self.now_ms - 50000, "value": 25},
            {"signal": "speed", "sampledAt": self.now_ms - 30000, "value": 0},
        ]
        result = self.module.handler(self.event(self.envelope(samples)), None)
        self.assertTrue(result["accepted"])
        self.assertEqual(2, result["stored"])
        self.assertEqual(2, len(self.table.items))

    def test_old_future_and_unordered_samples_are_rejected(self):
        cases = [
            ([{"signal": "soc", "sampledAt": self.now_ms - 259201000, "value": 50}],
             "sample_timestamp_out_of_range"),
            ([{"signal": "soc", "sampledAt": self.now_ms + 61000, "value": 50}],
             "sample_timestamp_out_of_range"),
            ([
                {"signal": "speed", "sampledAt": self.now_ms - 1000, "value": 1},
                {"signal": "soc", "sampledAt": self.now_ms - 2000, "value": 50},
            ], "samples_not_ordered"),
        ]
        for samples, reason in cases:
            with self.subTest(reason=reason):
                result = self.module.handler(self.event(self.envelope(samples)), None)
                self.assertFalse(result["accepted"])
                self.assertEqual(reason, result["reason"])

    def test_value_ranges_and_exact_sample_shape_are_enforced(self):
        cases = [
            {"signal": "soc", "sampledAt": self.now_ms - 1000, "value": 101},
            {"signal": "speed", "sampledAt": self.now_ms - 1000, "value": -1},
            {
                "signal": "soc",
                "sampledAt": self.now_ms - 1000,
                "value": 50,
                "latitude": 47,
            },
        ]
        for sample in cases:
            with self.subTest(sample=sample):
                result = self.module.handler(
                    self.event(self.envelope([sample], batch_id="bad")), None
                )
                self.assertFalse(result["accepted"])
        self.assertEqual(0, len(self.table.items))


if __name__ == "__main__":
    unittest.main()
