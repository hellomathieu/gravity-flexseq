import { readFileSync } from 'node:fs';
import {
  assessDrift,
  parseOnsetCsv,
  seriesStats,
  type OnsetRow,
} from './driftEstimator';

function fmt(value: number, digits = 3): string {
  return Number.isFinite(value) ? value.toFixed(digits) : 'n/a';
}

function reportChannel(label: string, rows: OnsetRow[], tickUs: number): boolean {
  const xs = rows.map((r) => r.expectedTick);
  const grid = rows.map((r) => r.gridErrorTicks);
  const timing = rows.map((r) => r.timingErrorUs);
  const verdict = assessDrift(xs, grid);
  const g = verdict.stats;
  const t = seriesStats(timing);

  process.stdout.write(`\n  ${label}  (${rows.length} onsets)\n`);
  process.stdout.write(
    `    grid_error_ticks   premier ${g.first}  dernier ${g.last}  min ${g.min}  max ${g.max}` +
      `  moyenne ${fmt(g.mean)}  mediane ${fmt(g.median)}  |max| ${g.absMax}\n`,
  );
  process.stdout.write(
    `    timing_error_us    min ${fmt(t.min)}  max ${fmt(t.max)}  moyenne ${fmt(t.mean)}` +
      `  mediane ${fmt(t.median)}  ecart-type ${fmt(t.stdDev)}\n`,
  );
  process.stdout.write(
    `    pente              OLS ${verdict.ols.slope.toExponential(3)} tick/tick` +
      ` (erreur type ${verdict.ols.stdError.toExponential(3)})` +
      `  Theil-Sen ${verdict.theilSenSlope.toExponential(3)}\n`,
  );
  process.stdout.write(
    `    derive cumulative  ${fmt(verdict.cumulativeDriftTicks)} tick sur la course` +
      `  (Theil-Sen ${fmt(verdict.theilSenSpanChange)})` +
      `  = ${fmt(verdict.cumulativeDriftTicks * tickUs / 1000)} ms\n`,
  );
  process.stdout.write(`    verdict            ${verdict.drifting ? 'DERIVE' : 'pas de derive'}\n`);
  return verdict.drifting;
}

function main(): void {
  const csvPath = process.argv[2];
  const tickUs = Number(process.argv[3] ?? '5208.333');
  if (!csvPath) {
    process.stderr.write('usage: driftReport <onsets.csv> [tick_us]\n');
    process.exit(2);
  }
  const rows = parseOnsetCsv(readFileSync(csvPath, 'utf8'));
  if (rows.length === 0) {
    process.stderr.write('aucun onset dans le CSV\n');
    process.exit(2);
  }

  process.stdout.write('=== ANALYSE DE DERIVE ===\n');
  let drifting = false;
  const channels = [...new Set(rows.map((r) => r.channel))].sort((a, b) => a - b);
  for (const channel of channels) {
    const sub = rows.filter((r) => r.channel === channel);
    drifting = reportChannel(`channel ${channel}`, sub, tickUs) || drifting;
  }
  drifting = reportChannel('TOUS CHANNELS', rows, tickUs) || drifting;

  const grid = rows.map((r) => r.gridErrorTicks);
  const distinct = [...new Set(grid)].sort((a, b) => a - b);
  process.stdout.write(`\n  valeurs de grid_error_ticks rencontrees : ${distinct.join(', ')}\n`);
  process.stdout.write(
    `  conclusion : ${
      drifting
        ? 'DERIVE CUMULATIVE DETECTEE'
        : '0 tick de derive cumulative detecte dans les conditions du test'
    }\n`,
  );
  process.exit(drifting ? 1 : 0);
}

main();
