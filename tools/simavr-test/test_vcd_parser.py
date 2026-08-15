import unittest

from vcd_parser import (
    Transition,
    assert_alternates_high_low,
    assert_periods,
    assert_rising_edge_periods,
    parse_signal,
)


class VcdParserTest(unittest.TestCase):

    def test_parses_signal_transitions(self):
        vcd = """\
$var wire 1 ! CH1 $end
$enddefinitions $end
$dumpvars
x!
$end
#937
1!
#100001062
0!
#200001312
1!
"""

        result = parse_signal(vcd, "CH1")

        self.assertEqual(
            result,
            [
                Transition(937, 1),
                Transition(100001062, 0),
                Transition(200001312, 1),
            ],
        )

    def test_signal_not_found(self):
        vcd = """\
$var wire 1 ! CH1 $end
$enddefinitions $end
"""

        with self.assertRaises(ValueError):
            parse_signal(vcd, "CH2")

    def test_ignores_initial_unknown_value(self):
        vcd = """\
$var wire 1 ! CH1 $end
$enddefinitions $end
$dumpvars
x!
$end
#100
1!
"""

        result = parse_signal(vcd, "CH1")

        self.assertEqual(
            result,
            [Transition(100, 1)],
        )


class PeriodAssertionTest(unittest.TestCase):

    def test_accepts_period_within_tolerance(self):
        transitions = [
            Transition(0, 1),
            Transition(100_000_000, 0),
            Transition(200_000_000, 1),
        ]

        assert_periods(
            transitions,
            expected_ns=100_000_000,
            tolerance_ns=1_000_000,
        )

    def test_rejects_period_outside_tolerance(self):
        transitions = [
            Transition(0, 1),
            Transition(100_000_000, 0),
            Transition(202_000_000, 1),
        ]

        with self.assertRaises(AssertionError):
            assert_periods(
                transitions,
                expected_ns=100_000_000,
                tolerance_ns=1_000_000,
            )

    def test_requires_at_least_three_transitions(self):
        transitions = [
            Transition(0, 1),
            Transition(100_000_000, 0),
        ]

        with self.assertRaises(AssertionError):
            assert_periods(
                transitions,
                expected_ns=100_000_000,
                tolerance_ns=1_000_000,
            )

class SignalAssertionTest(unittest.TestCase):

    def test_accepts_alternating_signal(self):
        transitions = [
            Transition(0, 1),
            Transition(100, 0),
            Transition(200, 1),
            Transition(300, 0),
        ]

        assert_alternates_high_low(transitions)

    def test_rejects_non_alternating_signal(self):
        transitions = [
            Transition(0, 1),
            Transition(100, 1),
        ]

        with self.assertRaises(AssertionError):
            assert_alternates_high_low(transitions)

    def test_accepts_rising_edge_period(self):
        transitions = [
            Transition(0, 1),
            Transition(5, 0),
            Transition(100, 1),
            Transition(105, 0),
            Transition(200, 1),
        ]

        assert_rising_edge_periods(
            transitions,
            expected_ns=100,
            tolerance_ns=1,
        )


if __name__ == "__main__":
    unittest.main()