export const DEFAULT_REVERSAL_WINDOW_MS = 12;
export const MAX_DELTA = 127;

export class EncoderFilter {
  private lastMs = 0;
  private lastSign = 0;
  private sawFirst = false;
  private suppressedCount = 0;
  private lastInterval = 0;

  constructor(private windowMs: number = DEFAULT_REVERSAL_WINDOW_MS) {}

  filter(delta: number, nowMs: number): number {
    if (delta === 0) return 0;
    const sign = delta < 0 ? -1 : 1;
    if (!this.sawFirst) {
      this.sawFirst = true;
      this.lastMs = nowMs;
      this.lastSign = sign;
      ++this.suppressedCount;
      return 0;
    }
    const elapsed = nowMs - this.lastMs;
    this.lastInterval = Math.min(elapsed, 0xffff);
    if (sign !== this.lastSign && elapsed < this.windowMs) {
      ++this.suppressedCount;
      return 0;
    }
    this.lastMs = nowMs;
    this.lastSign = sign;
    return Math.max(-MAX_DELTA, Math.min(MAX_DELTA, delta));
  }

  reset(): void {
    this.lastMs = 0;
    this.lastSign = 0;
    this.sawFirst = false;
    this.lastInterval = 0;
  }

  get reversalWindowMs(): number {
    return this.windowMs;
  }

  setReversalWindowMs(ms: number): void {
    this.windowMs = ms;
  }

  get suppressed(): number {
    return this.suppressedCount;
  }

  get lastIntervalMs(): number {
    return this.lastInterval;
  }

  get sawFirstMovement(): boolean {
    return this.sawFirst;
  }
}
