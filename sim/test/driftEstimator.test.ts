import { describe, it, expect } from 'vitest';
import {
  assessDrift,
  assessRecovery,
  matchOnsets,
  parseOnsetCsv,
  reconcileOnsets,
  seriesStats,
  subOnsetTick,
  subOnsetTicks,
  theilSenSlope,
  linearSlope,
} from '../src/analysis/driftEstimator';

function ticks(count: number, step = 96): number[] {
  return Array.from({ length: count }, (_, i) => i * step);
}

describe('driftEstimator — series de reference', () => {
  it('1. une serie nulle ne porte aucune derive', () => {
    const xs = ticks(500);
    const ys = xs.map(() => 0);
    const v = assessDrift(xs, ys);
    expect(v.drifting).toBe(false);
    expect(v.cumulativeDriftTicks).toBeCloseTo(0, 9);
    expect(v.stats.max).toBe(0);
  });

  it('2. une gigue positive constante est une gigue, pas une derive', () => {
    const xs = ticks(500);
    const pattern = [1, 1, 2, 1, 2];
    const ys = xs.map((_, i) => pattern[i % pattern.length]!);
    const v = assessDrift(xs, ys);
    expect(v.drifting).toBe(false);
    expect(v.jitterOnly).toBe(true);
    expect(v.stats.mean).toBeGreaterThan(1);
    expect(v.stats.max).toBe(2);
  });

  it('3. une derive artificielle de 1 tick par onset est detectee', () => {
    const xs = ticks(200);
    const ys = xs.map((_, i) => i);
    const v = assessDrift(xs, ys);
    expect(v.drifting).toBe(true);
    expect(v.cumulativeDriftTicks).toBeCloseTo(199, 6);
  });

  it('4. une pente connue est retrouvee dans la tolerance', () => {
    const xs = Array.from({ length: 1000 }, (_, i) => i * 100);
    const knownSlope = 0.001;
    const ys = xs.map((x) => x * knownSlope);
    const v = assessDrift(xs, ys);
    expect(v.ols.slope).toBeCloseTo(knownSlope, 9);
    expect(v.theilSenSlope).toBeCloseTo(knownSlope, 9);
    expect(v.cumulativeDriftTicks).toBeCloseTo(99.9, 3);
    expect(v.drifting).toBe(true);
  });

  it('7. un retard ponctuel suivi d un retour a la grille n est pas une derive', () => {
    const xs = ticks(400);
    const ys = xs.map((_, i) => (i === 200 ? 11 : 0));
    const v = assessDrift(xs, ys);
    expect(v.drifting).toBe(false);
    expect(v.stats.max).toBe(11);
    expect(v.stats.last).toBe(0);
  });

  it('7bis. un retard JAMAIS rattrape est une derive', () => {
    const xs = ticks(400);
    const ys = xs.map((_, i) => (i < 200 ? 0 : 11));
    const v = assessDrift(xs, ys);
    expect(v.drifting).toBe(true);
  });

  it('une derive ne se conclut jamais du seul premier contre dernier point', () => {
    const xs = ticks(400);
    const ys = xs.map((_, i) => (i === 0 || i === 399 ? 0 : i * 0.05));
    const v = assessDrift(xs, ys);
    expect(v.lastMinusFirst).toBe(0);
    expect(v.drifting).toBe(true);
  });
});

describe('driftEstimator — recuperation apres un retard', () => {
  it('A. un retard ponctuel avec retour a la grille est une perturbation locale', () => {
    const r = assessRecovery([0, 0, 0, 1, 1, 0, 0, 0]);
    expect(r.recovered).toBe(true);
    expect(r.maxError).toBe(1);
    expect(r.affectedCount).toBe(2);
    expect(r.samplesToRecover).toBe(2);
    expect(r.finalError).toBe(0);
    expect(r.persistentOffset).toBe(0);
  });

  it('B. un decalage qui reste est un decalage permanent', () => {
    const r = assessRecovery([0, 0, 0, 1, 1, 1, 1, 1]);
    expect(r.recovered).toBe(false);
    expect(r.persistentOffset).toBe(1);
    expect(r.finalError).toBe(1);
    expect(r.firstAffectedIndex).toBe(3);
  });

  it('C. une derive progressive n est pas une recuperation, et la pente la voit', () => {
    const ys = [0, 1, 2, 3, 4, 5];
    const r = assessRecovery(ys);
    expect(r.recovered).toBe(false);
    expect(r.persistentOffset).toBe(5);
    const xs = ys.map((_, i) => i * 96);
    expect(assessDrift(xs, ys).drifting).toBe(true);
  });

  it('D. un retard important suivi d un retour est une recuperation', () => {
    const r = assessRecovery([0, 0, 2, 2, 2, 1, 0, 0]);
    expect(r.recovered).toBe(true);
    expect(r.maxError).toBe(2);
    expect(r.affectedCount).toBe(4);
    expect(r.samplesToRecover).toBe(4);
    expect(r.finalError).toBe(0);
  });

  it('E. un onset perdu se distingue d un onset en retard', () => {
    const expected = [96, 192, 288, 384];
    const late = matchOnsets(expected, [96, 194, 288, 384]);
    expect(late.dropped).toEqual([]);
    expect(assessRecovery(late.matched.map((m) => m.gridErrorTicks)).recovered).toBe(true);

    const lost = matchOnsets(expected, [96, 288, 384]);
    expect(lost.dropped).toEqual([1]);
    expect(lost.matched).toHaveLength(3);
  });

  it('F. une dette temporaire remonte puis retombe, et ce n est pas une derive', () => {
    const pending = [0, 0, 3, 2, 1, 0, 0, 0];
    const r = assessRecovery(pending);
    expect(r.maxError).toBe(3);
    expect(r.recovered).toBe(true);
    expect(r.finalError).toBe(0);

    const xs = pending.map((_, i) => i * 96);
    expect(assessDrift(xs, pending).drifting).toBe(false);
  });

  it('une dette qui ne redescend jamais n est pas une recuperation', () => {
    const r = assessRecovery([0, 0, 3, 3, 3, 3]);
    expect(r.recovered).toBe(false);
    expect(r.persistentOffset).toBe(3);
  });
});

describe('driftEstimator — appariement des onsets', () => {
  it('5. un onset manquant est detecte', () => {
    const expected = [96, 192, 288, 384, 480];
    const actual = [96, 192, 384, 480];
    const m = matchOnsets(expected, actual);
    expect(m.dropped).toEqual([2]);
    expect(m.unexpected).toEqual([]);
    expect(m.matched).toHaveLength(4);
  });

  it('6. un onset supplementaire est detecte', () => {
    const expected = [96, 192, 288];
    const actual = [96, 120, 192, 288];
    const m = matchOnsets(expected, actual);
    expect(m.unexpected).toEqual([1]);
    expect(m.matched).toHaveLength(3);
    expect(m.dropped).toEqual([]);
  });

  it('un retard sous la tolerance reste apparie, avec son erreur de grille', () => {
    const expected = [96, 192, 288];
    const actual = [97, 194, 288];
    const m = matchOnsets(expected, actual);
    expect(m.dropped).toEqual([]);
    expect(m.matched.map((x) => x.gridErrorTicks)).toEqual([1, 2, 0]);
  });

  it('9. le budget d onsets se reconcilie: expected = emitted + pending + dropped', () => {
    expect(reconcileOnsets({ expected: 100, emitted: 94, pending: 2, dropped: 4 })).toEqual({
      consistent: true,
      residual: 0,
    });
    expect(reconcileOnsets({ expected: 100, emitted: 94, pending: 0, dropped: 4 }).consistent).toBe(
      false,
    );
  });
});

describe('driftEstimator — arrondis de sous-onsets', () => {
  it('8. la division est ENTIERE et la multiplication vient avant', () => {
    expect(subOnsetTick(32, 3, 1)).toBe(10);
    expect(subOnsetTick(32, 3, 2)).toBe(21);
    expect(subOnsetTick(8, 3, 1)).toBe(2);
    expect(subOnsetTick(8, 3, 2)).toBe(5);
  });

  it('8bis. les positions theoriques suivent la table du PRD 6.3.1', () => {
    expect(subOnsetTicks(96, 2)).toEqual([0, 48]);
    expect(subOnsetTicks(96, 3)).toEqual([0, 32, 64]);
    expect(subOnsetTicks(96, 4)).toEqual([0, 24, 48, 72]);
    expect(subOnsetTicks(96, 6)).toEqual([0, 16, 32, 48, 64, 80]);
    expect(subOnsetTicks(24, 6)).toEqual([0, 4, 8, 12, 16, 20]);
    expect(subOnsetTicks(8, 3)).toEqual([0, 2, 5]);
    expect(subOnsetTicks(6, 3)).toEqual([0, 2, 4]);
  });

  it('8ter. un ecart inegal du a l arrondi n est PAS une derive', () => {
    const positions: number[] = [];
    for (let step = 0; step < 200; ++step) {
      for (const sub of subOnsetTicks(8, 3)) positions.push(step * 8 + sub);
    }
    const gaps = positions.slice(1).map((p, i) => p - positions[i]!);
    expect(new Set(gaps)).toEqual(new Set([2, 3]));

    const v = assessDrift(positions, positions.map(() => 0));
    expect(v.drifting).toBe(false);
  });
});

describe('driftEstimator — robustesse des estimateurs', () => {
  it('Theil-Sen resiste a une valeur aberrante qui tire la moindre-carre', () => {
    const xs = ticks(200);
    const ys = xs.map((_, i) => (i === 199 ? 500 : 0));
    expect(Math.abs(theilSenSlope(xs, ys))).toBeLessThan(1e-9);
    expect(linearSlope(xs, ys).slope).toBeGreaterThan(0);
    expect(assessDrift(xs, ys).drifting).toBe(false);
  });

  it('les statistiques de serie rendent min, max, moyenne, mediane et ecart-type', () => {
    const s = seriesStats([0, 1, 2, 3, 4]);
    expect(s.min).toBe(0);
    expect(s.max).toBe(4);
    expect(s.mean).toBe(2);
    expect(s.median).toBe(2);
    expect(s.absMax).toBe(4);
    expect(s.stdDev).toBeCloseTo(Math.sqrt(2), 9);
  });
});

describe('driftEstimator — lecture du CSV de la sonde', () => {
  it('relit les colonnes ecrites par drift_probe', () => {
    const csv = [
      'channel,onset_index,step,sub_index,expected_tick,actual_tick,grid_error_ticks,expected_us,actual_us,timing_error_us',
      '1,0,3,0,288,289,1,1500.000,1506.250,6.250',
      '1,1,4,0,384,384,0,2000.000,2001.000,1.000',
    ].join('\n');
    const rows = parseOnsetCsv(csv);
    expect(rows).toHaveLength(2);
    expect(rows[0]!.gridErrorTicks).toBe(1);
    expect(rows[1]!.timingErrorUs).toBeCloseTo(1.0, 6);
    expect(rows[0]!.step).toBe(3);
  });

  it('refuse une ligne incomplete plutot que d inventer une colonne', () => {
    expect(() => parseOnsetCsv('channel,onset_index\n1,0')).toThrow();
  });
});
