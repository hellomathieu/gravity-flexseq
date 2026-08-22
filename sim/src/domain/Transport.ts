import type { SequencerEngine } from "./SequencerEngine.js";

export class Transport {
  constructor(private readonly engine: SequencerEngine) {}

  start(): void {
    this.engine.reset();
    this.engine.start();
  }

  resume(): void {
    this.engine.start();
  }

  stop(): void {
    this.engine.stop();
  }

  reset(): void {
    this.engine.reset();
  }

  tick(ticks = 1): void {
    this.engine.advance(ticks);
  }
}
