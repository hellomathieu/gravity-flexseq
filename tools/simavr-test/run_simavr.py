#!/usr/bin/env python3

import argparse
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path


def find_simavr() -> str:
    for command in ("simavr", "run_avr"):
        executable = shutil.which(command)

        if executable:
            return executable

    raise RuntimeError(
        "SimAVR executable not found.\n"
        "Expected either 'simavr' or 'run_avr' in PATH."
    )


def run_simavr(
    executable: str,
    firmware: Path,
    vcd: Path,
    duration: float,
    trace: str,
) -> int:
    command = [
        executable,
        "-v",
        "-m",
        "atmega328p",
        "-f",
        "16000000",
        "-o",
        str(vcd),
        "--add-trace",
        trace,
        str(firmware),
    ]

    print("==> SIMAVR")
    print(" ".join(command))

    process = subprocess.Popen(command)

    try:
        time.sleep(duration)

        if process.poll() is None:
            process.send_signal(signal.SIGINT)

        return_code = process.wait(timeout=5)

    except subprocess.TimeoutExpired:
        process.kill()
        return_code = process.wait()

    print(f"SimAVR exit code: {return_code}")

    if not vcd.exists():
        raise RuntimeError(
            f"SimAVR completed without producing VCD: {vcd}"
        )

    if vcd.stat().st_size == 0:
        raise RuntimeError(
            f"SimAVR produced an empty VCD: {vcd}"
        )

    return return_code


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the real Gravity FlexSeq firmware under SimAVR."
    )

    parser.add_argument(
        "--firmware",
        required=True,
        type=Path,
    )

    parser.add_argument(
        "--vcd",
        required=True,
        type=Path,
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=0.5,
    )

    parser.add_argument(
        "--trace",
        default="CH1=portpin@0x07/0x44",
    )

    args = parser.parse_args()

    if not args.firmware.exists():
        print(
            f"Firmware not found: {args.firmware}",
            file=sys.stderr,
        )
        return 1

    try:
        executable = find_simavr()
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1

    print(f"Using SimAVR: {executable}")

    args.vcd.parent.mkdir(parents=True, exist_ok=True)

    if args.vcd.exists():
        args.vcd.unlink()

    try:
        run_simavr(
            executable=executable,
            firmware=args.firmware,
            vcd=args.vcd,
            duration=args.duration,
            trace=args.trace,
        )
    except RuntimeError as error:
        print(error, file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())