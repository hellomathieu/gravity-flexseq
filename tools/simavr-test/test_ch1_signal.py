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

    def test_ch1_has_40ms_rising_edge_period(self):
        # Engine-driven: pattern 0 has active steps 0/4/8/12 played on CH1 at
        # one 1/16 step per 10 ms, so CH1 pulses once every 4 steps = 40 ms.
        assert_rising_edge_periods(
            self.transitions,
            expected_ns=40_000_000,
            tolerance_ns=3_000_000,
        )


if __name__ == "__main__":
    unittest.main()