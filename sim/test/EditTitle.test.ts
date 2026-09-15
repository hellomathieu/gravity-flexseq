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
    expect(LBL_EDIT_TITLE).toBe("EDIT PATTERN A1");
    expect(LBL_CV_TITLE).toBe("CV PATTERN A1");
  });

  it("nomme le pattern choisi sans routage", () => {
    const r = rig();
    r.engine.setSelectedPattern(0, 4);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("EDIT PATTERN A5");
    r.engine.setSelectedPattern(0, 13);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("EDIT PATTERN B6");
  });

  it("CV PATTERN nomme le pattern JOUE, pas le choisi", () => {
    const r = rig();
    r.engine.setSelectedPattern(0, 4);
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    r.engine.setCvInput(CV_SOURCE_1, 330);
    r.engine.start();
    r.engine.advance(96);
    expect(r.engine.patternCvIndex(0)).toBe(14);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("CV PATTERN B7");
  });

  it("le mot change des qu une source est routee vers PATTERN, zone nulle comprise", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_2, CvDestination.PATTERN);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("CV PATTERN A1");
  });

  it("un autre routage garde EDIT PATTERN", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    r.enterEditOf(0);
    expect(editTitleFor(r.ui, r.engine, 0)).toBe("EDIT PATTERN A1");
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
