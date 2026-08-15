import unittest
from pathlib import Path

from vcd_parser import (
    assert_alternates_high_low,
    assert_rising_edge_periods,
    parse_vcd_file,
)


VCD_FILE = Path(__file__).with_name("gravity-flexseq-ch1.vcd")


class Ch1SignalTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        if not VCD_FILE.exists():
            raise AssertionError(
                f"VCD file not found: {VCD_FILE}"
            )

        cls.transitions = parse_vcd_file(
            VCD_FILE,
            "CH1",
        )

    def test_ch1_has_transitions(self):
        self.assertGreaterEqual(
            len(self.transitions),
            3,
            "CH1 must contain at least three transitions",
        )

    def test_ch1_alternates_high_low(self):
        assert_alternates_high_low(self.transitions)

    def test_ch1_has_100ms_rising_edge_period(self):
        assert_rising_edge_periods(
            self.transitions,
            expected_ns=100_000_000,
            tolerance_ns=2_000_000,
        )


if __name__ == "__main__":
    unittest.main()