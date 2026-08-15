import unittest

from vcd_parser import (
    parse_vcd_file,
    assert_periods,
    Transition,
)


class Ch1SignalTest(unittest.TestCase):

    def test_ch1_has_100ms_high_low_transitions(self):
        transitions = parse_vcd_file(
            "gravity-ch1-test.vcd",
            "CH1",
        )

        self.assertGreaterEqual(
            len(transitions),
            6,
            "CH1 doit produire au moins 6 transitions",
        )

        # Chaque transition doit être espacée d'environ 100 ms.
        assert_periods(
            transitions[:7],
            expected_ns=100_000_000,
            tolerance_ns=1_000_000,
        )

    def test_ch1_alternates_high_low(self):
        transitions = parse_vcd_file(
            "gravity-ch1-test.vcd",
            "CH1",
        )

        values = [transition.value for transition in transitions[:10]]

        for previous, current in zip(values, values[1:]):
            self.assertNotEqual(
                previous,
                current,
                "CH1 doit alterner HIGH / LOW",
            )


if __name__ == "__main__":
    unittest.main()