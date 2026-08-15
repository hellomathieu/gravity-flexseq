from dataclasses import dataclass


@dataclass(frozen=True)
class Transition:
    time_ns: int
    value: int


def parse_signal(vcd_text: str, signal_name: str) -> list[Transition]:
    lines = vcd_text.splitlines()

    signal_id = None

    for line in lines:
        if line.startswith("$var "):
            parts = line.split()

            if len(parts) >= 5 and parts[4] == signal_name:
                signal_id = parts[3]
                break

    if signal_id is None:
        raise ValueError(f"Signal not found: {signal_name}")

    transitions = []
    current_time = None

    for line in lines:
        line = line.strip()

        if not line:
            continue

        if line.startswith("#"):
            current_time = int(line[1:])
            continue

        if current_time is None:
            continue

        if line == f"0{signal_id}":
            transitions.append(Transition(current_time, 0))
        elif line == f"1{signal_id}":
            transitions.append(Transition(current_time, 1))

    return transitions


def parse_vcd_file(path: str, signal_name: str) -> list[Transition]:
    with open(path, "r", encoding="utf-8") as file:
        return parse_signal(file.read(), signal_name)


def assert_periods(
    transitions: list[Transition],
    expected_ns: int,
    tolerance_ns: int,
) -> None:
    if len(transitions) < 3:
        raise AssertionError(
            "At least three transitions are required"
        )

    durations = []

    for previous, current in zip(transitions, transitions[1:]):
        duration = current.time_ns - previous.time_ns
        durations.append(duration)

    for index, duration in enumerate(durations):
        if abs(duration - expected_ns) > tolerance_ns:
            raise AssertionError(
                f"Transition {index}: "
                f"expected {expected_ns} ns ± {tolerance_ns} ns, "
                f"got {duration} ns"
            )
