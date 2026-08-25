import { describe, it, expect } from 'vitest';
import { reconcileTimeline } from '../src/analysis/reconcile';

describe('reconcileTimeline — les cas qui ont fait decrocher l analyseur', () => {
  it('1. deux onsets proches, aucun perdu, le second en retard', () => {
    const r = reconcileTimeline([0, 10], [1, 12]);
    expect(r.expected).toBe(2);
    expect(r.observed).toBe(2);
    expect(r.rawDifference).toBe(0);
    expect(r.matched).toBe(2);
    expect(r.missing).toBe(0);
    expect(r.extra).toBe(0);
    expect(r.ambiguous).toBe(0);
    expect(r.late).toBe(2);
    expect(r.maxDelayTicks).toBe(2);
  });

  it('2. deux onsets proches, le second reellement perdu', () => {
    const r = reconcileTimeline([0, 10], [1]);
    expect(r.rawDifference).toBe(1);
    expect(r.missing).toBe(1);
    expect(r.extra).toBe(0);
    expect(r.matched).toBe(1);
    expect(r.missingPositions).toEqual([1]);
    expect(r.ambiguous).toBe(0);
  });

  it('3. trois onsets proches, celui du milieu perdu', () => {
    const sansBorne = reconcileTimeline([0, 10, 20], [1, 21]);
    expect(sansBorne.rawDifference).toBe(1);
    expect(sansBorne.missing).toBe(1);
    expect(sansBorne.matched).toBe(2);
    expect(sansBorne.ambiguous).toBe(1);
    expect(sansBorne.ambiguousRange).toEqual([1, 2]);

    const avecBorne = reconcileTimeline([0, 10, 20], [1, 21], { maxDelayTicks: 5 });
    expect(avecBorne.missing).toBe(1);
    expect(avecBorne.missingPositions).toEqual([1]);
    expect(avecBorne.ambiguous).toBe(0);
  });

  it('4. plusieurs onsets retardes mais tous emis', () => {
    const r = reconcileTimeline([0, 10, 20, 30], [5, 16, 27, 38]);
    expect(r.missing).toBe(0);
    expect(r.extra).toBe(0);
    expect(r.matched).toBe(4);
    expect(r.late).toBe(4);
    expect(r.maxDelayTicks).toBe(8);
    expect(r.deficitReturnsToZero).toBe(true);
  });

  it('5. un retard SUPERIEUR a l intervalle ne fabrique ni perte ni evenement en trop', () => {
    const r = reconcileTimeline([0, 10, 20], [15, 25, 35]);
    expect(r.missing).toBe(0);
    expect(r.extra).toBe(0);
    expect(r.matched).toBe(3);
    expect(r.maxDelayTicks).toBe(15);
    expect(r.maxDeficit).toBeGreaterThan(0);
    expect(r.deficitReturnsToZero).toBe(true);
  });

  it('6. deux appariements possibles : le resultat est AMBIGU, pas un choix silencieux', () => {
    const r = reconcileTimeline([0, 10], [12]);
    expect(r.rawDifference).toBe(1);
    expect(r.missing).toBe(1);
    expect(r.ambiguous).toBe(1);
    expect(r.missingPositions).toEqual([]);
    expect(r.ambiguousRange).toEqual([0, 1]);
  });

  it('un front en trop est compte comme tel, jamais comme un onset apparie', () => {
    const r = reconcileTimeline([0, 10], [1, 5, 11]);
    expect(r.rawDifference).toBe(-1);
    expect(r.extra).toBe(1);
    expect(r.missing).toBe(0);
  });

  it('le comptage brut reste disponible meme quand l appariement est ambigu', () => {
    const r = reconcileTimeline([0, 10, 20, 30], [12, 22]);
    expect(r.expected).toBe(4);
    expect(r.observed).toBe(2);
    expect(r.rawDifference).toBe(2);
    expect(r.ambiguous).toBeGreaterThan(0);
  });

  it('une dette qui se resorbe laisse un deficit final nul', () => {
    const r = reconcileTimeline([0, 1, 2, 3], [4, 5, 6, 7]);
    expect(r.finalDeficit).toBe(0);
    expect(r.maxDeficit).toBe(4);
    expect(r.deficitReturnsToZero).toBe(true);
    expect(r.missing).toBe(0);
  });

  it('une dette qui ne se resorbe pas laisse un deficit final positif', () => {
    const r = reconcileTimeline([0, 1, 2, 3], [4, 5]);
    expect(r.finalDeficit).toBe(2);
    expect(r.deficitReturnsToZero).toBe(false);
    expect(r.missing).toBe(2);
  });

  it('la dette MAXIMALE est le pic, pas la derniere valeur', () => {
    const r = reconcileTimeline([0, 1, 2, 10], [3, 4, 5, 11]);
    expect(r.maxDeficit).toBe(3);
    expect(r.finalDeficit).toBe(0);
    expect(r.missing).toBe(0);
  });

  it('un onset ne peut pas etre emis avant son tick', () => {
    const r = reconcileTimeline([10, 20], [5, 21]);
    expect(r.extra).toBeGreaterThan(0);
  });
});
