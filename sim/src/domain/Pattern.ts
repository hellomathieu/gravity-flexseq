
export const RATCHET_NONE = 0;
export const RATCHET_PAUSE_HALF = 1;
export const RATCHET_2 = 2;
export const RATCHET_3 = 3;
export const RATCHET_4 = 4;
export const RATCHET_6 = 6;
export const RATCHET_TRIPLET = 7;
export const RATCHET_OFFBEAT = 8;
export const RATCHET_PAUSE_2 = 9;
export const RATCHET_PAUSE_3 = 10;
export const RATCHET_PAUSE_4 = 11;
export const RATCHET_PAUSE_5 = 12;
export const RATCHET_PAUSE_6 = 13;
export const RATCHET_PAUSE_7 = 14;
export const RATCHET_PAUSE_8 = 15;

export const RATCHET_CODES: readonly number[] = [
  RATCHET_NONE, RATCHET_OFFBEAT, RATCHET_2, RATCHET_3, RATCHET_4, RATCHET_6, RATCHET_TRIPLET,
  RATCHET_PAUSE_HALF, RATCHET_PAUSE_2, RATCHET_PAUSE_3, RATCHET_PAUSE_4,
  RATCHET_PAUSE_5, RATCHET_PAUSE_6, RATCHET_PAUSE_7, RATCHET_PAUSE_8,
];

export function isValidRatchet(code: number): boolean {
  return RATCHET_CODES.includes(code);
}

export const MIN_SLOT_TICKS = 2;

export function ratchetFitsStep(code: number, ticksPerStep: number): boolean {
  const triggers = ratchetTriggers(code);
  if (triggers <= 1) return true;
  const stepTicks = ticksPerStep * ratchetSpan(code);
  return Math.floor(stepTicks / triggers) >= MIN_SLOT_TICKS;
}

export function ratchetTriggers(code: number): number {
  if (code === RATCHET_TRIPLET) return 3;
  if (code === RATCHET_OFFBEAT) return 2;
  if (code === RATCHET_2 || code === RATCHET_3 || code === RATCHET_4 || code === RATCHET_6) {
    return code;
  }
  return 1;
}

export function ratchetPauseUnits(code: number): number {
  if (code < RATCHET_PAUSE_2 || code > RATCHET_PAUSE_8) return 0;
  return code - RATCHET_PAUSE_2 + 2;
}

export function ratchetIsPause(code: number): boolean {
  return code === RATCHET_PAUSE_HALF || ratchetPauseUnits(code) !== 0;
}

export function ratchetSkipsStepOnset(code: number): boolean {
  return code === RATCHET_OFFBEAT || ratchetIsPause(code);
}

export function ratchetSpan(code: number): number {
  if (code === RATCHET_TRIPLET) return 2;
  const pause = ratchetPauseUnits(code);
  return pause !== 0 ? pause : 1;
}

export const MAX_STEP_TICKS = 65535;

export function ratchetStepTicks(code: number, ticksPerStep: number): number {
  if (code === RATCHET_PAUSE_HALF) return ticksPerStep / 2;
  let ticks = ticksPerStep * ratchetSpan(code);
  while (ticks > MAX_STEP_TICKS) ticks -= ticksPerStep;
  return ticks;
}

export function ratchetLabel(code: number): string {
  if (code === RATCHET_TRIPLET) return "3T";
  if (code === RATCHET_OFFBEAT) return "off";
  if (code === RATCHET_PAUSE_HALF) return "P1/2";
  if (ratchetPauseUnits(code) !== 0) return `P${ratchetPauseUnits(code)}`;
  if (code === RATCHET_NONE) return "";
  return String(code);
}
export class Pattern {
  static readonly DEFAULT_TOTAL_STEPS = 36;

  private readonly steps: boolean[];
  private readonly ratchets: number[];

  constructor() {
    this.steps = new Array<boolean>(Pattern.DEFAULT_TOTAL_STEPS).fill(false);
    this.ratchets = new Array<number>(Pattern.DEFAULT_TOTAL_STEPS).fill(RATCHET_NONE);
  }

  private static isValidIndex(index: number): boolean {
    return Number.isInteger(index) && index >= 0 && index < Pattern.DEFAULT_TOTAL_STEPS;
  }

    readStep(index: number): boolean | null {
    if (!Pattern.isValidIndex(index)) return null;
    return this.steps[index] ?? false;
  }

    writeStep(index: number, active: boolean): boolean {
    if (!Pattern.isValidIndex(index)) return false;
    this.steps[index] = active;
    return true;
  }

      setLowStepMask(bits: number): void {
    for (let step = 0; step < 16; ++step) {
      this.steps[step] = ((bits >> step) & 1) === 1;
    }
  }

  clear(): void {
    this.steps.fill(false);
    this.clearRatchets();
  }

      getRatchet(index: number): number {
    if (!Pattern.isValidIndex(index)) return RATCHET_NONE;
    return this.ratchets[index] ?? RATCHET_NONE;
  }

    setRatchet(index: number, code: number): boolean {
    if (!Pattern.isValidIndex(index) || !isValidRatchet(code)) return false;
    this.ratchets[index] = code;
    return true;
  }

  clearRatchets(): void {
    this.ratchets.fill(RATCHET_NONE);
  }

}
