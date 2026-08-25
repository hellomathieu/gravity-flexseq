export interface OnsetRow {
  channel: number;
  onsetIndex: number;
  step: number;
  subIndex: number;
  expectedTick: number;
  actualTick: number;
  gridErrorTicks: number;
  expectedUs: number;
  actualUs: number;
  timingErrorUs: number;
}

export interface SeriesStats {
  count: number;
  first: number;
  last: number;
  min: number;
  max: number;
  mean: number;
  median: number;
  absMax: number;
  stdDev: number;
}

export interface SlopeEstimate {
  slope: number;
  intercept: number;
  stdError: number;
  spanChange: number;
}

export interface DriftVerdict {
  drifting: boolean;
  jitterOnly: boolean;
  reasons: string[];
  ols: SlopeEstimate;
  theilSenSlope: number;
  theilSenSpanChange: number;
  cumulativeDriftTicks: number;
  lastMinusFirst: number;
  tailMaxExceedsHead: boolean;
  stats: SeriesStats;
}

export interface MatchResult {
  matched: Array<{ expectedIndex: number; actualIndex: number; gridErrorTicks: number }>;
  dropped: number[];
  unexpected: number[];
}

export interface OnsetBudget {
  expected: number;
  emitted: number;
  pending: number;
  dropped: number;
}

export const CSV_HEADER =
  'channel,onset_index,step,sub_index,expected_tick,actual_tick,' +
  'grid_error_ticks,expected_us,actual_us,timing_error_us';

export function parseOnsetCsv(text: string): OnsetRow[] {
  const lines = text.split('\n').map((l) => l.trim()).filter((l) => l.length > 0);
  if (lines.length === 0) return [];
  const start = lines[0]!.startsWith('channel,') ? 1 : 0;
  const rows: OnsetRow[] = [];
  for (let i = start; i < lines.length; ++i) {
    const f = lines[i]!.split(',');
    if (f.length < 10) throw new Error(`ligne CSV incomplete: ${lines[i]}`);
    rows.push({
      channel: Number(f[0]),
      onsetIndex: Number(f[1]),
      step: Number(f[2]),
      subIndex: Number(f[3]),
      expectedTick: Number(f[4]),
      actualTick: Number(f[5]),
      gridErrorTicks: Number(f[6]),
      expectedUs: Number(f[7]),
      actualUs: Number(f[8]),
      timingErrorUs: Number(f[9]),
    });
  }
  return rows;
}

export function seriesStats(values: number[]): SeriesStats {
  if (values.length === 0) {
    return { count: 0, first: 0, last: 0, min: 0, max: 0, mean: 0, median: 0, absMax: 0, stdDev: 0 };
  }
  const sorted = [...values].sort((a, b) => a - b);
  const n = values.length;
  const mean = values.reduce((s, v) => s + v, 0) / n;
  const variance = values.reduce((s, v) => s + (v - mean) * (v - mean), 0) / n;
  const median = n % 2 === 1
    ? sorted[(n - 1) / 2]!
    : (sorted[n / 2 - 1]! + sorted[n / 2]!) / 2;
  return {
    count: n,
    first: values[0]!,
    last: values[n - 1]!,
    min: sorted[0]!,
    max: sorted[n - 1]!,
    mean,
    median,
    absMax: Math.max(Math.abs(sorted[0]!), Math.abs(sorted[n - 1]!)),
    stdDev: Math.sqrt(variance),
  };
}

export function linearSlope(xs: number[], ys: number[]): SlopeEstimate {
  const n = xs.length;
  if (n !== ys.length) throw new Error('xs et ys de longueurs differentes');
  if (n < 3) return { slope: 0, intercept: n > 0 ? ys[0]! : 0, stdError: 0, spanChange: 0 };
  const meanX = xs.reduce((s, v) => s + v, 0) / n;
  const meanY = ys.reduce((s, v) => s + v, 0) / n;
  let sxx = 0;
  let sxy = 0;
  for (let i = 0; i < n; ++i) {
    const dx = xs[i]! - meanX;
    sxx += dx * dx;
    sxy += dx * (ys[i]! - meanY);
  }
  if (sxx === 0) return { slope: 0, intercept: meanY, stdError: 0, spanChange: 0 };
  const slope = sxy / sxx;
  const intercept = meanY - slope * meanX;
  let sse = 0;
  for (let i = 0; i < n; ++i) {
    const residual = ys[i]! - (intercept + slope * xs[i]!);
    sse += residual * residual;
  }
  const stdError = Math.sqrt(sse / (n - 2) / sxx);
  return { slope, intercept, stdError, spanChange: slope * (xs[n - 1]! - xs[0]!) };
}

export function theilSenSlope(xs: number[], ys: number[], maxPairs = 20000): number {
  const n = xs.length;
  if (n < 2) return 0;
  const slopes: number[] = [];
  const stride = Math.max(1, Math.floor(Math.sqrt((n * (n - 1)) / 2 / maxPairs)));
  for (let i = 0; i < n; i += stride) {
    for (let j = i + 1; j < n; j += stride) {
      const dx = xs[j]! - xs[i]!;
      if (dx !== 0) slopes.push((ys[j]! - ys[i]!) / dx);
    }
  }
  if (slopes.length === 0) return 0;
  slopes.sort((a, b) => a - b);
  const m = slopes.length;
  return m % 2 === 1 ? slopes[(m - 1) / 2]! : (slopes[m / 2 - 1]! + slopes[m / 2]!) / 2;
}

export interface DriftOptions {
  minSpanTicks?: number;
  sigmaFactor?: number;
}

export function assessDrift(
  xs: number[],
  ys: number[],
  options: DriftOptions = {},
): DriftVerdict {
  const minSpan = options.minSpanTicks ?? 1;
  const sigma = options.sigmaFactor ?? 3;
  const stats = seriesStats(ys);
  const ols = linearSlope(xs, ys);
  const ts = theilSenSlope(xs, ys);
  const span = xs.length > 1 ? xs[xs.length - 1]! - xs[0]! : 0;
  const tsSpanChange = ts * span;

  const reasons: string[] = [];
  const olsSignificant = Math.abs(ols.slope) > sigma * ols.stdError;
  const olsLarge = Math.abs(ols.spanChange) >= minSpan;
  const tsLarge = Math.abs(tsSpanChange) >= minSpan;

  if (olsLarge) reasons.push(`pente OLS: ${ols.spanChange.toFixed(3)} tick sur la course`);
  if (olsSignificant) reasons.push(`pente OLS significative a ${sigma} sigma`);
  if (tsLarge) reasons.push(`pente Theil-Sen: ${tsSpanChange.toFixed(3)} tick sur la course`);

  const head = ys.slice(0, Math.max(1, Math.floor(ys.length / 10)));
  const tail = ys.slice(ys.length - Math.max(1, Math.floor(ys.length / 10)));
  const tailMaxExceedsHead = seriesStats(tail).max > seriesStats(head).max;
  if (tailMaxExceedsHead) reasons.push('le maximum du dernier dixieme depasse celui du premier');

  const drifting = olsLarge && olsSignificant && tsLarge;
  if (!drifting) reasons.push('aucune derive cumulative detectee');

  return {
    drifting,
    jitterOnly: !drifting && stats.max !== stats.min,
    reasons,
    ols,
    theilSenSlope: ts,
    theilSenSpanChange: tsSpanChange,
    cumulativeDriftTicks: ols.spanChange,
    lastMinusFirst: stats.last - stats.first,
    tailMaxExceedsHead,
    stats,
  };
}

export function matchOnsets(
  expected: number[],
  actual: number[],
  toleranceTicks = 2,
): MatchResult {
  const matched: MatchResult['matched'] = [];
  const dropped: number[] = [];
  const unexpected: number[] = [];
  let i = 0;
  let j = 0;
  while (i < expected.length && j < actual.length) {
    const at = actual[j]!;
    if (at + toleranceTicks < expected[i]!) {
      unexpected.push(j);
      ++j;
      continue;
    }
    if (i + 1 < expected.length && at >= expected[i + 1]!) {
      dropped.push(i);
      ++i;
      continue;
    }
    matched.push({ expectedIndex: i, actualIndex: j, gridErrorTicks: at - expected[i]! });
    ++i;
    ++j;
  }
  while (j < actual.length) unexpected.push(j++);
  while (i < expected.length) dropped.push(i++);
  return { matched, dropped, unexpected };
}

export function reconcileOnsets(budget: OnsetBudget): { consistent: boolean; residual: number } {
  const residual = budget.expected - (budget.emitted + budget.pending + budget.dropped);
  return { consistent: residual === 0, residual };
}

export interface RecoveryVerdict {
  nominal: number;
  maxError: number;
  affectedCount: number;
  firstAffectedIndex: number;
  lastAffectedIndex: number;
  samplesToRecover: number;
  recovered: boolean;
  finalError: number;
  persistentOffset: number;
}

export function assessRecovery(ys: number[], nominal = 0): RecoveryVerdict {
  const n = ys.length;
  if (n === 0) {
    return {
      nominal,
      maxError: 0,
      affectedCount: 0,
      firstAffectedIndex: -1,
      lastAffectedIndex: -1,
      samplesToRecover: 0,
      recovered: true,
      finalError: 0,
      persistentOffset: 0,
    };
  }
  let maxError = 0;
  let affectedCount = 0;
  let firstAffectedIndex = -1;
  let lastAffectedIndex = -1;
  for (let i = 0; i < n; ++i) {
    const deviation = Math.abs(ys[i]! - nominal);
    if (deviation > maxError) maxError = deviation;
    if (ys[i]! !== nominal) {
      ++affectedCount;
      if (firstAffectedIndex < 0) firstAffectedIndex = i;
      lastAffectedIndex = i;
    }
  }
  const recovered = lastAffectedIndex < 0 || ys[n - 1]! === nominal;
  const samplesToRecover =
    lastAffectedIndex < 0 ? 0 : recovered ? lastAffectedIndex - firstAffectedIndex + 1 : n - firstAffectedIndex;
  return {
    nominal,
    maxError,
    affectedCount,
    firstAffectedIndex,
    lastAffectedIndex,
    samplesToRecover,
    recovered,
    finalError: ys[n - 1]!,
    persistentOffset: recovered ? 0 : ys[n - 1]! - nominal,
  };
}

export function subOnsetTick(stepTicks: number, triggers: number, k: number): number {
  if (triggers <= 1) return stepTicks;
  return Math.floor((stepTicks * k) / triggers);
}

export function subOnsetTicks(stepTicks: number, triggers: number): number[] {
  const out: number[] = [0];
  for (let k = 1; k < triggers; ++k) out.push(subOnsetTick(stepTicks, triggers, k));
  return out;
}
