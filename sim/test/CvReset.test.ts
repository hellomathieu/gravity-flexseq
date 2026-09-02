import { describe, it, expect } from "vitest";
import { SequencerEngine, ChannelMode } from "../src/domain/SequencerEngine";
import { TriggerSequencer } from "../src/domain/TriggerSequencer";
import {
  CvDestination,
  CV_SOURCE_1,
  CV_SOURCE_2,
} from "../src/domain/CvDestination";
import { RATCHET_4, RATCHET_TRIPLET } from "../src/domain/Pattern";

const STEP = 96;

function seqRouted(source = CV_SOURCE_1): SequencerEngine {
  const e = new SequencerEngine();
  e.setChannelMode(0, ChannelMode.SEQ);
  expect(e.setCvDestination(0, source, CvDestination.RESET)).toBe(true);
  return e;
}

describe("le reset CV — application", () => {
  it("un front CV1 reset immediatement un channel SEQ route CV1", () => {
    const e = seqRouted(CV_SOURCE_1);
    e.start();
    e.advance(2 * STEP);
    expect(e.effectiveStep(0)).toBe(2);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(0);
  });

  it("un front CV2 reset un channel route CV2", () => {
    const e = seqRouted(CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP);
    expect(e.effectiveStep(0)).toBe(2);
    e.applyCvResetEvents(1 << CV_SOURCE_2);
    expect(e.effectiveStep(0)).toBe(0);
  });

  it("un front sur la source non routee ne fait rien", () => {
    const e = seqRouted(CV_SOURCE_2);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(2);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
  });

  it("un channel qui route les deux sources reset sur l'une ou l'autre", () => {
    const e = seqRouted(CV_SOURCE_1);
    expect(e.setCvDestination(0, CV_SOURCE_2, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_2);
    expect(e.effectiveStep(0)).toBe(0);
  });

  it("un masque zero est un no-op strict", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(0);
    expect(e.effectiveStep(0)).toBe(2);
    e.advance(56);
    expect(e.effectiveStep(0)).toBe(3);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("les bits au-dela des sources sont masques", () => {
    const e = seqRouted(CV_SOURCE_1);
    expect(e.setCvDestination(0, CV_SOURCE_2, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(0xfc);
    expect(e.effectiveStep(0)).toBe(2);
    e.advance(56);
    expect(e.effectiveStep(0)).toBe(3);
  });
});

describe("le reset CV — l'onset arme", () => {
  it("aucun onset entre l'appel et le prochain advance", () => {
    const e = seqRouted();
    e.start();
    e.advance(STEP + 40);
    expect(e.onsetCount(0)).toBe(1);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("le premier advance emet un onset sur le step 0", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
    expect(e.effectiveStep(0)).toBe(0);
  });

  it("l'armement ne tire pas deux fois", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
  });

  it("un step 0 inactif donne un onset et pas de trigger", () => {
    const e = seqRouted();
    const t = new TriggerSequencer(e);
    e.start();
    e.advance(2 * STEP);
    t.update();
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(1);
    t.update();
    expect(e.onsetCount(0)).toBe(1);
    expect(t.triggered(0)).toBe(false);
  });

  it("un step 0 actif donne un trigger", () => {
    const e = seqRouted();
    const t = new TriggerSequencer(e);
    expect(e.instanceForChannel(0)!.writeStep(0, true)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    t.update();
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(1);
    t.update();
    expect(t.triggerCount(0)).toBe(1);
  });

  it("deux fenetres donnent deux resets", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(3 * STEP - 1);
    expect(e.effectiveStep(0)).toBe(3);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("les deux bits dans UN masque donnent UN onset", () => {
    const e = seqRouted(CV_SOURCE_1);
    expect(e.setCvDestination(0, CV_SOURCE_2, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(0x03);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });
});

describe("le reset CV — ce qui reste intact", () => {
  it("le pattern, la selection, la base et la phase restent intacts", () => {
    const e = seqRouted();
    const p = e.instanceForChannel(0)!;
    expect(p.writeStep(0, true)).toBe(true);
    expect(p.writeStep(5, true)).toBe(true);
    expect(p.setRatchet(3, RATCHET_4)).toBe(true);
    expect(e.setSelectedPattern(0, 7)).toBe(true);
    expect(e.setBaseLength(0, 18)).toBe(true);
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.masterPhase).toBe(232);
    expect(e.getSelectedPattern(0)).toBe(7);
    expect(e.getBaseLength(0)).toBe(18);
    expect(p.readStep(0)).toBe(true);
    expect(p.readStep(5)).toBe(true);
    expect(p.readStep(1)).toBe(false);
    expect(p.getRatchet(3)).toBe(RATCHET_4);
  });

  it("la prochaine frontiere tombe exactement stepTicks apres le reset", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP + 40);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(95);
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(1);
    expect(e.effectiveStep(0)).toBe(1);
  });

  it("l'offset du length cv survit a un reset", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    expect(e.setBaseLength(0, 18)).toBe(true);
    expect(e.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH)).toBe(true);
    expect(e.setCvDestination(0, CV_SOURCE_2, CvDestination.RESET)).toBe(true);
    expect(e.setCvInput(CV_SOURCE_1, 330)).toBe(true);
    e.start();
    e.advance(STEP);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(28);
    e.applyCvResetEvents(1 << CV_SOURCE_2);
    expect(e.lengthCvOffset(0)).toBe(10);
    expect(e.getEffectiveLength(0)).toBe(28);
    expect(e.effectiveStep(0)).toBe(0);
  });

  it("une cadence differee survit au reset et s'applique sur le beat", () => {
    const e = seqRouted();
    e.start();
    e.advance(40);
    expect(e.setSubdiv(0, -4)).toBe(true);
    expect(e.getTicksPerStep(0)).toBe(96);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.getTicksPerStep(0)).toBe(96);
    e.advance(56);
    expect(e.getTicksPerStep(0)).toBe(24);
  });
});

describe("le reset CV — la matrice des modes", () => {
  it("CLOCK offset 0 tire au premier tick", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.CLOCK);
    expect(e.setCvDestination(0, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("CLOCK avec offset tire a l'offset et pas a l'armement", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.CLOCK);
    expect(e.setOffset(0, 10)).toBe(true);
    expect(e.setCvDestination(0, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(8);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("RANDOM ignore le reset et le retour en SEQ ne rejoue rien", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.RANDOM);
    expect(e.setCvDestination(0, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(2);
    e.setChannelMode(0, ChannelMode.SEQ);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
    expect(e.effectiveStep(0)).toBe(2);
  });

  it("SEQ reset pendant que RANDOM reste, sur la meme source", () => {
    const e = seqRouted();
    e.setChannelMode(1, ChannelMode.RANDOM);
    expect(e.setCvDestination(1, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(0);
    expect(e.effectiveStep(1)).toBe(2);
  });
});

describe("le reset CV — le transport", () => {
  it("un reset a l'arret repositionne et ne tire qu'apres start", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP + 40);
    e.stop();
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
    e.start();
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("le reset global subsume un armement CV en UN onset", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.reset();
    expect(e.effectiveStep(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("le reset global arme les six channels", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) e.setChannelMode(ch, ChannelMode.SEQ);
    e.start();
    e.advance(3 * STEP);
    e.reset();
    e.advance(1);
    for (let ch = 0; ch < 6; ++ch) {
      expect(e.onsetCount(ch)).toBe(1);
      expect(e.effectiveStep(ch)).toBe(0);
    }
  });

  it("un reset a l'arret arme jusqu'au premier start", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.start();
    e.advance(2 * STEP);
    e.stop();
    e.reset();
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
    e.start();
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("le reset global fait tirer CLOCK offset 0 au premier tick", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.CLOCK);
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("un armement global en CLOCK attend l'offset", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.CLOCK);
    expect(e.setOffset(0, 10)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.advance(1);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(8);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("un armement global en RANDOM emet l'onset a travers le tirage", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.RANDOM);
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });
});

describe("le reset CV — D80, l'armement survit au changement de mode", () => {
  it("l'armement survit au passage en RANDOM", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.setChannelMode(0, ChannelMode.RANDOM);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("l'armement survit au passage en CLOCK", () => {
    const e = seqRouted();
    e.start();
    e.advance(2 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.setChannelMode(0, ChannelMode.CLOCK);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("un armement global survit au passage en RANDOM", () => {
    const e = new SequencerEngine();
    e.setChannelMode(0, ChannelMode.SEQ);
    e.start();
    e.advance(2 * STEP);
    e.reset();
    e.setChannelMode(0, ChannelMode.RANDOM);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });
});

describe("le reset CV — les ratchets", () => {
  it("un reset en plein ratchet abandonne les sous-onsets restants", () => {
    const e = seqRouted();
    expect(e.instanceForChannel(0)!.setRatchet(2, RATCHET_4)).toBe(true);
    e.start();
    e.advance(2 * STEP);
    e.advance(24);
    expect(e.onsetCount(0)).toBe(1);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    e.advance(24);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(71);
    expect(e.onsetCount(0)).toBe(0);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
  });

  it("un TRIPLET sur le step 0 double le stepTicks recache", () => {
    const e = seqRouted();
    expect(e.instanceForChannel(0)!.setRatchet(0, RATCHET_TRIPLET)).toBe(true);
    e.refreshTiming(0);
    e.start();
    e.advance(192);
    expect(e.effectiveStep(0)).toBe(1);
    e.advance(40);
    expect(e.currentStepTicks(0)).toBe(96);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.currentStepTicks(0)).toBe(192);
    expect(e.currentStepTriggers(0)).toBe(3);
    e.advance(1);
    expect(e.onsetCount(0)).toBe(1);
    e.advance(63);
    expect(e.onsetCount(0)).toBe(1);
  });
});

describe("le reset CV — l'isolation", () => {
  it("le reset d'un channel laisse les cinq autres intacts", () => {
    const e = new SequencerEngine();
    for (let ch = 0; ch < 6; ++ch) e.setChannelMode(ch, ChannelMode.SEQ);
    expect(e.setCvDestination(2, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    e.start();
    e.advance(3 * STEP);
    e.applyCvResetEvents(1 << CV_SOURCE_1);
    expect(e.effectiveStep(2)).toBe(0);
    for (const ch of [0, 1, 3, 4, 5]) {
      expect(e.effectiveStep(ch)).toBe(3);
    }
  });
});
