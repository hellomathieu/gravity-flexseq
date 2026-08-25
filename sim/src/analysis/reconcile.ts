export interface ReconcilePair {
  expectedIndex: number;
  observedIndex: number;
  delayTicks: number;
}

export interface ReconcileResult {
  expected: number;
  observed: number;
  rawDifference: number;
  matched: number;
  late: number;
  missing: number;
  extra: number;
  ambiguous: number;
  missingPositions: number[];
  extraPositions: number[];
  ambiguousRange: [number, number] | null;
  maxDelayTicks: number;
  maxDeficit: number;
  finalDeficit: number;
  deficitReturnsToZero: boolean;
  pairs: ReconcilePair[];
}

export interface ReconcileOptions {
  maxDelayTicks?: number;
}

function deficitWalk(expected: number[], observed: number[]): { maxDeficit: number } {
  let i = 0;
  let j = 0;
  let deficit = 0;
  let maxDeficit = 0;
  while (i < expected.length || j < observed.length) {
    const takeExpected =
      j >= observed.length || (i < expected.length && expected[i]! <= observed[j]!);
    if (takeExpected) {
      ++deficit;
      if (deficit > maxDeficit) maxDeficit = deficit;
      ++i;
    } else {
      --deficit;
      ++j;
    }
  }
  return { maxDeficit };
}

function feasibleSingleLoss(
  expected: number[],
  observed: number[],
  maxDelay: number,
): number[] {
  const n = observed.length;
  const prefixOk: boolean[] = new Array(expected.length + 1).fill(false);
  let ok = true;
  prefixOk[0] = true;
  for (let i = 0; i < n; ++i) {
    const delay = observed[i]! - expected[i]!;
    ok = ok && delay >= 0 && delay <= maxDelay;
    prefixOk[i + 1] = ok;
  }
  for (let i = n + 1; i <= expected.length; ++i) prefixOk[i] = false;

  const suffixOk: boolean[] = new Array(expected.length + 1).fill(true);
  ok = true;
  for (let i = n - 1; i >= 0; --i) {
    const delay = observed[i]! - expected[i + 1]!;
    ok = ok && delay >= 0 && delay <= maxDelay;
    suffixOk[i] = ok;
  }

  const feasible: number[] = [];
  for (let p = 0; p <= expected.length - 1; ++p) {
    if (p <= n && prefixOk[p] === true && (suffixOk[p] ?? true)) feasible.push(p);
  }
  return feasible;
}

export function reconcileTimeline(
  expectedTicks: number[],
  observedTicks: number[],
  options: ReconcileOptions = {},
): ReconcileResult {
  const expected = [...expectedTicks].sort((a, b) => a - b);
  const observed = [...observedTicks].sort((a, b) => a - b);
  const maxDelay = options.maxDelayTicks ?? Number.POSITIVE_INFINITY;

  const pairs: ReconcilePair[] = [];
  const extraPositions: number[] = [];
  const unpaired: number[] = [];
  let i = 0;
  let j = 0;
  while (i < expected.length && j < observed.length) {
    if (observed[j]! < expected[i]!) {
      extraPositions.push(j);
      ++j;
      continue;
    }
    pairs.push({ expectedIndex: i, observedIndex: j, delayTicks: observed[j]! - expected[i]! });
    ++i;
    ++j;
  }
  while (j < observed.length) extraPositions.push(j++);
  while (i < expected.length) unpaired.push(i++);

  const { maxDeficit } = deficitWalk(expected, observed);
  const finalDeficit = expected.length - observed.length;

  let maxDelayTicks = 0;
  let late = 0;
  for (const pair of pairs) {
    if (pair.delayTicks > maxDelayTicks) maxDelayTicks = pair.delayTicks;
    if (pair.delayTicks >= 1) ++late;
  }

  const missing = unpaired.length;
  let missingPositions: number[] = [];
  let ambiguous = 0;
  let ambiguousRange: [number, number] | null = null;

  if (missing === 1) {
    const feasible = feasibleSingleLoss(expected, observed, maxDelay);
    if (feasible.length === 1) {
      missingPositions = [feasible[0]!];
    } else if (feasible.length > 1) {
      ambiguous = 1;
      ambiguousRange = [feasible[0]!, feasible[feasible.length - 1]!];
    } else {
      missingPositions = unpaired;
    }
  } else if (missing > 1) {
    ambiguous = missing;
    ambiguousRange = [unpaired[0]!, unpaired[unpaired.length - 1]!];
  }

  return {
    expected: expected.length,
    observed: observed.length,
    rawDifference: expected.length - observed.length,
    matched: pairs.length,
    late,
    missing,
    extra: extraPositions.length,
    ambiguous,
    missingPositions,
    extraPositions,
    ambiguousRange,
    maxDelayTicks,
    maxDeficit,
    finalDeficit,
    deficitReturnsToZero: finalDeficit === 0,
    pairs,
  };
}
