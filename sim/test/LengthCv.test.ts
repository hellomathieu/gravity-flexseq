import { describe, it, expect } from "vitest";
import {
  CV_MIN,
  CV_MAX,
  ZONE_WIDTH,
  ZONE_COUNT,
  OFFSET_MIN,
  OFFSET_MAX,
  HYSTERESIS,
  STAY_WIDTH,
  zoneFor,
  zoneWithHysteresis,
  effectiveLengthFor,
  patternIndexFor,
  readStepFor,
} from "../src/domain/LengthCv";

describe("Length CV — constantes du contrat", () => {
  it("porte les valeurs decidees en LCV.2 et LCV.3a", () => {
    expect(CV_MIN).toBe(-512);
    expect(CV_MAX).toBe(512);
    expect(ZONE_WIDTH).toBe(33);
    expect(ZONE_COUNT).toBe(31);
    expect(OFFSET_MIN).toBe(-15);
    expect(OFFSET_MAX).toBe(15);
    expect(HYSTERESIS).toBe(8);
    expect(STAY_WIDTH).toBe(24);
  });
});

describe("Length CV — quantification", () => {
  it("suit le vecteur partage avec le C++", () => {
    expect(zoneFor(-512)).toBe(-15);
    expect(zoneFor(-496)).toBe(-15);
    expect(zoneFor(-495)).toBe(-15);
    expect(zoneFor(-49)).toBe(-1);
    expect(zoneFor(-34)).toBe(-1);
    expect(zoneFor(-17)).toBe(-1);
    expect(zoneFor(-16)).toBe(0);
    expect(zoneFor(0)).toBe(0);
    expect(zoneFor(16)).toBe(0);
    expect(zoneFor(17)).toBe(1);
    expect(zoneFor(33)).toBe(1);
    expect(zoneFor(49)).toBe(1);
    expect(zoneFor(50)).toBe(2);
    expect(zoneFor(478)).toBe(14);
    expect(zoneFor(479)).toBe(15);
    expect(zoneFor(495)).toBe(15);
    expect(zoneFor(512)).toBe(15);
  });

  it("garde les deux zones extremes plus larges d'une unite", () => {
    expect(zoneFor(-479)).toBe(-15);
    expect(zoneFor(-478)).toBe(-14);
    expect(zoneFor(479)).toBe(15);
    expect(zoneFor(478)).toBe(14);
  });

  it("couvre toute la plage avec trente et une zones", () => {
    const seen = new Set<number>();
    for (let cv = -512; cv <= 512; cv += 1) {
      const zone = zoneFor(cv);
      expect(zone).toBeGreaterThanOrEqual(-15);
      expect(zone).toBeLessThanOrEqual(15);
      seen.add(zone);
    }
    expect(seen.size).toBe(31);
  });
});

describe("Length CV — hysteresis", () => {
  it("garde la zone sur la frontiere exacte", () => {
    expect(zoneWithHysteresis(24, 0)).toBe(0);
    expect(zoneWithHysteresis(-24, 0)).toBe(0);
    expect(zoneWithHysteresis(9, 1)).toBe(1);
    expect(zoneWithHysteresis(57, 1)).toBe(1);
  });

  it("cede une unite au-dela de la frontiere", () => {
    expect(zoneWithHysteresis(25, 0)).toBe(1);
    expect(zoneWithHysteresis(-25, 0)).toBe(-1);
    expect(zoneWithHysteresis(8, 1)).toBe(0);
    expect(zoneWithHysteresis(58, 1)).toBe(2);
  });

  it("se mesure depuis le CENTRE de la zone et non depuis son bord", () => {
    expect(zoneWithHysteresis(471, 15)).toBe(15);
    expect(zoneWithHysteresis(470, 15)).toBe(14);
    expect(zoneWithHysteresis(-471, -15)).toBe(-15);
    expect(zoneWithHysteresis(-470, -15)).toBe(-14);
  });

  it("ne cede jamais au bruit mesure de plus ou moins trois", () => {
    for (let zone = -15; zone <= 15; zone += 1) {
      const centre = 33 * zone;
      for (const probe of [centre, centre + 16, centre - 16, centre + 17, centre - 17]) {
        for (let delta = -3; delta <= 3; delta += 1) {
          const cv = Math.max(-512, Math.min(512, probe + delta));
          expect(zoneWithHysteresis(cv, zone)).toBe(zone);
        }
      }
    }
  });

  it("ne recule jamais sur une rampe montante", () => {
    let current = -15;
    let previous = -15;
    for (let cv = -512; cv <= 512; cv += 1) {
      current = zoneWithHysteresis(cv, current);
      expect(current).toBeGreaterThanOrEqual(previous);
      previous = current;
    }
    expect(current).toBe(15);
  });
});

describe("Length CV — longueur effective", () => {
  it("raccourcit et allonge", () => {
    expect(effectiveLengthFor(18, 0)).toBe(18);
    expect(effectiveLengthFor(18, 15)).toBe(33);
    expect(effectiveLengthFor(18, -15)).toBe(3);
    expect(effectiveLengthFor(18, 1)).toBe(19);
    expect(effectiveLengthFor(18, -1)).toBe(17);
  });

  it("sature a un et a trente-six", () => {
    expect(effectiveLengthFor(1, -15)).toBe(1);
    expect(effectiveLengthFor(1, 15)).toBe(16);
    expect(effectiveLengthFor(36, 15)).toBe(36);
    expect(effectiveLengthFor(36, -15)).toBe(21);
    expect(effectiveLengthFor(2, -15)).toBe(1);
    expect(effectiveLengthFor(35, 15)).toBe(36);
  });
});

describe("Pattern CV — index effectif", () => {
  it("deplace l'index", () => {
    expect(patternIndexFor(8, 0)).toBe(8);
    expect(patternIndexFor(8, 1)).toBe(9);
    expect(patternIndexFor(8, -1)).toBe(7);
    expect(patternIndexFor(0, 15)).toBe(15);
    expect(patternIndexFor(15, -15)).toBe(0);
  });

  it("sature a zero et a quinze", () => {
    expect(patternIndexFor(0, -15)).toBe(0);
    expect(patternIndexFor(15, 15)).toBe(15);
    expect(patternIndexFor(1, -15)).toBe(0);
    expect(patternIndexFor(14, 15)).toBe(15);
    expect(patternIndexFor(8, 15)).toBe(15);
    expect(patternIndexFor(8, -15)).toBe(0);
  });
});

describe("Step CV — lecture decalee", () => {
  it("l'offset est absolu, quelle que soit la longueur", () => {
    expect(readStepFor(0, 3, 4)).toBe(3);
    expect(readStepFor(0, 3, 12)).toBe(3);
    expect(readStepFor(0, 3, 36)).toBe(3);
    expect(readStepFor(0, 15, 36)).toBe(15);
    expect(readStepFor(0, 15, 20)).toBe(15);
    expect(readStepFor(0, 15, 16)).toBe(15);
    expect(readStepFor(0, 15, 8)).toBe(7);
    expect(readStepFor(0, 15, 5)).toBe(0);
    expect(readStepFor(7, 10, 8)).toBe(1);
  });

  it("un offset negatif enroule vers l'avant", () => {
    expect(readStepFor(0, -1, 36)).toBe(35);
    expect(readStepFor(0, -15, 36)).toBe(21);
    expect(readStepFor(2, -5, 12)).toBe(9);
    expect(readStepFor(5, -5, 8)).toBe(0);
    expect(readStepFor(0, -12, 12)).toBe(0);
  });

  it("un tour exact vers l'arriere ne rend jamais moins zero", () => {
    expect(Object.is(readStepFor(0, -12, 12), 0)).toBe(true);
    expect(Object.is(readStepFor(0, -30, 2), 0)).toBe(true);
    expect(Object.is(readStepFor(0, -30, 6), 0)).toBe(true);
  });

  it("un offset plus grand que la longueur fait plusieurs tours", () => {
    expect(readStepFor(0, 30, 4)).toBe(2);
    expect(readStepFor(1, 30, 3)).toBe(1);
    expect(readStepFor(0, -30, 2)).toBe(0);
    expect(readStepFor(3, -30, 7)).toBe(1);
  });

  it("la somme de deux sources couvre trente dans les deux sens", () => {
    expect(readStepFor(0, 30, 36)).toBe(30);
    expect(readStepFor(0, -30, 36)).toBe(6);
    expect(readStepFor(35, 30, 36)).toBe(29);
    expect(readStepFor(35, -30, 36)).toBe(5);
  });

  it("une source manque cinq positions a la longueur 36", () => {
    for (const base of [0, 20, 30]) {
      const seen = new Set<number>();
      for (let offset = -15; offset <= 15; ++offset) {
        const got = readStepFor(base, offset, 36);
        expect(got, `base ${base} offset ${offset}`).toBeGreaterThanOrEqual(0);
        expect(got, `base ${base} offset ${offset}`).toBeLessThan(36);
        seen.add(got);
      }
      expect(seen.size, `base ${base}`).toBe(31);
      for (let k = 16; k <= 20; ++k) {
        expect(seen.has((base + k) % 36), `base ${base} manque ${k}`).toBe(false);
      }
    }
  });

  it("deux sources atteignent les trente-six positions", () => {
    const seen = new Set<number>();
    for (let offset = -30; offset <= 30; ++offset) {
      const got = readStepFor(0, offset, 36);
      expect(got, `offset ${offset}`).toBeGreaterThanOrEqual(0);
      expect(got, `offset ${offset}`).toBeLessThan(36);
      seen.add(got);
    }
    expect(seen.size).toBe(36);
  });
});
