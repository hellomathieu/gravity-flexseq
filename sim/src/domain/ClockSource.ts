export enum ClockMode {
  INT = 0,
  EXT = 1,
  MIDI = 2,
}

export const CLOCK_MODE_COUNT = 3;
export const PPQN_CHOICE_COUNT = 4;

export const CLOCK_SOURCE_INT = 0;
export const CLOCK_SOURCE_FIRST_EXT = 1;
export const CLOCK_SOURCE_MIDI = CLOCK_SOURCE_FIRST_EXT + PPQN_CHOICE_COUNT;
export const CLOCK_SOURCE_VALUE_COUNT = CLOCK_SOURCE_MIDI + 1;

export function clockModeOf(source: number): ClockMode {
  if (source < CLOCK_SOURCE_FIRST_EXT) return ClockMode.INT;
  return source < CLOCK_SOURCE_MIDI ? ClockMode.EXT : ClockMode.MIDI;
}

export function ppqnIndexOf(source: number): number {
  return clockModeOf(source) === ClockMode.EXT ? source - CLOCK_SOURCE_FIRST_EXT : 0;
}

export function ppqnAtIndex(index: number): number {
  if (index === 0) return 24;
  if (index === 1) return 4;
  if (index === 2) return 2;
  return 1;
}

export function clockSourceOf(mode: ClockMode, ppqnIndex: number): number {
  if (mode === ClockMode.INT) return CLOCK_SOURCE_INT;
  if (mode === ClockMode.MIDI) return CLOCK_SOURCE_MIDI;
  return CLOCK_SOURCE_FIRST_EXT + (ppqnIndex >= 0 && ppqnIndex < PPQN_CHOICE_COUNT ? ppqnIndex : 0);
}
