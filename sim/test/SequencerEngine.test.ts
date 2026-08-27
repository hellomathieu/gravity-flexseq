import { Transport } from "../src/domain/Transport.js";
import { describe, expect, it } from "vitest";
import {
  SequencerEngine,
  PPQN,
  DEFAULT_LENGTH,
  CHANNEL_COUNT,
  ChannelMode,
  MAX_SKIP_CHANCE,
} from "../src/domain/SequencerEngine.js";
import { PatternBank } from "../src/domain/PatternBank.js";
import { RATCHET_3, RATCHET_4, RATCHET_6, RATCHET_TRIPLET } from "../src/domain/Pattern.js";

const STEP = PPQN; // 96 ticks = default ticksPerStep (/1 = noire)

describe("SequencerEngine — transport & masterPhase", () => {
  it("starts stopped at phase 0 with default per-channel state", () => {
    const e = new SequencerEngine();
    expect(e.masterPhase).toBe(0);
    expect(e.isRunning).toBe(false);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      expect(e.getEffectiveLength(ch)).toBe(DEFAULT_LENGTH);
      expect(e.effectiveStep(ch)).toBe(0);
    }
  });

  it("does not advance while stopped", () => {
    const e = new SequencerEngine();
    e.advance(STEP);
    expect(e.masterPhase).toBe(0);
  });

  it("advances by ticks only while running", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(); // +1 tick default
    expect(e.masterPhase).toBe(1);
    e.advance(STEP);
    expect(e.masterPhase).toBe(1 + STEP);
  });

  it("stop() preserves the phase; advance after stop is a no-op", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 3);
    e.stop();
    expect(e.masterPhase).toBe(STEP * 3);
    e.advance(STEP);
    expect(e.masterPhase).toBe(STEP * 3);
  });

  it("reset() zeroes the phase without changing running state", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 5);
    e.reset();
    expect(e.masterPhase).toBe(0);
    expect(e.isRunning).toBe(true);
  });

  it("ignores negative or non-integer tick advances", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(-4);
    e.advance(1.5);
    expect(e.masterPhase).toBe(0);
  });
});

describe("SequencerEngine — effectiveStep derivation", () => {
  it("derives the step as floor(phase / ticksPerStep) % effectiveLength", () => {
    const e = new SequencerEngine();
    e.start();
    expect(e.effectiveStep(0)).toBe(0);

    e.advance(STEP); // one full step
    expect(e.effectiveStep(0)).toBe(1);

    e.advance(STEP - 1); // still within step 1 until the boundary
    expect(e.effectiveStep(0)).toBe(1);

    e.advance(1); // crosses to step 2
    expect(e.effectiveStep(0)).toBe(2);
  });

  it("wraps effectiveStep at effectiveLength", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(0, 16);
    e.advance(STEP * 16); // one full loop of 16 steps
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(STEP); // step 1 again
    expect(e.effectiveStep(0)).toBe(1);
  });

  it("keeps masterPhase untouched when LENGTH changes", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 10);
    const before = e.masterPhase;

    expect(e.setEffectiveLength(0, 4)).toBe(true);
    expect(e.masterPhase).toBe(before);

    expect(e.setEffectiveLength(0, 24)).toBe(true);
    expect(e.masterPhase).toBe(before);
  });

  it("does NOT jump the playhead when LENGTH shrinks but stays within range", () => {
    // Regression : anciennement effectiveStep = absStep % length sautait meme
    // quand localStep < newLength. Desormais (phase locale lissee) : conserve.
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 5); // localStep = 5, length 16
    expect(e.effectiveStep(0)).toBe(5);

    expect(e.setEffectiveLength(0, 11)).toBe(true);
    expect(e.effectiveStep(0)).toBe(5); // pas de saut (5 < 11)

    expect(e.setEffectiveLength(0, 8)).toBe(true);
    expect(e.effectiveStep(0)).toBe(5); // pas de saut (5 < 8)
  });

  it("folds the playhead into range only when LENGTH drops at/below it", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP * 13); // localStep = 13
    expect(e.setEffectiveLength(0, 11)).toBe(true);
    expect(e.effectiveStep(0)).toBe(13 % 11); // 2, replie car hors bornes
  });

  it("keeps the current step when LENGTH grows", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(0, 8);
    e.advance(STEP * 3); // localStep = 3
    expect(e.setEffectiveLength(0, 24)).toBe(true);
    expect(e.effectiveStep(0)).toBe(3); // inchange
    e.advance(STEP); // continue sans saut
    expect(e.effectiveStep(0)).toBe(4);
  });

  it("global reset realigns all channels to step 0", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(1, 3);
    e.advance(STEP * 7);
    expect(e.effectiveStep(0)).toBeGreaterThan(0);
    e.reset();
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.effectiveStep(1)).toBe(0);
    expect(e.masterPhase).toBe(0);
  });

  it("rejects invalid effectiveLength without mutation", () => {
    const e = new SequencerEngine();
    expect(e.setEffectiveLength(0, 12)).toBe(true);
    expect(e.setEffectiveLength(0, 0)).toBe(false);
    expect(e.setEffectiveLength(0, 25)).toBe(false);
    expect(e.getEffectiveLength(0)).toBe(12);
  });

  it("isolates execution state between channels (same master phase)", () => {
    const e = new SequencerEngine();
    e.start();
    e.setEffectiveLength(0, 16);
    e.setEffectiveLength(1, 3);
    e.advance(STEP * 4);

    expect(e.effectiveStep(0)).toBe(4 % 16); // 4
    expect(e.effectiveStep(1)).toBe(4 % 3); // 1
    expect(e.masterPhase).toBe(STEP * 4);
  });

  it("supports different per-channel ticksPerStep from one master phase", () => {
    const e = new SequencerEngine();
    e.start();
    e.setTicksPerStep(1, STEP * 2); // channel 1 advances half as fast
    e.advance(STEP * 4);

    expect(e.effectiveStep(0)).toBe(4); // 96/24 = 4 steps
    expect(e.effectiveStep(1)).toBe(2); // 96/48 = 2 steps
  });

  it("rejects invalid channel indices and ticksPerStep", () => {
    const e = new SequencerEngine();
    expect(e.effectiveStep(6)).toBe(-1);
    expect(e.setEffectiveLength(6, 8)).toBe(false);
    expect(e.setTicksPerStep(0, 0)).toBe(false);
  });
});

describe("SequencerEngine — per-channel selected pattern", () => {
  it("defaults every channel to pattern 0", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) expect(e.getSelectedPattern(ch)).toBe(0);
  });

  it("sets and reads a channel's selected pattern independently", () => {
    const e = new SequencerEngine();
    expect(e.setSelectedPattern(0, 3)).toBe(true);
    expect(e.setSelectedPattern(1, 10)).toBe(true);
    expect(e.getSelectedPattern(0)).toBe(3);
    expect(e.getSelectedPattern(1)).toBe(10);
    expect(e.getSelectedPattern(2)).toBe(0);
  });

  it("rejects out-of-range pattern indices and channels", () => {
    const e = new SequencerEngine();
    expect(e.setSelectedPattern(0, -1)).toBe(false);
    expect(e.setSelectedPattern(0, 16)).toBe(false);
    expect(e.setSelectedPattern(6, 0)).toBe(false);
    expect(e.getSelectedPattern(6)).toBe(-1);
  });
});

describe("SequencerEngine — hasStepped (onset)", () => {
  it("reports boundary crossings for the last advance()", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(STEP - 1);
    expect(e.hasStepped(0)).toBe(false);
    e.advance(1); // crosses the first boundary
    expect(e.hasStepped(0)).toBe(true);
    e.advance(1); // within the step
    expect(e.hasStepped(0)).toBe(false);
  });

  it("is false while stopped and for an invalid channel", () => {
    const e = new SequencerEngine();
    e.advance(STEP);
    expect(e.hasStepped(0)).toBe(false);
    expect(e.hasStepped(6)).toBe(false);
  });

  it("is per-channel with different ticksPerStep", () => {
    const e = new SequencerEngine();
    e.start();
    e.setTicksPerStep(1, STEP * 2);
    e.advance(STEP);
    expect(e.hasStepped(0)).toBe(true);
    expect(e.hasStepped(1)).toBe(false);
    e.advance(STEP);
    expect(e.hasStepped(1)).toBe(true);
  });
});

describe("SequencerEngine — SUBDIV", () => {
  it("defaults each channel to SUBDIV /1 (quarter = 96 ticks)", () => {
    const e = new SequencerEngine();
    expect(e.getSubdiv(0)).toBe(1);
    expect(e.getTicksPerStep(0)).toBe(96);
  });

  it("setSubdiv updates the step rate", () => {
    const e = new SequencerEngine();
    e.start();
    expect(e.setSubdiv(0, -4)).toBe(true); // 1/16 = 24 ticks
    expect(e.getTicksPerStep(0)).toBe(24);
    e.advance(24);
    expect(e.effectiveStep(0)).toBe(1);
  });

  it("supports a different SUBDIV per channel (different rates)", () => {
    const e = new SequencerEngine();
    e.start();
    e.setSubdiv(0, -4); // 1/16 -> 24 ticks
    e.setSubdiv(1, 1); //  1/4  -> 96 ticks
    e.advance(96);
    expect(e.effectiveStep(0)).toBe(4); // 96/24 = 4 steps
    expect(e.effectiveStep(1)).toBe(1); // 96/96 = 1 step
  });

  it("rejects invalid subdiv and channel", () => {
    const e = new SequencerEngine();
    expect(e.setSubdiv(0, 0)).toBe(false);
    expect(e.setSubdiv(6, 1)).toBe(false);
    expect(e.getSubdiv(6)).toBe(0);
  });
});

describe("SequencerEngine — ratchets", () => {
  function rig() {
    const bank = new PatternBank();
    const e = new SequencerEngine();
    for (let ch = 0; ch < e.channelCount(); ++ch) e.setChannelMode(ch, ChannelMode.SEQ);
    e.setPatternBank(bank);
    e.setSelectedPattern(0, 0);
    return { bank, e };
  }

  it("un step simple emet un declenchement par step", () => {
    const { e } = rig();
    e.start();
    expect(e.currentStepTicks(0)).toBe(96);
    expect(e.currentStepTriggers(0)).toBe(1);
    e.advance(96);
    expect(e.onsetCount(0)).toBe(1);
    expect(e.hasStepped(0)).toBe(true);
  });

  it("un ratchet emet N declenchements SANS changer la duree du step", () => {
    const { bank, e } = rig();
    bank.getPattern(0)!.setRatchet(1, RATCHET_3);
    e.start();

    e.advance(96); // -> step 1
    expect(e.currentStepTriggers(0)).toBe(3);
    expect(e.currentStepTicks(0)).toBe(96); // duree inchangee

    e.advance(32);
    expect(e.onsetCount(0)).toBe(1);
    expect(e.effectiveStep(0)).toBe(1); // toujours le meme step
    e.advance(32);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(32);
    expect(e.effectiveStep(0)).toBe(2);
  });

  it("un ratchet ne change pas la duree totale du pattern", () => {
    const { bank, e } = rig();
    e.setEffectiveLength(0, 4);
    bank.getPattern(0)!.setRatchet(0, RATCHET_6);
    e.start();
    e.advance(96 * 4);
    expect(e.effectiveStep(0)).toBe(0); // boucle complete
  });

  it("compte tous les declenchements d'un advance groupe", () => {
    const { bank, e } = rig();
    bank.getPattern(0)!.setRatchet(1, RATCHET_4);
    e.start();
    e.advance(96);
    e.advance(96); // tout le step ratchet en une passe
    expect(e.onsetCount(0)).toBe(4);
  });

  it("le triolet etire le step sur DEUX unites", () => {
    const { bank, e } = rig();
    bank.getPattern(0)!.setRatchet(1, RATCHET_TRIPLET);
    e.start();
    e.advance(96); // -> step 1
    expect(e.currentStepTicks(0)).toBe(192);
    expect(e.currentStepTriggers(0)).toBe(3);
    e.advance(64);
    expect(e.onsetCount(0)).toBe(1);
    expect(e.effectiveStep(0)).toBe(1);
    e.advance(64);
    e.advance(64);
    expect(e.effectiveStep(0)).toBe(2);
  });

  it("le triolet decale la suite du pattern", () => {
    const plainBank = new PatternBank();
    const tripletBank = new PatternBank();
    tripletBank.getPattern(0)!.setRatchet(0, RATCHET_TRIPLET);

    const plain = new SequencerEngine();
    const stretched = new SequencerEngine();
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      plain.setChannelMode(ch, ChannelMode.SEQ);
      stretched.setChannelMode(ch, ChannelMode.SEQ);
    }
    plain.setPatternBank(plainBank);
    stretched.setPatternBank(tripletBank);
    plain.start();
    stretched.start();
    plain.advance(96 * 3);
    stretched.advance(96 * 3);
    expect(plain.effectiveStep(0)).toBe(3);
    expect(stretched.effectiveStep(0)).toBe(2); // un step de retard
  });

  // PRD 6.3.1 : un sous-slot n'a plus a tomber sur un tick entier, il doit valoir
  // au moins MIN_SLOT_TICKS.
  it("joue le ratchet meme quand le sous-slot n'est pas un tick entier", () => {
    const { bank, e } = rig();
    bank.getPattern(0)!.setRatchet(0, RATCHET_3);
    expect(e.setSubdiv(0, -3)).toBe(true); // 32 ticks : un tiers = 10,67
    expect(e.currentStepTriggers(0)).toBe(3);
  });

  it("refuse le ratchet quand la tranche tomberait sous deux ticks", () => {
    const { bank, e } = rig();
    bank.getPattern(0)!.setRatchet(0, RATCHET_6);
    expect(e.setSubdiv(0, -12)).toBe(true); // 8 ticks : un sixieme = 1,33
    expect(e.currentStepTriggers(0)).toBe(1);
  });

  it("sans banque, tous les steps sont simples", () => {
    const bank = new PatternBank();
    bank.getPattern(0)!.setRatchet(0, RATCHET_6);
    const e = new SequencerEngine(); // pas de setPatternBank
    e.start();
    expect(e.currentStepTriggers(0)).toBe(1);
    e.advance(96);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("les ratchets ne decalent pas masterPhase", () => {
    const { bank, e } = rig();
    bank.getPattern(0)!.setRatchet(0, RATCHET_TRIPLET);
    e.start();
    e.advance(192);
    expect(e.masterPhase).toBe(192);
  });
});

describe("SequencerEngine — separation de mesure (graphique)", () => {
  it("vaut 4 par defaut et accepte le jeu autorise", () => {
    const e = new SequencerEngine();
    expect(e.getBarLength(0)).toBe(4);
    for (const n of [0, 2, 3, 4, 6]) expect(e.setBarLength(0, n)).toBe(true);
    expect(e.getBarLength(0)).toBe(6);
  });

  it("rejette une valeur qui ne divise pas 12", () => {
    const e = new SequencerEngine();
    expect(e.setBarLength(0, 5)).toBe(false);
    expect(e.setBarLength(0, 8)).toBe(false);
    expect(e.setBarLength(6, 4)).toBe(false);
    expect(e.getBarLength(6)).toBe(-1);
  });

  it("n'a AUCUN effet sur le temps", () => {
    const a = new SequencerEngine();
    const b = new SequencerEngine();
    b.setBarLength(0, 3);
    a.start();
    b.start();
    a.advance(96 * 5);
    b.advance(96 * 5);
    expect(b.effectiveStep(0)).toBe(a.effectiveStep(0));
    expect(b.masterPhase).toBe(a.masterPhase);
  });
});

describe("SequencerEngine — modes de channel (PRD 4.2)", () => {
  it("demarre avec les six channels en CLOCK, sans offset ni chance de saut", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      expect(e.getChannelMode(ch)).toBe(ChannelMode.CLOCK);
      expect(e.getOffset(ch)).toBe(0);
      expect(e.getSkipChance(ch)).toBe(0);
    }
  });

  it("refuse un mode inconnu et un channel inconnu", () => {
    const e = new SequencerEngine();
    expect(e.setChannelMode(0, 3 as ChannelMode)).toBe(false);
    expect(e.setChannelMode(CHANNEL_COUNT, ChannelMode.SEQ)).toBe(false);
    expect(e.getChannelMode(0)).toBe(ChannelMode.CLOCK);
  });

  it("CLOCK emet un declenchement par step a offset 0", () => {
    const e = new SequencerEngine();
    e.start();
    e.advance(95);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(STEP);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("CLOCK declenche au pulse offset, pas a la frontiere", () => {
    const e = new SequencerEngine();
    expect(e.setOffset(0, 10)).toBe(true);
    e.start();
    e.advance(9);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(86);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(10);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("CLOCK garde un declenchement par step quel que soit l'offset", () => {
    for (let offset = 0; offset < STEP; offset += 7) {
      const e = new SequencerEngine();
      e.setOffset(0, offset);
      e.start();
      let total = 0;
      for (let tick = 0; tick < STEP * 10; ++tick) {
        e.advance(1);
        total += e.onsetCount(0);
      }
      expect(total).toBe(10);
    }
  });

  it("CLOCK compte chaque franchissement dans un advance groupe", () => {
    const e = new SequencerEngine();
    e.setOffset(0, 10);
    e.start();
    e.advance(200);
    expect(e.onsetCount(0)).toBe(2);
  });

  it("CLOCK et RANDOM ignorent les ratchets", () => {
    const bank = new PatternBank();
    bank.getPattern(0)!.setRatchet(0, RATCHET_4);
    bank.getPattern(0)!.setRatchet(1, RATCHET_TRIPLET);

    const e = new SequencerEngine();
    e.setPatternBank(bank);
    e.setChannelMode(1, ChannelMode.RANDOM);
    e.start();

    expect(e.currentStepTriggers(0)).toBe(1);
    expect(e.currentStepTriggers(1)).toBe(1);
    e.advance(STEP);
    expect(e.onsetCount(0)).toBe(1);
    expect(e.onsetCount(1)).toBe(1);
    expect(e.currentStepTicks(0)).toBe(STEP);
  });

  it("RANDOM declenche sur la frontiere de step, offset ignore", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.RANDOM);
    e.setOffset(0, 10);
    e.start();
    e.advance(10);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(86);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("l'offset est ecrete au step et suit la cadence", () => {
    const e = new SequencerEngine();
    expect(e.setOffset(0, 500)).toBe(true);
    expect(e.getOffset(0)).toBe(95);
    e.setSubdiv(0, 2);
    expect(e.getOffset(0)).toBe(95);
    e.setOffset(0, 150);
    e.setSubdiv(0, -4);
    expect(e.getOffset(0)).toBe(23);
    e.setTicksPerStep(0, 8);
    expect(e.getOffset(0)).toBe(7);
  });

  it("un offset egal au step est ramene dedans", () => {
    const e = new SequencerEngine();
    e.setOffset(0, STEP);
    expect(e.getOffset(0)).toBe(STEP - 1);
    e.start();
    let total = 0;
    for (let tick = 0; tick < STEP * 4; ++tick) {
      e.advance(1);
      total += e.onsetCount(0);
    }
    expect(total).toBe(4);
  });

  it("la chance de saut est bornee a dix dixiemes", () => {
    const e = new SequencerEngine();
    expect(e.setSkipChance(0, MAX_SKIP_CHANCE)).toBe(true);
    expect(e.getSkipChance(0)).toBe(MAX_SKIP_CHANCE);
    expect(e.setSkipChance(0, MAX_SKIP_CHANCE + 1)).toBe(false);
    expect(e.getSkipChance(0)).toBe(MAX_SKIP_CHANCE);
    expect(e.setSkipChance(CHANNEL_COUNT, 3)).toBe(false);
  });

  it("repasser en SEQ relit le pattern", () => {
    const bank = new PatternBank();
    bank.getPattern(0)!.setRatchet(0, RATCHET_4);
    const e = new SequencerEngine();
    e.setPatternBank(bank);
    expect(e.currentStepTriggers(0)).toBe(1);
    e.setChannelMode(0, ChannelMode.SEQ);
    expect(e.currentStepTriggers(0)).toBe(4);
    e.setChannelMode(0, ChannelMode.CLOCK);
    expect(e.currentStepTriggers(0)).toBe(1);
  });
});

describe("Transport", () => {
  it("rapporte l etat de marche", () => {
    const engine = new SequencerEngine();
    const transport = new Transport(engine);
    expect(transport.isRunning()).toBe(false);
    transport.start();
    expect(transport.isRunning()).toBe(true);
    transport.stop();
    expect(transport.isRunning()).toBe(false);
    transport.resume();
    expect(transport.isRunning()).toBe(true);
  });
});

describe("les instances par canal", () => {
  it("existent pour les six canaux", () => {
    const engine = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) expect(engine.instanceForChannel(ch)).not.toBeNull();
    expect(CHANNEL_COUNT).toBe(6);
  });

  it("n existent pas pour un canal invalide", () => {
    const engine = new SequencerEngine();
    expect(engine.instanceForChannel(6)).toBeNull();
    expect(engine.instanceForChannel(255)).toBeNull();
    expect(engine.instanceForChannel(-1)).toBeNull();
  });

  it("sont six objets distincts", () => {
    const engine = new SequencerEngine();
    for (let a = 0; a < 6; ++a) {
      for (let b = 0; b < 6; ++b) {
        if (a === b) continue;
        expect(engine.instanceForChannel(a)).not.toBe(engine.instanceForChannel(b));
      }
    }
  });

  it("ne fuient pas d un canal a l autre", () => {
    const engine = new SequencerEngine();
    engine.instanceForChannel(0)!.writeStep(3, true);
    engine.instanceForChannel(0)!.setRatchet(3, RATCHET_3);
    for (let ch = 1; ch < 6; ++ch) {
      const other = engine.instanceForChannel(ch)!;
      for (let step = 0; step < 36; ++step) {
        expect(other.readStep(step)).toBe(false);
        expect(other.getRatchet(step)).toBe(0);
      }
    }
  });

  it("portent chacune son propre contenu", () => {
    const engine = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) engine.instanceForChannel(ch)!.writeStep(ch, true);
    for (let ch = 0; ch < 6; ++ch) {
      for (let step = 0; step < 6; ++step) {
        expect(engine.instanceForChannel(ch)!.readStep(step)).toBe(step === ch);
      }
    }
  });

  it("sont independantes de la banque", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    engine.setPatternBank(bank);
    engine.setSelectedPattern(0, 4);

    engine.instanceForChannel(0)!.writeStep(7, true);
    expect(bank.getPattern(4)!.readStep(7)).toBe(false);

    bank.getPattern(4)!.writeStep(9, true);
    expect(engine.instanceForChannel(0)!.readStep(9)).toBe(false);
  });

  it("restent separees quand deux canaux visent le meme template", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    engine.setPatternBank(bank);
    expect(engine.setSelectedPattern(0, 2)).toBe(true);
    expect(engine.setSelectedPattern(1, 2)).toBe(true);
    engine.instanceForChannel(0)!.writeStep(5, true);
    expect(engine.instanceForChannel(1)!.readStep(5)).toBe(false);
  });

  it("ne sont pas encore la source de patternForChannel", () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    engine.setPatternBank(bank);
    engine.setSelectedPattern(0, 0);
    expect(engine.patternForChannel(0)).toBe(bank.getPattern(0));
    expect(engine.patternForChannel(0)).not.toBe(engine.instanceForChannel(0));
  });
});
