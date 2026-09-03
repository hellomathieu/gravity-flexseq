import { describe, it, expect } from "vitest";
import { SequencerEngine, ChannelMode } from "../src/domain/SequencerEngine";
import { TriggerSequencer } from "../src/domain/TriggerSequencer";
import { RATCHET_6, RATCHET_TRIPLET } from "../src/domain/Pattern";
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
  e.setChannelMode(0, ChannelMode.SEQ);
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
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setChannelMode(1, ChannelMode.SEQ);
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
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setChannelMode(1, ChannelMode.SEQ);
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
    e.setChannelMode(0, ChannelMode.SEQ);
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


describe("PATTERN CV — l'index effectif", () => {
  function routedPattern(selected: number, source = CV_SOURCE_1): SequencerEngine {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setSelectedPattern(0, selected);
    e.setCvDestination(0, source, CvDestination.PATTERN);
    return e;
  }

  it("rend le pattern selectionne quand rien n'est route", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) expect(e.patternCvIndex(ch)).toBe(0);
    e.setSelectedPattern(2, 9);
    expect(e.patternCvIndex(2)).toBe(9);
    expect(e.patternCvIndex(6)).toBe(-1);
  });

  it("ne bouge pas sur une valeur poussee seule", () => {
    const e = routedPattern(3);
    e.setCvInput(CV_SOURCE_1, 330);
    expect(e.patternCvIndex(0)).toBe(3);
  });

  it("bouge a la frontiere de step", () => {
    const e = routedPattern(3);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(13);
  });

  it("ne mute jamais le pattern selectionne", () => {
    const e = routedPattern(10);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(15);
    expect(e.getSelectedPattern(0)).toBe(10);
  });

  it("ecrete une seule fois pour deux sources", () => {
    const e = routedPattern(15);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.PATTERN);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, -330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(15);
  });

  it("ignore une source routee vers la longueur", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setSelectedPattern(0, 3);
    e.setBaseLength(0, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(3);
    expect(e.getEffectiveLength(0)).toBe(28);
  });

  it("ne touche pas la longueur quand la source vise le pattern", () => {
    const e = routedPattern(3);
    e.setBaseLength(0, 18);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(13);
    expect(e.getEffectiveLength(0)).toBe(18);
    expect(e.lengthCvOffset(0)).toBe(0);
  });

  it("revient a l'index de base quand le routage part", () => {
    const e = routedPattern(3);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(13);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.NONE);
    expect(e.patternCvIndex(0)).toBe(3);
  });
});


describe("Changement de mode — ce qu'il laisse intact", () => {
  const COURSE = [ChannelMode.CLOCK, ChannelMode.SEQ, ChannelMode.RANDOM, ChannelMode.SEQ];

  it("conserve le routage", () => {
    const e = new SequencerEngine();
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.PATTERN);
    for (const mode of COURSE) {
      expect(e.setChannelMode(0, mode)).toBe(true);
      expect(e.getChannelMode(0)).toBe(mode);
      expect(e.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.LENGTH);
      expect(e.getCvDestination(0, CV_SOURCE_2)).toBe(CvDestination.PATTERN);
    }
  });

  it("conserve les bases", () => {
    const e = new SequencerEngine();
    e.setBaseLength(0, 18);
    e.setSelectedPattern(0, 7);
    for (const mode of COURSE) {
      expect(e.setChannelMode(0, mode)).toBe(true);
      expect(e.getBaseLength(0)).toBe(18);
      expect(e.getSelectedPattern(0)).toBe(7);
    }
  });

});

describe("Gating par mode (E3.7-F2)", () => {
  it("laisse la base a un routage LENGTH hors SEQ", () => {
    for (const outside of [ChannelMode.CLOCK, ChannelMode.RANDOM]) {
      const e = new SequencerEngine();
      e.setBaseLength(0, 18);
      e.setChannelMode(0, outside);
      e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
      e.setCvInput(CV_SOURCE_1, 330); // zone +10 si elle etait lue
      e.start();
      e.advance(STEP * 2);
      expect(e.lengthCvOffset(0)).toBe(0);
      expect(e.getEffectiveLength(0)).toBe(18);
    }
  });

  it("remet la zone CV a zero au changement de mode", () => {
    const e = new SequencerEngine();
    e.setBaseLength(0, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10
    e.start();
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(28);

    expect(e.setChannelMode(0, ChannelMode.CLOCK)).toBe(true);
    expect(e.getEffectiveLength(0)).toBe(18); // retour immediat a la base

    expect(e.setChannelMode(0, ChannelMode.SEQ)).toBe(true);
    expect(e.lengthCvOffset(0)).toBe(0); // 10 serait la modulation heritee
    expect(e.getEffectiveLength(0)).toBe(18);
  });

  it("reapplique le CV a la premiere frontiere apres le retour en SEQ", () => {
    const e = new SequencerEngine();
    e.setBaseLength(0, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setChannelMode(0, ChannelMode.SEQ);
    e.start();
    e.advance(STEP);
    expect(e.getEffectiveLength(0)).toBe(18);

    expect(e.setChannelMode(0, ChannelMode.CLOCK)).toBe(true);
    e.setCvInput(CV_SOURCE_1, 330); // zone +10 une fois lue en SEQ
    e.advance(STEP * 2);
    expect(e.lengthCvOffset(0)).toBe(0);
    expect(e.getEffectiveLength(0)).toBe(18);

    expect(e.setChannelMode(0, ChannelMode.SEQ)).toBe(true);
    expect(e.getEffectiveLength(0)).toBe(18); // avant la frontiere : la base
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(28); // la frontiere reapplique
  });
});


describe("Changement de base — ce qu'il laisse intact", () => {
  it("conserve l'offset de longueur", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setBaseLength(0, 18);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(28);

    expect(e.setBaseLength(0, 20)).toBe(true);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(30);
    expect(e.getBaseLength(0)).toBe(20);
  });

  // Observation INDIRECTE : l'API publique n'expose pas d'offset PATTERN,
  // seulement l'index derive. L'index qui suit la base d'exactement +10 prouve
  // que la zone n'a pas bouge. Les bases 3 et 4 gardent de la marge sous 15.
  it("fait suivre l'index derive du pattern sans perdre l'offset", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setSelectedPattern(0, 3);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.patternCvIndex(0)).toBe(13);

    expect(e.setSelectedPattern(0, 4)).toBe(true);
    expect(e.patternCvIndex(0)).toBe(14);
    expect(e.getSelectedPattern(0)).toBe(4);
  });
});

describe("STEP CV — lecture decalee (lot STEP, etape 4d)", () => {
  function routedStep(base: number): SequencerEngine {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setBaseLength(0, base);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.STEP);
    return e;
  }

  it("la frontiere de step deplace l'offset de lecture", () => {
    const e = routedStep(12);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(10);
  });

  it("le CV decale la lecture sans deplacer le step local", () => {
    const e = routedStep(12);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    for (let i = 0; i < 4; ++i) e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(4);
    expect(e.currentReadStep(0)).toBe(2);
  });

  it("un changement de longueur garde l'offset et deplace la lecture", () => {
    const e = routedStep(12);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    for (let i = 0; i < 5; ++i) e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(10);
    expect(e.currentReadStep(0)).toBe(3);
    e.setBaseLength(0, 8);
    expect(e.getEffectiveLength(0)).toBe(8);
    expect(e.stepCvOffset(0)).toBe(10);
    expect(e.currentReadStep(0)).toBe(7);
  });

  it("deux sources s'additionnent avant le modulo", () => {
    const e = routedStep(36);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP);
    e.setCvInput(CV_SOURCE_1, 330);
    e.setCvInput(CV_SOURCE_2, 330);
    e.start();
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(20);
    expect(e.currentReadStep(0)).toBe(21);
  });

  it("un triolet sur le step LU allonge le step", () => {
    const e = routedStep(12);
    e.instanceForChannel(0)!.setRatchet(4, RATCHET_TRIPLET);
    e.setCvInput(CV_SOURCE_1, 99);
    e.start();
    e.advance(STEP);
    expect(e.currentStepTicks(0)).toBe(2 * STEP);
    expect(e.currentStepTriggers(0)).toBe(3);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(4);
  });

  it("un triolet sur le step LOCAL seul n'allonge pas le step", () => {
    const e = routedStep(12);
    e.instanceForChannel(0)!.setRatchet(1, RATCHET_TRIPLET);
    e.setCvInput(CV_SOURCE_1, 99);
    e.start();
    e.advance(STEP);
    expect(e.currentStepTicks(0)).toBe(STEP);
    expect(e.currentStepTriggers(0)).toBe(1);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(4);
  });

  // STEP-8b.4 : la decision de trigger lit le CONTENU a readStep. La premiere
  // frontiere consomme l'onset arme du step 0 (D79) et est ecartee ; la seconde
  // emet un onset sur le step local 2, lu 5 sous une zone de +3. Aucun ratchet.
  function advanceToLocalStepTwoUnderStepOffsetThree(e: SequencerEngine, t: TriggerSequencer): void {
    e.setCvInput(CV_SOURCE_1, 99);
    e.start();
    e.advance(STEP);
    t.update();
    e.advance(STEP);
    t.update();
    expect(e.effectiveStep(0)).toBe(2);
    expect(e.currentReadStep(0)).toBe(5);
  }

  it("le trigger part sur un step actif au step LU seulement", () => {
    const e = routedStep(12);
    const t = new TriggerSequencer(e);
    e.instanceForChannel(0)!.writeStep(5, true);
    advanceToLocalStepTwoUnderStepOffsetThree(e, t);
    expect(t.triggerCount(0)).toBe(1);
  });

  it("le trigger reste muet sur un step actif au step LOCAL seulement", () => {
    const e = routedStep(12);
    const t = new TriggerSequencer(e);
    e.instanceForChannel(0)!.writeStep(2, true);
    advanceToLocalStepTwoUnderStepOffsetThree(e, t);
    expect(t.triggerCount(0)).toBe(0);
  });

  // STEP-8b.5 : LENGTH et STEP changent a la meme frontiere (E3.4-4). CV1 fait
  // passer la longueur de 12 a 8 sur la frontiere du step local 6 ; CV2 tient un
  // decalage de +3. Le step lu passe de 9 a 1, l'offset ne bouge pas (P33), et le
  // contenu comme le ratchet sont lus au step lu de la NOUVELLE longueur.
  it("un changement de longueur a la frontiere lit le contenu et le ratchet au nouveau step lu", () => {
    const e = new SequencerEngine();
    const t = new TriggerSequencer(e);
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)!.writeStep(1, true);
    e.instanceForChannel(0)!.setRatchet(1, RATCHET_TRIPLET);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP);
    e.setCvInput(CV_SOURCE_2, 99);
    e.start();
    for (let i = 0; i < 5; ++i) {
      e.advance(STEP);
      t.update();
    }
    expect(e.effectiveStep(0)).toBe(5);
    expect(e.currentReadStep(0)).toBe(8);
    expect(e.getEffectiveLength(0)).toBe(12);
    expect(e.currentStepTriggers(0)).toBe(1);

    e.setCvInput(CV_SOURCE_1, -132);
    e.advance(STEP);
    t.update();
    expect(e.getEffectiveLength(0)).toBe(8);
    expect(e.effectiveStep(0)).toBe(6);
    expect(e.stepCvOffset(0)).toBe(3);
    expect(e.currentReadStep(0)).toBe(1);
    expect(e.currentStepTicks(0)).toBe(2 * STEP);
    expect(e.currentStepTriggers(0)).toBe(3);
    expect(t.triggerCount(0)).toBe(1);
  });

  // STEP-9.3 G1 : un reset CV remet localStep a 0 et garde la zone STEP.
  it("un reset CV garde la zone STEP et lit depuis l'offset", () => {
    const e = new SequencerEngine();
    const t = new TriggerSequencer(e);
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setBaseLength(0, 12);
    e.instanceForChannel(0)!.writeStep(3, true);
    expect(e.setCvDestination(0, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP);
    e.setCvInput(CV_SOURCE_2, 99);
    e.start();
    e.advance(STEP);
    t.update();
    e.advance(STEP);
    t.update();
    expect(e.effectiveStep(0)).toBe(2);
    expect(e.currentReadStep(0)).toBe(5);

    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.stepCvOffset(0)).toBe(3);
    expect(e.currentReadStep(0)).toBe(3);

    e.advance(1);
    t.update();
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.onsetCount(0)).toBe(1);
    expect(t.triggerCount(0)).toBe(1);

    e.advance(STEP - 1);
    t.update();
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(4);
    expect(t.triggerCount(0)).toBe(0);
  });

  // STEP-9.3 G2, valeurs DERIVEES seulement : le service et le tampon n'ont pas
  // de miroir, la decision musicale sur le nouveau template est prouvee en C++.
  it("l'index PATTERN et l'offset STEP changent a la meme frontiere (valeurs derivees)", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.setBaseLength(0, 12);
    e.setSelectedPattern(0, 5);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    e.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP);
    e.start();
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(1);
    expect(e.patternCvIndex(0)).toBe(5);
    e.setCvInput(CV_SOURCE_1, -165);
    e.setCvInput(CV_SOURCE_2, 99);
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(2);
    expect(e.stepCvOffset(0)).toBe(3);
    expect(e.currentReadStep(0)).toBe(5);
    expect(e.patternCvIndex(0)).toBe(0);
  });

  // STEP-9.3 G3 : une zone negative fait reculer la lecture (P30).
  it("une zone STEP negative fait reculer la lecture avec le modulo positif", () => {
    const e = routedStep(12);
    e.setCvInput(CV_SOURCE_1, -99);
    e.start();
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(-3);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(10);
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(2);
    expect(e.currentReadStep(0)).toBe(11);
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(3);
    expect(e.currentReadStep(0)).toBe(0);
  });

  // STEP-9.3 G4 : les deux bornes de la longueur sous un routage STEP.
  it("une longueur de 1 sous un routage STEP lit toujours le step 0", () => {
    const e = routedStep(1);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    for (let i = 0; i < 3; ++i) {
      e.advance(STEP);
      expect(e.stepCvOffset(0)).toBe(10);
      expect(e.effectiveStep(0)).toBe(0);
      expect(e.currentReadStep(0)).toBe(0);
    }
  });

  it("une longueur de 36 sous un routage STEP boucle a 36", () => {
    const e = routedStep(36);
    e.setCvInput(CV_SOURCE_1, 99);
    e.start();
    for (let i = 0; i < 34; ++i) e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(34);
    expect(e.currentReadStep(0)).toBe(1);
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(35);
    expect(e.currentReadStep(0)).toBe(2);
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.currentReadStep(0)).toBe(3);
  });

  // STEP-9.3 G5 : hors SEQ la zone STEP vaut 0 (A3) ; le retour en SEQ lit la
  // base jusqu'a la frontiere suivante.
  it("un changement de mode remet la zone STEP a zero et le retour en SEQ la reprend a la frontiere suivante", () => {
    const e = routedStep(12);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(10);
    expect(e.currentReadStep(0)).toBe(11);

    expect(e.setChannelMode(0, ChannelMode.CLOCK)).toBe(true);
    expect(e.stepCvOffset(0)).toBe(0);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(1);
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(0);
    expect(e.effectiveStep(0)).toBe(2);
    expect(e.currentReadStep(0)).toBe(2);

    expect(e.setChannelMode(0, ChannelMode.SEQ)).toBe(true);
    expect(e.stepCvOffset(0)).toBe(0);
    expect(e.currentReadStep(0)).toBe(2);
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(10);
    expect(e.effectiveStep(0)).toBe(3);
    expect(e.currentReadStep(0)).toBe(1);
  });

  // STEP-9.3 G6 : un ratchet 6 est lu au step LU. A /1 un slot fait 16 ticks,
  // R6 est admis par la matrice : un ecart ici est la coordonnee, pas la cadence.
  it("un ratchet 6 sur le step LU donne six triggers", () => {
    const e = routedStep(12);
    e.instanceForChannel(0)!.setRatchet(4, RATCHET_6);
    e.setCvInput(CV_SOURCE_1, 99);
    e.start();
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(4);
    expect(e.currentStepTriggers(0)).toBe(6);
    expect(e.currentStepTicks(0)).toBe(STEP);
  });

  it("un ratchet 6 sur le step LOCAL seul donne un trigger", () => {
    const e = routedStep(12);
    e.instanceForChannel(0)!.setRatchet(1, RATCHET_6);
    e.setCvInput(CV_SOURCE_1, 99);
    e.start();
    e.advance(STEP);
    expect(e.effectiveStep(0)).toBe(1);
    expect(e.currentReadStep(0)).toBe(4);
    expect(e.currentStepTriggers(0)).toBe(1);
    expect(e.currentStepTicks(0)).toBe(STEP);
  });

  // STEP-9.3 G10 : une source routee vers STEP ne nourrit ni la longueur ni l'index.
  it("une source routee vers STEP ne bouge ni la longueur ni l'index PATTERN", () => {
    const e = routedStep(12);
    e.setSelectedPattern(0, 5);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    expect(e.stepCvOffset(0)).toBe(10);
    expect(e.lengthCvOffset(0)).toBe(0);
    expect(e.getEffectiveLength(0)).toBe(12);
    expect(e.patternCvIndex(0)).toBe(5);
    expect(e.getSelectedPattern(0)).toBe(5);
  });

  // STEP-9.3 G11 : en CLOCK un routage STEP reste inerte.
  it("un routage STEP en CLOCK garde une zone nulle et un trigger par step", () => {
    const e = new SequencerEngine();
    const t = new TriggerSequencer(e);
    e.setChannelMode(0, ChannelMode.CLOCK);
    e.setBaseLength(0, 12);
    e.setCvDestination(0, CV_SOURCE_1, CvDestination.STEP);
    e.setCvInput(CV_SOURCE_1, 330);
    e.start();
    e.advance(STEP);
    t.update();
    e.advance(STEP);
    t.update();
    expect(e.stepCvOffset(0)).toBe(0);
    expect(e.effectiveStep(0)).toBe(2);
    expect(e.currentReadStep(0)).toBe(2);
    expect(e.onsetCount(0)).toBe(1);
    expect(t.triggerCount(0)).toBe(1);
  });
});
