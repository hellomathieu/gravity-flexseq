export const DEFAULT_SEED = 0xace1;

export class Prng {
  private value: number;

  constructor(seed: number = DEFAULT_SEED) {
    this.value = seed === 0 ? DEFAULT_SEED : seed & 0xffff;
  }

  seed(seed: number): void {
    this.value = seed === 0 ? DEFAULT_SEED : seed & 0xffff;
  }

  state(): number {
    return this.value;
  }

  next(): number {
    let x = this.value;
    x = (x ^ (x << 7)) & 0xffff;
    x = x ^ (x >>> 9);
    x = (x ^ (x << 8)) & 0xffff;
    this.value = x;
    return x;
  }

  below(bound: number): number {
    if (bound <= 0) return 0;
    return this.next() % bound;
  }
}
