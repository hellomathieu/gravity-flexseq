import unittest
from pathlib import Path

from vcd_parser import (
    assert_alternates_high_low,
    assert_rising_edge_periods,
    parse_vcd_file,
)


# Le VCD de CE harnais, et non celui du firmware.
#
# run_signal_test.sh simulait main.c puis lancait les assertions de
# test_ch1_signal.py, qui lit gravity-flexseq-ch1.vcd -- le VCD produit par
# l'AUTRE script. Sa propre simulation n'etait donc verifiee par rien : il
# passait au vert sur un artefact perime, et aurait echoue sur un depot frais ou
# ce fichier n'existe pas encore. Chaque harnais assertionne desormais ce qu'il
# a lui-meme produit.
VCD_FILE = Path(__file__).with_name("gravity-ch1-test.vcd")

# main.c : creneau nu sur PORTD7 (= Gravity CH1), _delay_ms(100) de chaque cote.
HALF_PERIOD_NS = 100_000_000
PERIOD_NS = 2 * HALF_PERIOD_NS
# La gigue mesuree est inferieure a la microseconde (simavr compte les cycles) ;
# 1 ms laisse de la marge sans rendre l'assertion complaisante.
TOLERANCE_NS = 1_000_000


class SquareWaveTest(unittest.TestCase):
    """Chaine verifiee : avr-gcc -> .hex -> simavr -> VCD -> assertions.

    Ce harnais ne teste AUCUN code FlexSeq : il verifie que la chaine d'outils
    elle-meme est fidele, sur un signal dont la periode est connue d'avance.
    C'est ce qui permet de croire ensuite le VCD du vrai firmware
    (test_ch1_signal.py).
    """

    @classmethod
    def setUpClass(cls):
        if not VCD_FILE.exists():
            raise AssertionError(f"VCD file not found: {VCD_FILE}")

        cls.transitions = parse_vcd_file(VCD_FILE, "CH1")

    def test_ch1_has_transitions(self):
        self.assertGreaterEqual(
            len(self.transitions),
            3,
            "CH1 must contain at least three transitions",
        )

    def test_ch1_alternates_high_low(self):
        assert_alternates_high_low(self.transitions)

    def test_ch1_has_200ms_rising_edge_period(self):
        # 100 ms haut + 100 ms bas : un front montant tous les 200 ms.
        assert_rising_edge_periods(
            self.transitions,
            expected_ns=PERIOD_NS,
            tolerance_ns=TOLERANCE_NS,
        )

    def test_ch1_stays_high_for_100ms(self):
        # La periode seule ne dirait rien du rapport cyclique : deux delais
        # devenus asymetriques la laisseraient intacte.
        highs = [
            self.transitions[i + 1].time_ns - self.transitions[i].time_ns
            for i in range(len(self.transitions) - 1)
            if self.transitions[i].value == 1
        ]

        self.assertTrue(highs, "no complete HIGH interval in the VCD")

        for duration in highs:
            self.assertAlmostEqual(
                duration,
                HALF_PERIOD_NS,
                delta=TOLERANCE_NS,
                msg=f"HIGH lasted {duration} ns, expected {HALF_PERIOD_NS} ns",
            )


if __name__ == "__main__":
    unittest.main()
