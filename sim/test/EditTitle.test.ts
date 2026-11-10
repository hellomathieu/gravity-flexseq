import { describe, expect, it } from "vitest";
import { PatternBank } from "../src/domain/PatternBank.js";
import { ChannelMode, SequencerEngine } from "../src/domain/SequencerEngine.js";
import { Transport } from "../src/domain/Transport.js";
import {
  TAB_FIRST_CHANNEL,
  UiController,
  UiEvent,
  UiField,
  UiLevel,
} from "../src/domain/UiController.js";
import { CvDestination, CV_SOURCE_1, CV_SOURCE_2 } from "../src/domain/CvDestination.js";
import {
  EDIT_TITLE_CAPACITY,
  LBL_CV_TITLE,
  LBL_EDIT_TITLE,
  editCursorFor,
  editTitleFor,
} from "../src/domain/EditTitle.js";
import { editScreenModelOf } from "../src/sim/EditScreenModel.js";

/*
 * PRD 5.0, amendment of 2026-09-15 — the EDIT title names the pattern the
 * channel PLAYS, and it reads CV PATTERN while a source is routed to PATTERN.
 * Miroir de `test/test_main_screen_model/test_main_screen_model.cpp`.
 */
function rig() {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  engine.setPatternBank(bank);
  for (let ch = 0; ch < engine.channelCount(); ++ch) engine.setChannelMode(ch, ChannelMode.SEQ);
  const ui = new UiController(engine, new Transport(engine));
  const enterEditOf = (channel: number) => {
    const tab = channel + TAB_FIRST_CHANNEL;
    for (let guard = 0; guard < 16 && ui.currentTab !== tab; ++guard) ui.handle(UiEvent.Rotate, 1);
    expect(ui.currentTab).toBe(tab);
    ui.handle(UiEvent.Press);
    while (ui.field !== UiField.EditEntry) ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    expect(ui.level).toBe(UiLevel.Edit);
  };
  return { bank, engine, ui, enterEditOf };
}

describe("le titre de l ecran EDIT", () => {
  it("les deux libelles tiennent dans le tampon du C++", () => {
    expect(LBL_EDIT_TITLE.length + 1).toBeLessThanOrEqual(EDIT_TITLE_CAPACITY);
    expect(LBL_CV_TITLE.length + 1).toBeLessThanOrEqual(EDIT_TITLE_CAPACITY);
    expect(LBL_EDIT_TITLE).toBe("EDIT PAT. A1");
    expect(LBL_CV_TITLE).toBe("CV PAT. A1");
  });

  it("nomme le pattern choisi sans routage", () => {
    const r = rig();
    r.engine.setSelectedPattern(0, 4);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("EDIT PAT. A5");
    r.engine.setSelectedPattern(0, 13);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("EDIT PAT. B6");
  });

  it("CV PAT. nomme le pattern JOUE, pas le choisi", () => {
    const r = rig();
    r.engine.setSelectedPattern(0, 4);
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    r.engine.setCvInput(CV_SOURCE_1, 330);
    r.engine.start();
    r.engine.advance(96);
    expect(r.engine.patternCvIndex(0)).toBe(14);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("CV PAT. B7");
  });

  it("le mot change des qu une source est routee vers PATTERN, zone nulle comprise", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_2, CvDestination.PATTERN);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("CV PAT. A1");
  });

  it("un autre routage garde EDIT PATTERN", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("EDIT PAT. A1");
  });

  it("refuse un canal invalide", () => {
    const r = rig();
    expect(editTitleFor(r.ui, r.engine, 6)).toBeNull();
  });
});

describe("le curseur de l ecran EDIT", () => {
  it("suit le pas, et se masque sur le bandeau", () => {
    const r = rig();
    r.enterEditOf(0);
    expect(editCursorFor(r.ui)).toBe(0);
    r.ui.handle(UiEvent.Rotate, 1);
    r.ui.handle(UiEvent.Rotate, 1);
    expect(editCursorFor(r.ui)).toBe(2);
    r.ui.handle(UiEvent.Rotate, -1);
    r.ui.handle(UiEvent.Rotate, -1);
    r.ui.handle(UiEvent.Rotate, -1);
    expect(r.ui.isOnHeader).toBe(true);
    expect(editCursorFor(r.ui)).toBe(-1);
  });

  it("se masque sous un routage PATTERN", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    r.enterEditOf(0);
    r.ui.handle(UiEvent.Rotate, 1);
    expect(r.ui.stepCursor).toBe(1);
    expect(editCursorFor(r.ui)).toBe(-1);
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.NONE);
    expect(editCursorFor(r.ui)).toBe(1);
  });
});

describe("le modele de l ecran EDIT", () => {
  it("porte la cadence du canal", () => {
    const r = rig();
    r.enterEditOf(0);
    expect(editScreenModelOf(r.ui, r.engine, 0, "T").ticksPerStep).toBe(96);
    expect(r.engine.setSubdiv(0, -24)).toBe(true);
    expect(editScreenModelOf(r.ui, r.engine, 0, "T").ticksPerStep).toBe(4);
  });

  it("lit la cadence de SON canal", () => {
    const r = rig();
    expect(r.engine.setSubdiv(0, -24)).toBe(true);
    expect(r.engine.setSubdiv(3, -4)).toBe(true);
    r.enterEditOf(3);
    expect(editScreenModelOf(r.ui, r.engine, 3, "T").ticksPerStep).toBe(24);
  });

  it("montre le pattern joue et l etat du canal", () => {
    const r = rig();
    r.engine.setSelectedPattern(0, 4);
    r.engine.setBaseLength(0, 20);
    r.engine.setBarLength(0, 3);
    expect(r.bank.getPattern(4)!.writeStep(7, true)).toBe(true);
    expect(r.bank.getPattern(4)!.setRatchet(7, 3)).toBe(true);
    r.enterEditOf(0);
    r.ui.handle(UiEvent.Rotate, 1);
    r.ui.handle(UiEvent.Rotate, 1);
    const m = editScreenModelOf(r.ui, r.engine, 0, "EDIT PAT. A5");
    expect(m.title).toBe("EDIT PAT. A5");
    expect(m.steps[7]).toBe(true);
    expect(m.steps[6]).toBe(false);
    expect(m.ratchets[7]).toBe(3);
    expect(m.length).toBe(20);
    expect(m.barLength).toBe(3);
    expect(m.cursor).toBe(2);
    expect(m.playhead).toBe(0);
    expect(m.sepSelected).toBe(false);
    expect(m.sepOpen).toBe(false);
  });

  it("suit le playhead", () => {
    const r = rig();
    r.enterEditOf(0);
    r.engine.start();
    r.engine.advance(96);
    r.engine.advance(96);
    expect(editScreenModelOf(r.ui, r.engine, 0, "T").playhead).toBe(2);
  });

  it("le compteur montre le pas sous le curseur a l arret", () => {
    const r = rig();
    r.engine.setBaseLength(0, 12);
    r.enterEditOf(0);
    let m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.counterStep, "le compteur compte a partir de 1").toBe(1);
    expect(m.baseLength).toBe(12);
    for (let i = 0; i < 19; ++i) r.ui.handle(UiEvent.Rotate, 1);
    m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.counterStep, "le curseur peut depasser la longueur : 20/12").toBe(20);
    expect(m.baseLength).toBe(12);
  });

  it("le compteur garde le pas sous le curseur pendant la lecture", () => {
    const r = rig();
    r.enterEditOf(0);
    for (let i = 0; i < 5; ++i) r.ui.handle(UiEvent.Rotate, 1);
    r.engine.start();
    r.engine.advance(96);
    r.engine.advance(96);
    const m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.playhead, "temoin : le pattern joue le pas 3").toBe(2);
    expect(m.counterStep, "le compteur garde le pas 6 du curseur").toBe(6);
  });

  it("le compteur ne nomme aucun pas sur l ecran en lecture seule", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    r.engine.setBaseLength(0, 12);
    r.enterEditOf(0);
    r.ui.handle(UiEvent.Rotate, 1);
    r.ui.handle(UiEvent.Rotate, 1);
    const m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.counterStep).toBe(0);
    expect(m.baseLength).toBe(12);
  });

  it("le compteur montre la longueur de base, pas la derivee", () => {
    const r = rig();
    r.engine.setBaseLength(0, 12);
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    r.engine.setCvInput(CV_SOURCE_1, 330);
    r.enterEditOf(0);
    r.engine.start();
    r.engine.advance(96);
    expect(r.engine.getEffectiveLength(0), "temoin : la longueur jouee est modulee").toBe(22);
    const m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.baseLength, "le denominateur montre la base").toBe(12);
    expect(m.length, "la grille garde la longueur jouee").toBe(22);
  });

  it("marque le champ de longueur et son ouverture", () => {
    const r = rig();
    r.enterEditOf(0);
    let m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.lenSelected).toBe(false);
    expect(m.lenOpen).toBe(false);
    r.ui.handle(UiEvent.Rotate, -1);
    r.ui.handle(UiEvent.Press);
    m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.sepOpen).toBe(true);
    expect(m.lenSelected, "SEP ouvert ne selectionne pas la longueur").toBe(false);
    expect(m.lenOpen).toBe(false);
    r.ui.handle(UiEvent.Press);
    r.ui.handle(UiEvent.Rotate, -1);
    m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.lenSelected).toBe(true);
    expect(m.lenOpen).toBe(false);
    expect(m.sepSelected, "une seule selection a la fois").toBe(false);
    expect(m.cursor).toBe(-1);
    r.ui.handle(UiEvent.Press);
    m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.lenSelected).toBe(true);
    expect(m.lenOpen).toBe(true);
    expect(m.sepOpen).toBe(false);
  });

  it("marque le champ du bandeau et son ouverture", () => {
    const r = rig();
    r.enterEditOf(0);
    r.ui.handle(UiEvent.Rotate, -1);
    expect(r.ui.isOnHeader).toBe(true);
    let m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.sepSelected).toBe(true);
    expect(m.sepOpen).toBe(false);
    expect(m.cursor).toBe(-1);
    r.ui.handle(UiEvent.Press);
    m = editScreenModelOf(r.ui, r.engine, 0, "T");
    expect(m.sepSelected).toBe(true);
    expect(m.sepOpen).toBe(true);
  });
});
