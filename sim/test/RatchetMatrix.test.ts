import { describe, expect, it } from "vitest";
import { PatternBank } from "../src/domain/PatternBank.js";
import {
  ChannelMode,
  SequencerEngine,
  subOnsetTick,
} from "../src/domain/SequencerEngine.js";
import { TriggerSequencer } from "../src/domain/TriggerSequencer.js";
import { Transport } from "../src/domain/Transport.js";
import {
  UiController,
  UiEvent,
  UiField,
  UiLevel,
} from "../src/domain/UiController.js";
import {
  ratchetFitsStep,
  RATCHET_2,
  RATCHET_3,
  RATCHET_4,
  RATCHET_6,
  RATCHET_NONE,
  RATCHET_TRIPLET,
} from "../src/domain/Pattern.js";

// La table des cadences, ecrite en clair. Elle ne passe PAS par subdivToTicks :
// une assertion qui se compare a la fonction qu'elle teste se confirme
// elle-meme.
const SUBDIVS = [
  -24, -16, -12, -8, -6, -4, -3, -2,
  1,
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 24, 32, 64, 128,
];

const STEP_TICKS = [
  4, 6, 8, 12, 16, 24, 32, 48,
  96,
  192, 288, 384, 480, 576, 672, 768, 864, 960, 1056, 1152,
  1536, 2304, 3072, 6144, 12288,
];

const CODES = [RATCHET_2, RATCHET_3, RATCHET_4, RATCHET_6, RATCHET_TRIPLET];

// Nombre de declenchements RELEVE apres le lot 21. Le plancher de MIN_SLOT_TICKS
// remplace l'ancienne condition de divisibilite : SIX couples sur 125 valent 1 au
// lieu de N, contre treize avant, et tous aux trois cadences les plus rapides.
const TRIGGERS: number[][] = [
  [2, 1, 1, 1, 3], // x24 : 4 ticks, seuls R2 et le triolet tiennent
  [2, 3, 1, 1, 3], // x16 : 6 ticks
  [2, 3, 4, 1, 3], // x12 : 8 ticks
  [2, 3, 4, 6, 3], // x8
  [2, 3, 4, 6, 3], // x6
  [2, 3, 4, 6, 3], // x4
  [2, 3, 4, 6, 3], // x3
  [2, 3, 4, 6, 3], // x2
  [2, 3, 4, 6, 3], // /1
  [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3],
  [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3],
  [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3],
  [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3], [2, 3, 4, 6, 3],
];

function rig(subdiv: number, code: number, activeSteps: number[], length = 4) {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  engine.setPatternBank(bank);
  const seq = new TriggerSequencer(bank, engine);
  engine.setChannelMode(0, ChannelMode.SEQ);
  const pattern = bank.getPattern(0)!;
  for (const step of activeSteps) pattern.writeStep(step, true);
  if (code !== RATCHET_NONE) pattern.setRatchet(0, code);
  engine.setEffectiveLength(0, length);
  engine.setSubdiv(0, subdiv);
  engine.refreshTiming();
  return { bank, engine, seq, pattern };
}

// Ecarts en ticks entre declenchements SORTIS, ceux que la sortie recoit.
// TriggerSequencer filtre sur l'activite du pas, le moteur non.
function gaps(r: ReturnType<typeof rig>, ticks: number, max: number): number[] {
  r.engine.start();
  const out: number[] = [];
  let last = 0;
  let seen = 0;
  for (let t = 1; t <= ticks; ++t) {
    r.engine.advance(1);
    r.seq.update();
    if (r.seq.triggerCount(0) === 0) continue;
    if (seen > 0 && out.length < max) out.push(t - last);
    last = t;
    ++seen;
  }
  return out;
}

describe("La cadence — les 25 valeurs", () => {
  it("donne la duree de step documentee, pour chaque cadence", () => {
    for (let i = 0; i < SUBDIVS.length; ++i) {
      const r = rig(SUBDIVS[i]!, RATCHET_NONE, []);
      expect(r.engine.getTicksPerStep(0)).toBe(STEP_TICKS[i]);
    }
  });

  it("fait durer un triolet deux steps, a toutes les cadences", () => {
    for (let i = 0; i < SUBDIVS.length; ++i) {
      const r = rig(SUBDIVS[i]!, RATCHET_TRIPLET, [0]);
      expect(r.engine.currentStepTicks(0)).toBe(STEP_TICKS[i]! * 2);
    }
  });
});

describe("La matrice ratchet x cadence", () => {
  it("est celle qui a ete auditee", () => {
    for (let i = 0; i < SUBDIVS.length; ++i) {
      for (let c = 0; c < CODES.length; ++c) {
        const r = rig(SUBDIVS[i]!, CODES[c]!, [0]);
        expect(r.engine.currentStepTriggers(0)).toBe(TRIGGERS[i]![c]);
      }
    }
  });

  // Compte les couples replies EN INTERROGEANT LE MOTEUR, jamais la table
  // ci-dessus : un compte tire de la table se confirmerait lui-meme.
  it("refuse exactement six couples", () => {
    let folded = 0;
    for (const subdiv of SUBDIVS) {
      for (const code of CODES) {
        const r = rig(subdiv, code, [0]);
        if (r.engine.currentStepTriggers(0) === 1) ++folded;
      }
    }
    expect(folded).toBe(6);
  });

  it("nomme les six couples refuses, et le predicat les refuse aussi", () => {
    const pairs: Array<[number, number]> = [
      [-24, RATCHET_3], [-24, RATCHET_4], [-24, RATCHET_6],
      [-16, RATCHET_4], [-16, RATCHET_6], [-12, RATCHET_6],
    ];
    for (const [subdiv, code] of pairs) {
      const r = rig(subdiv, code, [0]);
      expect(r.engine.currentStepTriggers(0)).toBe(1);
      expect(ratchetFitsStep(code, r.engine.getTicksPerStep(0))).toBe(false);
    }
  });

  it("laisse passer le triolet a toutes les cadences", () => {
    for (const subdiv of SUBDIVS) {
      const r = rig(subdiv, RATCHET_TRIPLET, [0]);
      expect(r.engine.currentStepTriggers(0)).toBe(3);
    }
  });

  it("ne laisse jamais un sous-declenchement deriver d'un tick entier", () => {
    for (const subdiv of SUBDIVS) {
      for (const code of CODES) {
        const r = rig(subdiv, code, [0]);
        const step = r.engine.currentStepTicks(0);
        const triggers = r.engine.currentStepTriggers(0);
        if (triggers <= 1) continue;
        for (let k = 1; k < triggers; ++k) {
          const got = subOnsetTick(step, triggers, k);
          expect(got * triggers).toBeLessThanOrEqual(step * k);
          expect(step * k - got * triggers).toBeLessThan(triggers);
        }
      }
    }
  });
});

describe("Le train de declenchements", () => {
  it("est regulier sans ratchet", () => {
    const r = rig(1, RATCHET_NONE, [0, 1, 2, 3]);
    expect(gaps(r, 96 * 6, 8)).toEqual([96, 96, 96, 96, 96]);
  });

  it("coupe le step en trois avec un ratchet 3 a l'unite", () => {
    const r = rig(1, RATCHET_3, [0, 1, 2, 3]);
    expect(gaps(r, 96 * 6, 9)).toEqual([32, 32, 96, 96, 96, 32, 32, 32, 96]);
  });

  it("emet trois fois sur deux steps avec un triolet a l'unite", () => {
    const r = rig(1, RATCHET_TRIPLET, [0, 1, 2, 3]);
    expect(gaps(r, 96 * 6, 6)).toEqual([64, 64, 96, 96, 96, 64]);
  });

  // A x3 un step vaut 32 ticks, un tiers 10,67. Les tranches font 10, 11 et 11 :
  // somme exacte de 32, aucun tick perdu.
  it("utilise tout le step quand la tranche est inegale", () => {
    const r = rig(-3, RATCHET_3, [0, 1, 2, 3]);
    const observed = gaps(r, 32 * 6, 9);
    expect(observed).toEqual([11, 11, 32, 32, 32, 10, 11, 11, 32]);
    expect(observed[5]! + observed[6]! + observed[7]!).toBe(32);
  });

  it("joue les trois notes d'un triolet a x3", () => {
    const r = rig(-3, RATCHET_TRIPLET, [0]);
    expect(r.engine.currentStepTicks(0)).toBe(64);
    expect(r.engine.currentStepTriggers(0)).toBe(3);
  });
});

describe("Le refus a la saisie", () => {
  function editRig(subdiv: number) {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    engine.setPatternBank(bank);
    const transport = new Transport(engine);
    const ui = new UiController(engine, bank, transport);
    for (let ch = 0; ch < 6; ++ch) engine.setChannelMode(ch, ChannelMode.SEQ);
    engine.setSubdiv(0, subdiv);
    engine.refreshTiming();
    const pattern = bank.getPattern(0)!;
    pattern.writeStep(0, true);

    ui.handle(UiEvent.Press);
    for (let guard = 0; guard < 8; ++guard) {
      if (ui.field === UiField.EditEntry) break;
      ui.handle(UiEvent.Rotate, 1);
    }
    expect(ui.field).toBe(UiField.EditEntry);
    ui.handle(UiEvent.Press);
    expect(ui.level).toBe(UiLevel.Edit);
    return { ui, pattern };
  }

  // A x24 seuls RATCHET_2 et le triolet tiennent : tourner saute R3, R4 et R6.
  it("saute les codes que la cadence refuse", () => {
    const r = editRig(-24);
    r.ui.handle(UiEvent.RotateHeld, 1);
    expect(r.pattern.getRatchet(0)).toBe(RATCHET_2);
    r.ui.handle(UiEvent.RotateHeld, 1);
    expect(r.pattern.getRatchet(0)).toBe(RATCHET_TRIPLET);
  });

  it("n'en saute aucun a l'unite", () => {
    const r = editRig(1);
    for (const code of [RATCHET_2, RATCHET_3, RATCHET_4, RATCHET_6, RATCHET_TRIPLET]) {
      r.ui.handle(UiEvent.RotateHeld, 1);
      expect(r.pattern.getRatchet(0)).toBe(code);
    }
  });
});

describe("Les quatre regles du 2026-08-23", () => {
  it("n'emet rien sur un pas inactif, quel que soit son ratchet", () => {
    for (const code of CODES) {
      const r = rig(1, code, [], 1);
      r.engine.start();
      let fired = 0;
      for (let t = 0; t < 96 * 4; ++t) {
        r.engine.advance(1);
        r.seq.update();
        fired += r.seq.triggerCount(0);
      }
      expect(fired).toBe(0);
    }
  });

  it("traite un triolet inactif comme un silence de deux unites", () => {
    const r = rig(1, RATCHET_TRIPLET, [1, 2, 3]);
    expect(gaps(r, 96 * 12, 6)).toEqual([96, 96, 288, 96, 96, 288]);
  });

  it("garde le ratchet quand le pas est desactive, et le rend avec lui", () => {
    const r = rig(1, RATCHET_NONE, []);
    r.pattern.writeStep(5, true);
    r.pattern.setRatchet(5, RATCHET_6);

    r.pattern.writeStep(5, false);
    expect(r.pattern.getRatchet(5)).toBe(RATCHET_6);

    r.pattern.writeStep(5, true);
    expect(r.pattern.getRatchet(5)).toBe(RATCHET_6);
  });

  it("efface les pas et les ratchets ensemble", () => {
    const r = rig(1, RATCHET_NONE, []);
    r.pattern.writeStep(0, true);
    r.pattern.writeStep(7, true);
    r.pattern.setRatchet(0, RATCHET_4);
    r.pattern.setRatchet(7, RATCHET_TRIPLET);

    r.pattern.clear();

    expect(r.pattern.readStep(0)).toBe(false);
    expect(r.pattern.readStep(7)).toBe(false);
    expect(r.pattern.getRatchet(0)).toBe(RATCHET_NONE);
    expect(r.pattern.getRatchet(7)).toBe(RATCHET_NONE);
  });
});
