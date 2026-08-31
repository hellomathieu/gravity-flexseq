import { describe, it, expect } from "vitest";
import { SequencerEngine } from "../src/domain/SequencerEngine";
import {
  CvDestination,
  CV_DESTINATION_COUNT,
  CV_SOURCE_COUNT,
  CV_SOURCE_1,
  CV_SOURCE_2,
} from "../src/domain/CvDestination";

const STEP = 96; // DEFAULT_SUBDIV = /1

function routed(base: number, source = CV_SOURCE_1): SequencerEngine {
  const e = new SequencerEngine();
  e.setBaseLength(0, base);
  e.setCvDestination(0, source, CvDestination.LENGTH);
  return e;
}

describe("Routage CV — destinations", () => {
  it("porte les cinq codes persistes", () => {
    expect(CvDestination.NONE).toBe(0);
    expect(CvDestination.PATTERN).toBe(1);
    expect(CvDestination.LENGTH).toBe(2);
    expect(CvDestination.RESET).toBe(3);
    expect(CvDestination.STEP).toBe(4);
    expect(CV_DESTINATION_COUNT).toBe(5);
    expect(CV_SOURCE_COUNT).toBe(2);
  });

  it("ne route rien sur un moteur neuf", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < e.channelCount(); ++ch) {
      expect(e.getCvDestination(ch, CV_SOURCE_1)).toBe(CvDestination.NONE);
      expect(e.getCvDestination(ch, CV_SOURCE_2)).toBe(CvDestination.NONE);
      expect(e.lengthCvOffset(ch)).toBe(0);
    }
  });

  it("refuse une destination hors plage sans changer le channel", () => {
    const e = routed(18);
    expect(e.setCvDestination(0, CV_SOURCE_1, 5)).toBe(false);
    expect(e.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.LENGTH);
    expect(e.setCvDestination(0, 2, CvDestination.LENGTH)).toBe(false);
    expect(e.setCvDestination(6, CV_SOURCE_1, CvDestination.LENGTH)).toBe(false);
  });

  it("refuse une source hors plage", () => {
    const e = new SequencerEngine();
    expect(e.setCvInput(CV_SOURCE_1, 330)).toBe(true);
    expect(e.setCvInput(2, 330)).toBe(false);
    expect(e.getCvInput(CV_SOURCE_1)).toBe(330);
    expect(e.getCvInput(2)).toBe(0);
  });
});

describe("Length CV — application", () => {
  it("ne change rien tant qu'aucune frontiere n'est franchie", () => {
    const e = routed(18);
    e.setCvInput(CV_SOURCE_1, 330);
    expect(e.getEffectiveLength(0)).toBe(18);
  });

  it("n'applique rien transport arrete", () => {
    const e = routed(18);
    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP * 4);
    expect(e.getEffectiveLength(0)).toBe(18);
  });

  it("applique a la frontiere de step", () => {
    const e = routed(18);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
  });

  it("relit la valeur poussee a chaque frontiere", () => {
    const e = routed(18);
    e.start();
    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
    e.setCvInput(CV_SOURCE_1, 0);
    expect(e.getEffectiveLength(0)).toBe(28);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(18);
  });

  it("garde un etat d'hysteresis par channel", () => {
    const e = new SequencerEngine();
    e.setBaseLength(0, 18);
    e.setBaseLength(1, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvDestination(1, CV_SOURCE_1, CvDestination.LENGTH);
    e.setSubdiv(1, 2);
    e.start();

    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
    expect(e.getEffectiveLength(1)).toBe(18);

    e.setCvInput(CV_SOURCE_1, 310);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
    expect(e.getEffectiveLength(1)).toBe(27);
  });
});

describe("Length CV — CV1 et CV2", () => {
  it("laisse chaque source piloter seule", () => {
    const e = new SequencerEngine();
    e.setBaseLength(0, 18);
    e.setBaseLength(1, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvDestination(1, CV_SOURCE_2, CvDestination.LENGTH);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, -330);
    e.start();
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
    expect(e.getEffectiveLength(1)).toBe(8);
  });

  it("additionne deux sources sur la meme destination", () => {
    const e = routed(10);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.LENGTH);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, 165);
    e.start();
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(15);
    expect(e.getEffectiveLength(0)).toBe(25);
  });

  it("ecrete UNE fois, apres la somme, et non deux fois", () => {
    const e = routed(36);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.LENGTH);
    e.setCvInput(CV_SOURCE_1, 512);
    e.setCvInput(CV_SOURCE_2, -512);
    e.start();
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(0);
    expect(e.getEffectiveLength(0)).toBe(36); // deux ecretages rendraient 21
  });

  it("ignore une source routee ailleurs", () => {
    const e = routed(18);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.PATTERN);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, 512);
    e.start();
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(28);
  });

  it("laisse un channel non route sur sa base", () => {
    const e = new SequencerEngine();
    e.setBaseLength(0, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP);
    e.setCvInput(CV_SOURCE_1, 512);
    e.setCvInput(CV_SOURCE_2, -512);
    e.start();
    e.advance(STEP * 4);
    expect(e.lengthCvOffset(0)).toBe(0);
    expect(e.getEffectiveLength(0)).toBe(18);
  });
});

describe("Length CV — transitions", () => {
  it("revient a la base a la frontiere suivant le retrait", () => {
    const e = routed(18);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.NONE);
    expect(e.getEffectiveLength(0)).toBe(28);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(18);
  });

  it("remet a zero l'hysteresis de la source qui change de destination", () => {
    const e = routed(18);
    e.start();
    e.setCvInput(CV_SOURCE_1, 330);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);

    e.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvInput(CV_SOURCE_1, 310);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(27); // 9, et non 10 garde
  });

  it("ne remet pas les zones a zero sur STOP puis PLAY", () => {
    const e = routed(18);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
    e.stop();
    e.reset();
    e.start();
    e.setCvInput(CV_SOURCE_1, 310);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(28);
  });

  it("ne replie jamais le playhead sur un RESET", () => {
    const e = routed(30);
    e.setCvInput(CV_SOURCE_1, -512);
    e.start();
    e.advance(STEP * 6);
    expect(e.getEffectiveLength(0)).toBe(15);
    e.reset();
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.getEffectiveLength(0)).toBe(15);
  });
});

describe("Length CV — invariants", () => {
  it("preserve l'etat CV au travers d'un reset", () => {
    const e = routed(18);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    e.reset();
    expect(e.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.LENGTH);
    expect(e.lengthCvOffset(0)).toBe(10);
  });

  it("ne replie le playhead qu'une fois par franchissement", () => {
    const e = routed(30);
    e.start();
    e.advance(STEP * 26);
    expect(e.effectiveStep(0)).toBe(26);
    e.setCvInput(CV_SOURCE_1, -512);
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(15);
    expect(e.effectiveStep(0)).toBe(27 % 15);
  });
});
