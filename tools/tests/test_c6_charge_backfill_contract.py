import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "firmware/esp32-c6/src/c6_charge_journal.cpp").read_text()
AWS = (ROOT / "firmware/esp32-c6/src/c6_aws.cpp").read_text()


class C6ChargeBackfillContractTests(unittest.TestCase):
    def test_charge_journal_is_bounded_persistent_and_acknowledged(self):
        self.assertIn('constexpr size_t MAX_RECORDS = 16', SOURCE)
        self.assertIn('/offline-cache/charges.bin', SOURCE)
        self.assertIn('charge/backfill/v1', SOURCE)
        self.assertIn('charge/backfill/ack/v1', SOURCE)
        self.assertIn('removeFirst()', SOURCE)

    def test_charge_record_contains_soc_odometer_energy_and_coverage(self):
        for field in (
            'startOdometerKm', 'endOdometerKm', 'startSoc', 'endSoc',
            'energyChargedWh', 'powerCoverageMs',
        ):
            self.assertIn(field, SOURCE)

    def test_aws_loop_routes_charge_backfill_and_ack(self):
        self.assertIn('c6ChargeBackfillHandleAwsMessage', AWS)
        self.assertIn('c6ChargeBackfillLoop', AWS)


if __name__ == '__main__':
    unittest.main()
