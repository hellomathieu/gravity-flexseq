import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

from vcd_parser import (
    assert_alternates_high_low,
    assert_rising_edge_periods,
    parse_vcd_file,
)


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE = PROJECT_ROOT / ".pio" / "build" / "simavr" / "firmware.elf"
VCD_FILE = Path(__file__).resolve().parent / "gravity-flexseq-ch1.vcd"

SIMAVR_FREQUENCY_HZ = 16_000_000

# CH1 = Arduino D7 = ATmega328P PD7.
CH1_TRACE = "CH1=portpin@0x07/0x44"

# Current firmware test configuration:
# CH1 rising edge every 100 ms.
EXPECTED_RISING_EDGE_PERIOD_NS = 100_000_000
RISING_EDGE_TOLERANCE_NS = 1_000_000

# Host-side simulation duration.
SIMULATION_DURATION_SECONDS = 0.2


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    run_avr = shutil.which("run_avr")

    if run_avr is None:
        fail(
            "run_avr was not found in PATH. "
            "Build simavr and make its run_avr executable available "
            "through PATH."
        )

    if not FIRMWARE.is_file():
        fail(
            f"Firmware not found: {FIRMWARE}\n"
            "Build it first with: pio run -e simavr"
        )

    VCD_FILE.unlink(missing_ok=True)

    command = [
        run_avr,
        "-m",
        "atmega328p",
        "-f",
        str(SIMAVR_FREQUENCY_HZ),
        "-o",
        str(VCD_FILE),
        "--add-trace",
        CH1_TRACE,
        str(FIRMWARE),
    ]

    print("Running SimAVR...")
    print(f"Firmware: {FIRMWARE}")
    print(f"VCD:      {VCD_FILE}")

    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    try:
        time.sleep(SIMULATION_DURATION_SECONDS)

        # Ask SimAVR to terminate cleanly so it can flush the VCD.
        process.send_signal(signal.SIGTERM)

        stdout, stderr = process.communicate(timeout=2)

    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate()

        fail(
            "SimAVR did not terminate after receiving SIGTERM.\n"
            f"stdout:\n{stdout}\n"
            f"stderr:\n{stderr}"
        )

    if stdout:
        print(stdout, end="")

    if stderr:
        print(stderr, end="", file=sys.stderr)

    if not VCD_FILE.is_file():
        fail("SimAVR did not produce the expected VCD file.")

    transitions = parse_vcd_file(VCD_FILE, "CH1")

    print(f"CH1 transitions: {len(transitions)}")

    if not transitions:
        fail("CH1 produced no transitions.")

    assert_alternates_high_low(transitions)

    assert_rising_edge_periods(
        transitions,
        expected_ns=EXPECTED_RISING_EDGE_PERIOD_NS,
        tolerance_ns=RISING_EDGE_TOLERANCE_NS,
    )

    print("CH1 VCD assertions: OK")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())