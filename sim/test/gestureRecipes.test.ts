import { describe, expect, it } from 'vitest';
import { GestureDriver } from '../src/analysis/gestureRecipes';
import {
  CHANNEL_TAB_FIELDS,
  STEP_COUNT,
  TAB_FIRST_CHANNEL,
  UiController,
  UiEvent,
  UiField,
  UiLevel,
} from '../src/domain/UiController.js';
import { Transport } from '../src/domain/Transport.js';
import { PatternBank } from '../src/domain/PatternBank.js';
import {
  ChannelMode,
  SequencerEngine,
} from '../src/domain/SequencerEngine.js';
import {
  RATCHET_2,
  RATCHET_6,
  RATCHET_NONE,
  RATCHET_TRIPLET,
  ratchetFitsStep,
} from '../src/domain/Pattern.js';

const ACTIVE_STEPS = [0, 5, 9];

function rig(activeSteps: readonly number[] = ACTIVE_STEPS) {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  for (let ch = 0; ch < engine.channelCount(); ++ch) {
    engine.setChannelMode(ch, ChannelMode.SEQ);
  }
  const pattern = engine.instanceForChannel(0)!;
  for (const step of activeSteps) pattern.writeStep(step, true);
  const transport = new Transport(engine);
  const ui = new UiController(engine, transport);
  const driver = new GestureDriver(ui, bank, engine);
  return { bank, engine, transport, ui, driver, pattern };
}

function onsetTimeline(engine: SequencerEngine, channel: number, ticks: number) {
  const events: Array<{ tick: number; step: number }> = [];
  engine.start();
  for (let tick = 1; tick <= ticks; ++tick) {
    engine.advance(1);
    if (engine.onsetCount(channel) > 0) {
      events.push({ tick, step: engine.effectiveStep(channel) });
    }
  }
  return events;
}

function cycleLengthInTicks(engine: SequencerEngine, channel: number, ticks: number): number {
  const events = onsetTimeline(engine, channel, ticks);
  const firstStep = events[0]?.step;
  if (firstStep === undefined) throw new Error('aucun onset observe');
  const occurrences = events.filter((e) => e.step === firstStep).map((e) => e.tick);
  if (occurrences.length < 2) throw new Error('cycle jamais reboucle');
  return occurrences[1]! - occurrences[0]!;
}

describe('couche 1 — recettes de gestes contre le MODELE de reference, jamais contre le firmware', () => {
  describe('navigation', () => {
    it('1. la recette atteint l onglet du channel demande', () => {
      const { ui, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL + 1);
      expect(ui.level).toBe(UiLevel.Tab);
      expect(ui.currentTab).toBe(TAB_FIRST_CHANNEL + 1);
      expect(ui.field).toBe(UiField.Mode);
    });

    it('2. la recette pose le curseur sur le champ demande, en autant de rotations que la distance', () => {
      const { ui, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      const before = driver.events.length;
      driver.selectField(UiField.Subdiv);
      const rotations = driver.events
        .slice(before)
        .filter((g) => g.event === UiEvent.Rotate).length;
      expect(ui.field).toBe(UiField.Subdiv);
      expect(rotations).toBe(3);
      expect(rotations).toBeLessThan(CHANNEL_TAB_FIELDS);
    });

    it('3. un appui long remonte d UN seul niveau', () => {
      const { ui, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.enterEdit();
      expect(ui.level).toBe(UiLevel.Edit);
      driver.backOneLevel();
      expect(ui.level).toBe(UiLevel.Tab);
      driver.backOneLevel();
      expect(ui.level).toBe(UiLevel.TabBar);
    });
  });

  describe('LENGTH — verification B, par le comportement observe et non par une variable', () => {
    it('4. la recette change la PERIODE JOUEE du motif', () => {
      const before = rig();
      const channel = 0;
      const periodBefore = cycleLengthInTicks(before.engine, channel, 96 * 40);
      expect(periodBefore).toBe(16 * 96);

      const after = rig();
      after.driver.goToTab(TAB_FIRST_CHANNEL);
      after.driver.setLength(3);
      const periodAfter = cycleLengthInTicks(after.engine, channel, 96 * 45);
      expect(periodAfter).toBe(19 * 96);
      expect(periodAfter).not.toBe(periodBefore);
    });

    it('5. la recette CLAMPE a la longueur maximale au lieu de produire une valeur illegale', () => {
      const { engine, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.setLength(30);
      const period = cycleLengthInTicks(engine, 0, 96 * 60);
      expect(period).toBe(3456);
    });
  });

  describe('SUBDIV — verification B, par la duree de step observee', () => {
    it('6. la recette change la DUREE DU STEP', () => {
      const { engine, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.setSubdiv(1);
      const events = onsetTimeline(engine, 0, 192 * 30);
      const first = events.find((e) => e.step === 0)!;
      const next = events.find((e) => e.tick > first.tick && e.step === 5)!;
      expect(next.tick - first.tick).toBe(5 * 192);
    });

    it('7. la recette reste dans les 25 cadences legales', () => {
      const { engine, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.setSubdiv(50);
      const events = onsetTimeline(engine, 0, 96 * 128 * 10);
      const first = events.find((e) => e.step === 5)!;
      const next = events.find((e) => e.tick > first.tick && e.step === 9)!;
      expect(next.tick - first.tick).toBe(4 * 96 * 128);
    });
  });

  describe('steps — verification A, disposition garantie par le static_assert de Pattern', () => {
    it('8. la recette bascule le step vise et AUCUN autre', () => {
      const { pattern, driver } = rig();
      const beforeAll = Array.from({ length: STEP_COUNT }, (_, i) => pattern.readStep(i));
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.enterEdit();
      driver.toggleStep(3);
      expect(pattern.readStep(3)).toBe(!beforeAll[3]);
      for (let i = 0; i < STEP_COUNT; ++i) {
        if (i === 3) continue;
        expect(pattern.readStep(i), `step ${i}`).toBe(beforeAll[i]);
      }
    });
  });

  describe('ratchets et triolet — verification A', () => {
    it('9. la recette pose le code demande sur un step ACTIF', () => {
      const { pattern, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.enterEdit();
      const outcome = driver.setRatchet(5, RATCHET_6);
      expect(outcome.applied).toBe(true);
      expect(pattern.getRatchet(5)).toBe(RATCHET_6);
    });

    it('10. sur un step INACTIF la recette ne change rien et le dit', () => {
      const { pattern, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.enterEdit();
      const outcome = driver.setRatchet(4, RATCHET_6);
      expect(outcome.applied).toBe(false);
      expect(outcome.reason).toMatch(/inactif/i);
      expect(pattern.getRatchet(4)).toBe(RATCHET_NONE);
    });

    it('11. la recette SAUTE les codes que la cadence refuse et n en pose jamais un illegal', () => {
      const { engine, pattern, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.setSubdiv(-8);
      driver.enterEdit();

      const ticks = engine.getTicksPerStep(0);
      expect(ticks).toBe(4);
      expect(ratchetFitsStep(RATCHET_6, ticks)).toBe(false);

      const refused = driver.setRatchet(5, RATCHET_6);
      expect(refused.applied).toBe(false);
      expect(refused.reason).toMatch(/cadence|fits/i);
      expect(pattern.getRatchet(5)).toBe(RATCHET_NONE);

      const accepted = driver.setRatchet(5, RATCHET_TRIPLET);
      expect(accepted.applied).toBe(true);
      expect(pattern.getRatchet(5)).toBe(RATCHET_TRIPLET);
      expect(ratchetFitsStep(pattern.getRatchet(5), ticks)).toBe(true);
    });

    it('12. le triolet s active puis se retire', () => {
      const { pattern, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.enterEdit();
      expect(driver.setRatchet(9, RATCHET_TRIPLET).applied).toBe(true);
      expect(pattern.getRatchet(9)).toBe(RATCHET_TRIPLET);
      expect(driver.setRatchet(9, RATCHET_NONE).applied).toBe(true);
      expect(pattern.getRatchet(9)).toBe(RATCHET_NONE);
    });
  });

  describe('composition', () => {
    it('13. aller au channel, changer LENGTH, revenir : etat connu et changement effectif', () => {
      const { engine, ui, driver } = rig();
      const tab = TAB_FIRST_CHANNEL + 2;
      driver.goToTab(tab);
      driver.setLength(3);
      driver.backOneLevel();
      expect(ui.level).toBe(UiLevel.TabBar);
      expect(ui.currentTab).toBe(tab);
      const channel = tab - TAB_FIRST_CHANNEL;
      expect(cycleLengthInTicks(engine, channel, 96 * 45)).toBe(19 * 96);
    });
  });

  describe('crans SHIFT consecutifs — contrat avec le pilote physique', () => {
    function longestShiftRun(driver: GestureDriver): number {
      let best = 0;
      let run = 0;
      for (const gesture of driver.events) {
        if (gesture.event === UiEvent.ShiftRotate) {
          run += 1;
          if (run > best) best = run;
        } else {
          run = 0;
        }
      }
      return best;
    }

    const recipes: Array<[string, number, (d: GestureDriver) => void]> = [
      ['4 LENGTH', 3, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.setLength(3); }],
      ['5 LENGTH au maximum', 30, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.setLength(30); }],
      ['6 SUBDIV', 1, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.setSubdiv(1); }],
      ['7 SUBDIV au bout', 50, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.setSubdiv(50); }],
      ['8 step', 0, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.enterEdit(); d.toggleStep(3); }],
      ['9 ratchet', 4, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.enterEdit(); d.setRatchet(5, RATCHET_6); }],
      ['10 step inactif', 0, (d) => { d.goToTab(TAB_FIRST_CHANNEL); d.enterEdit(); d.setRatchet(4, RATCHET_6); }],
      ['11 cadence x24', 8, (d) => {
        d.goToTab(TAB_FIRST_CHANNEL); d.setSubdiv(-8); d.enterEdit();
        d.setRatchet(5, RATCHET_6); d.setRatchet(5, RATCHET_TRIPLET);
      }],
      ['12 triolet aller-retour', 10, (d) => {
        d.goToTab(TAB_FIRST_CHANNEL); d.enterEdit();
        d.setRatchet(9, RATCHET_TRIPLET); d.setRatchet(9, RATCHET_NONE);
      }],
      ['13 composition', 3, (d) => { d.goToTab(TAB_FIRST_CHANNEL + 2); d.setLength(3); d.backOneLevel(); }],
    ];

    it.each(recipes)('%s demande %i crans SHIFT consecutifs au plus', (_name, expected, play) => {
      const { driver } = rig();
      play(driver);
      expect(longestShiftRun(driver)).toBe(expected);
    });

    it('seules les recettes 5 et 7 depassent les 12 crans que le pilote physique tient sous 750 ms', () => {
      const over = recipes
        .filter(([, detents]) => detents > 12)
        .map(([name]) => name);
      expect(over).toEqual(['5 LENGTH au maximum', '7 SUBDIV au bout']);
    });

    it('11 atteint bien x24, ou le code 6 ne tient pas dans un step', () => {
      const { engine, driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      driver.setSubdiv(-8);
      expect(engine.getTicksPerStep(0)).toBe(4);
      expect(ratchetFitsStep(RATCHET_6, 4)).toBe(false);
      expect(ratchetFitsStep(RATCHET_TRIPLET, 4)).toBe(true);
    });
  });

  describe('ce que la couche 1 NE prouve PAS', () => {
    it('les recettes ne portent que sur le modele : la quadrature, l anti-rebond et les durees de maintien restent a prouver sur le firmware', () => {
      const { driver } = rig();
      driver.goToTab(TAB_FIRST_CHANNEL);
      expect(driver.events.every((g) => Object.values(UiEvent).includes(g.event))).toBe(true);
      expect(driver.events.some((g) => 'holdMs' in g)).toBe(false);
    });
  });
});
