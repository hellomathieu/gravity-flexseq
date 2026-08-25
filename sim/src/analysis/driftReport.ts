import { readFileSync } from 'node:fs';
import { reconcileTimeline } from './reconcile';
import {
  assessDrift,
  assessRecovery,
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
      `  = ${fmt((verdict.cumulativeDriftTicks * tickUs) / 1000)} ms\n`,
  );
  process.stdout.write(`    verdict            ${verdict.drifting ? 'DERIVE' : 'pas de derive'}\n`);
  return verdict.drifting;
}

function phase(rows: OnsetRow[], from: number, to: number, which: 'before' | 'during' | 'after'): OnsetRow[] {
  if (which === 'before') return rows.filter((r) => r.expectedTick < from);
  if (which === 'during') return rows.filter((r) => r.expectedTick >= from && r.expectedTick <= to);
  return rows.filter((r) => r.expectedTick > to);
}

function reportWindow(rows: OnsetRow[], from: number, to: number): boolean {
  process.stdout.write(`\n=== RETARD ARTIFICIEL : ticks ${from} a ${to} ===\n`);
  for (const which of ['before', 'during', 'after'] as const) {
    const sub = phase(rows, from, to, which);
    const g = seriesStats(sub.map((r) => r.gridErrorTicks));
    const t = seriesStats(sub.map((r) => r.timingErrorUs));
    const label = which === 'before' ? 'avant ' : which === 'during' ? 'pendant' : 'apres ';
    process.stdout.write(
      `  ${label}  ${String(sub.length).padStart(5)} onsets` +
        `   grid_error min ${g.min} max ${g.max} moyenne ${fmt(g.mean, 4)}` +
        `   timing_us min ${fmt(t.min, 1)} max ${fmt(t.max, 1)} mediane ${fmt(t.median, 1)}\n`,
    );
  }

  const channels = [...new Set(rows.map((r) => r.channel))].sort((a, b) => a - b);
  let allRecovered = true;
  let worstError = 0;
  let worstRecovery = 0;
  let persistent = 0;
  for (const channel of channels) {
    const sub = rows
      .filter((r) => r.channel === channel)
      .sort((a, b) => a.expectedTick - b.expectedTick);
    const r = assessRecovery(sub.map((x) => x.gridErrorTicks));
    allRecovered = allRecovered && r.recovered;
    worstError = Math.max(worstError, r.maxError);
    worstRecovery = Math.max(worstRecovery, r.samplesToRecover);
    persistent = Math.max(persistent, Math.abs(r.persistentOffset));
  }
  process.stdout.write(
    `\n  recuperation       erreur maximale ${worstError} tick(s)` +
      `  onsets affectes jusqu a ${worstRecovery}` +
      `  erreur persistante ${persistent}\n`,
  );
  process.stdout.write(
    `  conclusion         ${
      allRecovered
        ? 'le moteur revient sur sa grille apres le retard'
        : 'UNE ERREUR SUBSISTE apres le retour au regime nominal'
    }\n`,
  );
  return !allRecovered;
}

interface TimelineRow { channel: number; kind: string; tick: number }

function parseTimeline(text: string): TimelineRow[] {
  const lines = text.split('\n').map((l) => l.trim()).filter((l) => l.length > 0);
  const start = lines[0]!.startsWith('channel,') ? 1 : 0;
  const rows: TimelineRow[] = [];
  for (let i = start; i < lines.length; ++i) {
    const f = lines[i]!.split(',');
    if (f.length < 3) throw new Error(`ligne de timeline incomplete: ${lines[i]}`);
    rows.push({ channel: Number(f[0]), kind: f[1]!, tick: Number(f[2]) });
  }
  return rows;
}

function reportTimeline(path: string, maxDelayTicks: number | undefined): void {
  const rows = parseTimeline(readFileSync(path, 'utf8'));
  const expected = rows.filter((r) => r.kind === 'E').map((r) => r.tick);
  const channels = [...new Set(rows.filter((r) => r.kind === 'O').map((r) => r.channel))].sort(
    (a, b) => a - b,
  );

  process.stdout.write('\n=== COMPTAGE BRUT ET APPARIEMENT ===\n');
  process.stdout.write(
    `  borne de retard plausible : ${
      maxDelayTicks === undefined ? 'aucune' : `${maxDelayTicks} ticks`
    }\n`,
  );
  process.stdout.write(
    '  channel  attendus  fronts   ecart  apparies  en retard  manquants  en trop  ambigus' +
      '  retard max  dette max\n',
  );

  let totalExpected = 0;
  let totalObserved = 0;
  let totalMissing = 0;
  let totalExtra = 0;
  let totalAmbiguous = 0;
  let worstDelay = 0;
  let worstDeficit = 0;

  for (const channel of channels) {
    const observed = rows.filter((r) => r.kind === 'O' && r.channel === channel).map((r) => r.tick);
    const r = reconcileTimeline(
      expected,
      observed,
      maxDelayTicks === undefined ? {} : { maxDelayTicks },
    );
    totalExpected += r.expected;
    totalObserved += r.observed;
    totalMissing += r.missing;
    totalExtra += r.extra;
    totalAmbiguous += r.ambiguous;
    worstDelay = Math.max(worstDelay, r.maxDelayTicks);
    worstDeficit = Math.max(worstDeficit, r.maxDeficit);
    process.stdout.write(
      `  ${String(channel).padStart(7)}  ${String(r.expected).padStart(8)}` +
        `  ${String(r.observed).padStart(6)}  ${String(r.rawDifference).padStart(6)}` +
        `  ${String(r.matched).padStart(8)}  ${String(r.late).padStart(9)}` +
        `  ${String(r.missing).padStart(9)}  ${String(r.extra).padStart(7)}` +
        `  ${String(r.ambiguous).padStart(7)}  ${String(r.maxDelayTicks).padStart(10)}` +
        `  ${String(r.maxDeficit).padStart(9)}\n`,
    );
  }

  process.stdout.write(
    `\n  TOTAL    attendus ${totalExpected}  fronts ${totalObserved}` +
      `  ecart brut ${totalExpected - totalObserved}` +
      `  manquants ${totalMissing}  en trop ${totalExtra}  ambigus ${totalAmbiguous}` +
      `  retard max ${worstDelay} ticks  dette max ${worstDeficit}\n`,
  );
  if (totalAmbiguous > 0) {
    process.stdout.write(
      '  ⚠ appariement AMBIGU sur au moins un channel : le comptage brut fait foi,' +
        ' la position des manquants n est pas etablie\n',
    );
  }
}

function main(): void {
  const csvPath = process.argv[2];
  const tickUs = Number(process.argv[3] ?? '5208.333');
  const windowFrom = process.argv[4] ? Number(process.argv[4]) : null;
  const windowTo = process.argv[5] ? Number(process.argv[5]) : null;
  if (!csvPath) {
    process.stderr.write('usage: driftReport <onsets.csv> [tick_us] [tick_debut] [tick_fin]\n');
    process.exit(2);
  }
  const rows = parseOnsetCsv(readFileSync(csvPath, 'utf8'));
  if (rows.length === 0) {
    process.stderr.write('aucun onset dans le CSV\n');
    process.exit(2);
  }

  process.stdout.write('=== ANALYSE DE DERIVE ===\n');
  let failed = false;
  const channels = [...new Set(rows.map((r) => r.channel))].sort((a, b) => a - b);
  for (const channel of channels) {
    const sub = rows.filter((r) => r.channel === channel);
    failed = reportChannel(`channel ${channel}`, sub, tickUs) || failed;
  }
  failed = reportChannel('TOUS CHANNELS', rows, tickUs) || failed;

  const grid = rows.map((r) => r.gridErrorTicks);
  const distinct = [...new Set(grid)].sort((a, b) => a - b);
  process.stdout.write(`\n  valeurs de grid_error_ticks rencontrees : ${distinct.join(', ')}\n`);
  process.stdout.write(
    `  conclusion : ${
      failed
        ? 'DERIVE CUMULATIVE DETECTEE'
        : '0 tick de derive cumulative detecte dans les conditions du test'
    }\n`,
  );

  if (windowFrom !== null && windowTo !== null && windowTo > 0) {
    failed = reportWindow(rows, windowFrom, windowTo) || failed;
  }

  const timelinePath = process.argv[6];
  if (timelinePath) {
    const bound = process.argv[7] ? Number(process.argv[7]) : undefined;
    reportTimeline(timelinePath, bound);
  }

  process.exit(failed ? 1 : 0);
}

main();
