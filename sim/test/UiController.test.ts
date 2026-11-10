import { PatternBank } from "../src/domain/PatternBank.js";
import { describe, expect, it } from "vitest";
import {
  CHANNEL_TAB_FIELDS,
  CLOCK_SOURCE_COUNT,
  DEFAULT_TEMPO,
  MAX_TEMPO,
  MIN_TEMPO,
  CONFIG_FIELD_INDEX_LENGTH,
  CONFIG_FIELD_INDEX_SUBDIV,
  SEQ_CHANNEL_TAB_FIELDS,
  SEQ_FIELD_INDEX_CONFIG,
  SEQ_FIELD_INDEX_EDIT_ENTRY,
  SEQ_FIELD_INDEX_PATTERN,
  SEQ_FIELD_INDEX_MODE,
  STEP_COUNT,
  TAB_CLOCK,
  TAB_COUNT,
  TAB_FIRST_CHANNEL,
  TAB_SETTINGS,
  UiController,
  UiEvent,
  UiField,
  SettingsAction,
  UiLevel,
} from "../src/domain/UiController.js";
import { Transport } from "../src/domain/Transport.js";
import {
  ClockMode,
  clockModeOf,
  clockSourceOf,
  ppqnAtIndex,
  ppqnIndexOf,
} from "../src/domain/ClockSource.js";
import { PATTERN_COUNT } from "../src/domain/PatternBank.js";
import {
  CHANNEL_COUNT,
  DEFAULT_BAR_LENGTH,
  DEFAULT_LENGTH,
  PPQN,
  SequencerEngine,
  ChannelMode,
} from "../src/domain/SequencerEngine.js";
import { RATCHET_CODES, RATCHET_NONE, RATCHET_2, RATCHET_3, RATCHET_TRIPLET } from "../src/domain/Pattern.js";
import {
  CV_SOURCE_1,
  CV_SOURCE_2,
  CvDestination,
  MOD_CHOICE_COUNT,
  modChoiceAt,
  modIndexOf,
} from "../src/domain/CvDestination.js";

function rig() {
  const engine = new SequencerEngine();
  engine.setPatternBank(new PatternBank());
  for (let ch = 0; ch < engine.channelCount(); ++ch) {
    engine.setChannelMode(ch, ChannelMode.SEQ);
  }
  const transport = new Transport(engine);
  const ui = new UiController(engine, transport);

  const gotoTab = (tab: number) => {
    for (let guard = 0; guard < TAB_COUNT; guard += 1) {
      if (ui.currentTab === tab) return;
      ui.handle(UiEvent.Rotate, 1);
    }
    throw new Error("tab never reached");
  };
  const gotoField = (field: UiField): void => {
    const onConfig = field === UiField.Length
      || field === UiField.Subdiv
      || field === UiField.Mod;
    if (onConfig && !ui.isOnConfigPage) {
      gotoField(UiField.Config);
      ui.handle(UiEvent.Press);
    }
    for (let guard = 0; guard <= CHANNEL_TAB_FIELDS; guard += 1) {
      if (ui.field === field) return;
      ui.handle(UiEvent.Rotate, 1);
    }
    throw new Error("field never reached");
  };
  const enterTab = () => ui.handle(UiEvent.Press);
  const enterEdit = () => {
    enterTab();
    gotoField(UiField.EditEntry);
    ui.handle(UiEvent.Press);
  };

  return { engine, transport, ui, gotoTab, gotoField, enterTab, enterEdit };
}

describe("UiController — tab bar", () => {
  it("starts on the tab bar over the first channel", () => {
    const { ui } = rig();
    expect(ui.level).toBe(UiLevel.TabBar);
    expect(ui.currentTab).toBe(TAB_FIRST_CHANNEL);
    expect(ui.selectedChannel).toBe(0);
    expect(ui.fieldOpen).toBe(false);
  });

  it("rotate changes tab and wraps at both ends", () => {
    const { ui, gotoTab } = rig();
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.currentTab).toBe(2);
    gotoTab(TAB_SETTINGS);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.currentTab).toBe(TAB_CLOCK);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.currentTab).toBe(TAB_SETTINGS);
  });

  it("moves one tab per detent whatever the acceleration reports", () => {
    const { ui } = rig();
    ui.handle(UiEvent.Rotate, 3);
    expect(ui.currentTab).toBe(2);
    ui.handle(UiEvent.Rotate, -3);
    expect(ui.currentTab).toBe(1);
  });

  it("press enters a tab that has fields", () => {
    const { ui, enterTab } = rig();
    enterTab();
    expect(ui.level).toBe(UiLevel.Tab);
    expect(ui.cursor).toBe(0);
    // Le rig met les six canaux en SEQ : le premier champ est la grande valeur.
    expect(ui.field).toBe(UiField.Pattern);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field, "MODE est la ligne 1 des trois").toBe(UiField.Mode);
  });

  it("the settings tab holds the four entries of the original", () => {
    const { ui, gotoTab, enterTab } = rig();
    gotoTab(TAB_SETTINGS);
    expect(ui.fieldCount).toBe(4);
    expect(ui.fieldAt(0)).toBe(UiField.Calibrate);
    expect(ui.fieldAt(1)).toBe(UiField.RotateScreen);
    expect(ui.fieldAt(2)).toBe(UiField.ReverseEncoder);
    expect(ui.fieldAt(3)).toBe(UiField.FactoryReset);
    expect(ui.fieldAt(4)).toBe(UiField.None);
    enterTab();
    expect(ui.level).toBe(UiLevel.Tab);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.None);
  });

  const settingsField = (index: number) => {
    const r = rig();
    r.gotoTab(TAB_SETTINGS);
    r.enterTab();
    for (let i = 0; i < index; i += 1) r.ui.handle(UiEvent.Rotate, 1);
    expect(r.ui.cursor).toBe(index);
    return r.ui;
  };

  it("calibrate asks for the action once and shows done", () => {
    const ui = settingsField(0);
    ui.handle(UiEvent.Press);
    expect(ui.doneShown).toBe(true);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.Calibrate);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.None);
  });

  it("rotate screen asks for the action and shows no done", () => {
    const ui = settingsField(1);
    ui.handle(UiEvent.Press);
    expect(ui.doneShown).toBe(false);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.RotateScreen);
  });

  it("reverse encoder asks for the action and shows done", () => {
    const ui = settingsField(2);
    ui.handle(UiEvent.Press);
    expect(ui.doneShown).toBe(true);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.ReverseEncoder);
  });

  it("the press that clears done does nothing else", () => {
    const ui = settingsField(0);
    ui.handle(UiEvent.Press);
    ui.takeSettingsAction();
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.doneShown).toBe(true);
    expect(ui.cursor).toBe(0);
    ui.handle(UiEvent.Press);
    expect(ui.doneShown).toBe(false);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.None);
    expect(ui.level).toBe(UiLevel.Tab);
    ui.handle(UiEvent.Press);
    expect(ui.doneShown).toBe(true);
    ui.handle(UiEvent.LongPress);
    expect(ui.doneShown).toBe(false);
    expect(ui.level).toBe(UiLevel.Tab);
  });

  it("factory reset asks first, and no comes first", () => {
    const ui = settingsField(3);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(true);
    expect(ui.confirmYes).toBe(false);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.None);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.doneShown).toBe(false);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.None);
  });

  it("factory reset runs on yes only", () => {
    const ui = settingsField(3);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.confirmYes).toBe(true);
    expect(ui.cursor).toBe(3);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.confirmYes).toBe(true);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.confirmYes).toBe(false);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.doneShown).toBe(true);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.FactoryReset);
  });

  it("a long press cancels the factory reset question", () => {
    const ui = settingsField(3);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.LongPress);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.level).toBe(UiLevel.Tab);
    expect(ui.takeSettingsAction()).toBe(SettingsAction.None);
    ui.handle(UiEvent.Press);
    expect(ui.confirmYes, "the question starts again at NO").toBe(false);
  });

  it("shift rotation never answers the factory reset question", () => {
    const ui = settingsField(3);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.confirmYes).toBe(false);
    expect(ui.fieldOpen).toBe(false);
  });

  it("the internal clock exposes the tempo then the mode and owns no channel", () => {
    const { ui, gotoTab } = rig();
    gotoTab(TAB_CLOCK);
    expect(ui.fieldCount).toBe(2);
    expect(ui.fieldAt(0)).toBe(UiField.Tempo);
    expect(ui.fieldAt(1)).toBe(UiField.ClockMode);
    expect(ui.fieldAt(2)).toBe(UiField.None);
    expect(ui.mainField).toBe(UiField.Tempo);
    expect(ui.selectedChannel).toBe(-1);
  });

  it("the external clock exposes the mode then the ppqn", () => {
    for (const source of [1, 2, 3, 4]) {
      const { ui, gotoTab } = rig();
      gotoTab(TAB_CLOCK);
      expect(ui.setClockSource(source)).toBe(true);
      expect(ui.fieldCount).toBe(2);
      expect(ui.fieldAt(0)).toBe(UiField.ClockMode);
      expect(ui.fieldAt(1)).toBe(UiField.Ppqn);
      expect(ui.fieldAt(2)).toBe(UiField.None);
      expect(ui.mainField).toBe(UiField.None);
    }
  });

  it("the midi clock exposes the mode alone", () => {
    const { ui, gotoTab } = rig();
    gotoTab(TAB_CLOCK);
    expect(ui.setClockSource(5)).toBe(true);
    expect(ui.fieldCount).toBe(1);
    expect(ui.fieldAt(0)).toBe(UiField.ClockMode);
    expect(ui.fieldAt(1)).toBe(UiField.None);
    expect(ui.mainField).toBe(UiField.None);
  });

  it("the mode and the ppqn are two views of the source byte", () => {
    const modes = [0, 1, 1, 1, 1, 2];
    const rates = [0, 24, 4, 2, 1, 0];
    for (let source = 0; source < 6; source += 1) {
      expect(clockModeOf(source)).toBe(modes[source]);
      if (modes[source] === 1) {
        expect(ppqnAtIndex(ppqnIndexOf(source))).toBe(rates[source]);
        expect(clockSourceOf(ClockMode.EXT, ppqnIndexOf(source))).toBe(source);
      } else {
        expect(ppqnIndexOf(source)).toBe(0);
      }
    }
    expect(clockSourceOf(ClockMode.INT, 3)).toBe(0);
    expect(clockSourceOf(ClockMode.MIDI, 3)).toBe(5);
    expect(clockSourceOf(ClockMode.EXT, 4)).toBe(1);
    expect(CLOCK_SOURCE_COUNT).toBe(6);
  });

  it("the mode field walks int, ext, midi and stops at both ends", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    gotoField(UiField.ClockMode);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.clockSource).toBe(0);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.clockSource).toBe(1);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.clockSource).toBe(5);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.clockSource).toBe(5);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.clockSource).toBe(1);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.clockSource).toBe(0);
  });

  it("the cursor follows the mode field when the mode moves it", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    gotoField(UiField.ClockMode);
    expect(ui.cursor).toBe(1);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.cursor).toBe(0);
    expect(ui.field).toBe(UiField.ClockMode);
    expect(ui.fieldOpen).toBe(true);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.cursor).toBe(0);
    expect(ui.field).toBe(UiField.ClockMode);
    ui.handle(UiEvent.Rotate, -1);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.clockSource).toBe(0);
    expect(ui.cursor).toBe(1);
    expect(ui.field).toBe(UiField.ClockMode);
  });

  it("the ppqn field walks 24, 4, 2, 1 and never leaves the external mode", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    expect(ui.setClockSource(1)).toBe(true);
    enterTab();
    gotoField(UiField.Ppqn);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.clockSource).toBe(1);
    for (const expected of [2, 3, 4]) {
      ui.handle(UiEvent.Rotate, 1);
      expect(ui.clockSource).toBe(expected);
    }
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.clockSource).toBe(4);
    expect(ui.field).toBe(UiField.Ppqn);
  });

  it("a mode that does not change keeps the ppqn", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    expect(ui.setClockSource(3)).toBe(true);
    enterTab();
    gotoField(UiField.ClockMode);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 0);
    expect(ui.clockSource).toBe(3);
  });

  it("entering the external mode starts at 24 ppqn", () => {
    const { ui, gotoTab, enterTab } = rig();
    gotoTab(TAB_CLOCK);
    expect(ui.setClockSource(5)).toBe(true);
    enterTab();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.clockSource).toBe(1);
  });

  it("shift rotation on the bar leaves the tempo alone off the internal clock", () => {
    for (const source of [1, 5]) {
      const { ui, gotoTab } = rig();
      gotoTab(TAB_CLOCK);
      expect(ui.setClockSource(source)).toBe(true);
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(ui.tempo).toBe(120);
      expect(ui.clockSource).toBe(source);
    }
  });

  it("the other gestures do nothing on the tab bar", () => {
    const { ui } = rig();
    for (const event of [
      UiEvent.LongPress,
      UiEvent.ShiftPress,
      UiEvent.ShiftLongPress,
      UiEvent.ShiftPlayPress,
    ]) {
      ui.handle(event, 1);
      expect(ui.level).toBe(UiLevel.TabBar);
      expect(ui.currentTab).toBe(TAB_FIRST_CHANNEL);
    }
  });

  it("covers every tab of the bar", () => {
    const { ui } = rig();
    const seen = new Set<number>();
    for (let i = 0; i < TAB_COUNT; i += 1) {
      seen.add(ui.currentTab);
      ui.handle(UiEvent.Rotate, 1);
    }
    expect(seen.size).toBe(TAB_COUNT);
  });
});

describe("UiController — inside a tab", () => {
  it("rotate moves the field cursor and wraps", () => {
    const { ui, enterTab } = rig();
    enterTab();
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field).toBe(UiField.Mode);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.field).toBe(UiField.Pattern);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.field).toBe(UiField.Config);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field, "la liste boucle sur la grande valeur").toBe(UiField.Pattern);
  });

  it("press opens a value field and press closes it", () => {
    const { ui, enterTab } = rig();
    enterTab();
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(true);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.level).toBe(UiLevel.Tab);
  });

  it("rotate changes the value while the field is open", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH + 1);
    expect(ui.field).toBe(UiField.Length);
  });

  it("long press closes the open field without leaving the tab", () => {
    const { ui, enterTab } = rig();
    enterTab();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.LongPress);
    expect(ui.fieldOpen).toBe(false);
    expect(ui.level).toBe(UiLevel.Tab);
  });

  it("long press on a closed field returns to the tab bar", () => {
    const { ui, enterTab } = rig();
    enterTab();
    ui.handle(UiEvent.LongPress);
    expect(ui.level).toBe(UiLevel.TabBar);
    expect(ui.currentTab).toBe(TAB_FIRST_CHANNEL);
  });

  it("shift rotate changes the value without opening the field", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.fieldOpen).toBe(false);
    expect(engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH - 1);
  });

  it("the tempo range is the one the module announces", () => {
    expect(MIN_TEMPO).toBe(20);
    expect(MAX_TEMPO).toBe(300);
    const { ui } = rig();
    expect(ui.setTempo(19)).toBe(false);
    expect(ui.setTempo(20)).toBe(true);
    expect(ui.setTempo(300)).toBe(true);
    expect(ui.setTempo(301)).toBe(false);
  });

  it("tempo is clamped to the musical range", () => {
    const { ui, gotoTab, enterTab } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    expect(ui.tempo).toBe(DEFAULT_TEMPO);
    for (let i = 0; i < MAX_TEMPO + 10; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.tempo).toBe(MAX_TEMPO);
    for (let i = 0; i < MAX_TEMPO + 10; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.tempo).toBe(MIN_TEMPO);
  });

  it("the clock fields never reach the sentinel", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    gotoField(UiField.ClockMode);
    for (let i = 0; i < 11; i += 1) {
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(ui.clockSource).toBeLessThan(6);
    }
    expect(ui.clockSource).toBe(5);
    for (let i = 0; i < 11; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.clockSource).toBe(0);
  });

  // ⚠️ SHIFT plus rotation CHOISIT le nom et ne charge pas — PRD 5.0 amendement
  // 1quater. Ce qui se deplace est le nom AFFICHE.
  it("le nom affiche est ecrete a la banque", () => {
    const { ui, engine } = rig();
    for (let i = 0; i < PATTERN_COUNT + 5; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.displayedPattern).toBe(15);
    for (let i = 0; i < PATTERN_COUNT + 5; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.displayedPattern).toBe(0);
    expect(engine.getSelectedPattern(0), "et le canal n a jamais charge").toBe(0);
  });

  it("edite la BASE et jamais la valeur derivee (risque 64)", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    engine.setBaseLength(0, 18);
    engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    engine.setCvInput(CV_SOURCE_1, 330);
    engine.start();
    engine.advance(96);
    expect(engine.getBaseLength(0)).toBe(18);
    expect(engine.getEffectiveLength(0)).toBe(28);

    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getBaseLength(0)).toBe(19);
    expect(engine.getEffectiveLength(0)).toBe(29);
  });

  it("laisse la modulation en place pendant l'edition", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    engine.setBaseLength(0, 18);
    engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    engine.setCvInput(CV_SOURCE_1, -330);
    engine.start();
    engine.advance(96);
    expect(engine.getEffectiveLength(0)).toBe(8);

    for (let i = 0; i < 3; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getBaseLength(0)).toBe(15);
    expect(engine.getEffectiveLength(0)).toBe(5);
  });

  it("the length field is clamped to one and thirty six", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    for (let i = 0; i < 40; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getEffectiveLength(0)).toBe(36);
    for (let i = 0; i < 40; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getEffectiveLength(0)).toBe(1);
  });

  it("no field keeps the acceleration, not even the tempo", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(ui.tempo).toBe(DEFAULT_TEMPO + 1);
    gotoField(UiField.ClockMode);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(ui.clockSource).toBe(1);
  });

  it("never accelerates a short list", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH + 1);
  });

  it("an accelerated turn lands on the bound instead of being refused", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    for (let i = 0; i < 19; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getEffectiveLength(0)).toBe(35);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(engine.getEffectiveLength(0)).toBe(36);

    for (let i = 0; i < 34; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getEffectiveLength(0)).toBe(2);
    ui.handle(UiEvent.ShiftRotate, -3);
    expect(engine.getEffectiveLength(0)).toBe(1);

    // le NOM du pattern est le parametre principal : il se choisit par SHIFT
    // plus rotation sur la BARRE, et ce choix ne charge rien.
    ui.handle(UiEvent.LongPress);
    ui.handle(UiEvent.LongPress);
    for (let i = 0; i < 14; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.displayedPattern).toBe(14);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(ui.displayedPattern).toBe(15);
  });

  it("a turn to the right multiplies the subdiv and a turn to the left divides it", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Subdiv);
    expect(engine.getSubdiv(0)).toBe(1);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSubdiv(0), "un cran a droite : x2").toBe(-2);
    ui.handle(UiEvent.ShiftRotate, -2);
    expect(engine.getSubdiv(0), "un geste vaut un cran").toBe(1);
    ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getSubdiv(0), "un cran a gauche : /2").toBe(2);
    for (let i = 0; i < 30; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSubdiv(0), "butee a droite : x24").toBe(-24);
    for (let i = 0; i < 30; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getSubdiv(0), "butee a gauche : /128").toBe(128);
  });

  it("the bar length field walks only the allowed values", () => {
    // SEP vit dans l en-tete de l ecran EDIT depuis le lot 12.
    const { ui, engine, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Rotate, -1);
    ui.handle(UiEvent.Press);
    expect(engine.getBarLength(0)).toBe(DEFAULT_BAR_LENGTH);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getBarLength(0)).toBe(6);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getBarLength(0)).toBe(6);
    for (let i = 0; i < 8; i += 1) ui.handle(UiEvent.Rotate, -1);
    expect(engine.getBarLength(0)).toBe(0);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getBarLength(0)).toBe(2);
  });

  it("a field edit applies to the channel of the current tab", () => {
    const { ui, engine, gotoTab, enterTab, gotoField } = rig();
    gotoTab(3);
    enterTab();
    gotoField(UiField.Length);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getEffectiveLength(2)).toBe(DEFAULT_LENGTH + 1);
    expect(engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH);
  });

  it("the edit entry is not a value", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.EditEntry);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.level).toBe(UiLevel.Tab);
    expect(ui.fieldOpen).toBe(false);
    expect(engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH);
  });
});

describe("UiController — EDIT PATTERN", () => {
  it("press on the edit entry enters the grid", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    expect(ui.level).toBe(UiLevel.Edit);
    expect(ui.stepCursor).toBe(0);
  });

  it("rotate moves the step cursor and wraps at thirty six", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.isOnHeader, "un cran a gauche monte dans l en-tete").toBe(true);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.stepCursor).toBe(0);
    for (let i = 0; i < 5; ++i) ui.handle(UiEvent.Rotate, 5);
    expect(ui.stepCursor).toBe(5);
  });

  it("press toggles the step under the cursor", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    for (let i = 0; i < 7; ++i) ui.handle(UiEvent.Rotate, 1);
    expect(engine.patternForChannel(0)!.readStep(7)).toBe(false);
    ui.handle(UiEvent.Press);
    expect(engine.patternForChannel(0)!.readStep(7)).toBe(true);
    ui.handle(UiEvent.Press);
    expect(engine.patternForChannel(0)!.readStep(7)).toBe(false);
  });

  it("shift rotate sets the ratchet of an active step and clamps", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(8);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(RATCHET_2);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(RATCHET_3);
    for (let i = 0; i < RATCHET_CODES.length + 3; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(RATCHET_TRIPLET);
    for (let i = 0; i < RATCHET_CODES.length + 3; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(RATCHET_NONE);
  });

  it("a ratchet edit takes effect on the current step immediately", () => {
    const { ui, engine, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    engine.start();
    expect(engine.currentStepTicks(0)).toBe(PPQN);
    for (let i = 0; i < 6; ++i) ui.handle(UiEvent.ShiftRotate, 5);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(RATCHET_TRIPLET);
    expect(engine.currentStepTicks(0)).toBe(2 * PPQN);
  });

  it("shift rotate in edit no longer changes channel", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.selectedChannel).toBe(0);
    expect(ui.level).toBe(UiLevel.Edit);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(8);
  });

  it("shift rotate on an inactive step does nothing", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    expect(engine.patternForChannel(0)!.readStep(0)).toBe(false);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.patternForChannel(0)!.getRatchet(0)).toBe(RATCHET_NONE);
  });

  it("the grid follows the pattern of the channel selected in edit", () => {
    const { ui, engine, enterEdit, gotoTab } = rig();
    engine.setSelectedPattern(1, 5);
    gotoTab(TAB_FIRST_CHANNEL + 1);
    enterEdit();
    ui.handle(UiEvent.Press);
    expect(engine.patternForChannel(1)!.readStep(0)).toBe(true);
    expect(engine.patternForChannel(0)!.readStep(0)).toBe(false);
  });

  it("shift long press clears the pattern steps and ratchets", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftRotate, 3);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftLongPress);
    for (let step = 0; step < STEP_COUNT; step += 1) {
      expect(engine.patternForChannel(0)!.readStep(step)).toBe(false);
      expect(engine.patternForChannel(0)!.getRatchet(step)).toBe(RATCHET_NONE);
    }
  });

  it("long press returns from the grid to the tab, then to the bar", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.LongPress);
    expect(ui.level).toBe(UiLevel.Tab);
    expect(ui.field).toBe(UiField.EditEntry);
    ui.handle(UiEvent.LongPress);
    expect(ui.level).toBe(UiLevel.TabBar);
  });
});

describe("UiController — PLAY and the gesture left free", () => {
  it("play toggles the transport at every level", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(true);
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(false);

    enterTab();
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(true);
    expect(ui.level).toBe(UiLevel.Tab);

    gotoField(UiField.EditEntry);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(false);
    expect(ui.level).toBe(UiLevel.Edit);
  });

  it("play realigns the channels when it starts", () => {
    const { ui, engine } = rig();
    engine.start();
    engine.advance(3 * PPQN);
    expect(engine.masterPhase).toBe(3 * PPQN);
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(false);
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(true);
    expect(engine.masterPhase).toBe(0);
    for (let ch = 0; ch < CHANNEL_COUNT; ch += 1) {
      expect(engine.effectiveStep(ch)).toBe(0);
    }
  });

  it("moves the revision on every handled gesture and refused setting apart", () => {
    const { ui } = rig();
    const start = ui.revision;
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.revision).not.toBe(start);
    const afterRotate = ui.revision;
    expect(ui.setTempo(174)).toBe(true);
    expect(ui.revision).not.toBe(afterRotate);
    const afterTempo = ui.revision;
    expect(ui.setTempo(MAX_TEMPO + 1)).toBe(false);
    expect(ui.setClockSource(CLOCK_SOURCE_COUNT)).toBe(false);
    expect(ui.revision).toBe(afterTempo);
  });

  it("never repeats a revision on two consecutive gestures", () => {
    const { ui } = rig();
    let previous = ui.revision;
    for (let i = 0; i < 600; ++i) {
      ui.handle(UiEvent.Rotate, 1);
      expect(ui.revision).not.toBe(previous);
      previous = ui.revision;
    }
  });

  it("shift press is deliberately free and changes nothing", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    for (let i = 0; i < 3; ++i) ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.ShiftPress);
    expect(ui.level).toBe(UiLevel.Edit);
    expect(ui.stepCursor).toBe(3);
    expect(engine.patternForChannel(0)!.readStep(3)).toBe(false);
  });
});

describe("UiController — le parametre principal, edite depuis la barre avec SHIFT", () => {
  it("the main parameter follows the channel mode", () => {
    const { ui, engine, gotoTab } = rig();
    expect(ui.mainField).toBe(UiField.Pattern);
    engine.setChannelMode(0, ChannelMode.CLOCK);
    expect(ui.mainField).toBe(UiField.Subdiv);
    engine.setChannelMode(0, ChannelMode.RANDOM);
    expect(ui.mainField).toBe(UiField.SkipChance);

    gotoTab(TAB_CLOCK);
    expect(ui.mainField).toBe(UiField.Tempo);
    gotoTab(TAB_SETTINGS);
    expect(ui.mainField).toBe(UiField.None);
  });

  it("the interface caps the skip chance at nine", () => {
    const { ui, engine } = rig();
    engine.setChannelMode(0, ChannelMode.RANDOM);
    for (let i = 0; i < 20; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSkipChance(0)).toBe(9);
  });

  it("the engine still accepts a skip chance of ten", () => {
    const { engine } = rig();
    expect(engine.setSkipChance(0, 10)).toBe(true);
    expect(engine.getSkipChance(0)).toBe(10);
    expect(engine.setSkipChance(0, 11)).toBe(false);
    expect(engine.getSkipChance(0)).toBe(10);
  });

  it("shift rotate on the bar changes the main parameter", () => {
    {
      const { ui, engine } = rig();
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(ui.displayedPattern, "le NOM change").toBe(1);
      expect(engine.getSelectedPattern(0), "et rien n est charge").toBe(0);
    }
    {
      const { ui, engine } = rig();
      engine.setChannelMode(0, ChannelMode.CLOCK);
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(engine.getSubdiv(0)).toBe(-2);
    }
    {
      const { ui, engine } = rig();
      engine.setChannelMode(0, ChannelMode.RANDOM);
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(engine.getSkipChance(0)).toBe(1);
    }
    {
      const { ui, gotoTab } = rig();
      gotoTab(TAB_CLOCK);
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(ui.tempo).toBe(DEFAULT_TEMPO + 1);
    }
  });

  it("shift rotate on the bar moves nothing else", () => {
    const { ui, engine } = rig();
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.level).toBe(UiLevel.TabBar);
    expect(ui.currentTab).toBe(TAB_FIRST_CHANNEL);
    expect(ui.cursor).toBe(0);
    expect(ui.fieldOpen).toBe(false);
    expect(engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH);
  });

  it("shift rotate on the settings tab changes nothing", () => {
    const { ui, engine, gotoTab } = rig();
    gotoTab(TAB_SETTINGS);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.tempo).toBe(DEFAULT_TEMPO);
    expect(engine.getSelectedPattern(0)).toBe(0);
  });

  it("shift play is reserved and does not toggle the transport", () => {
    const { ui, engine } = rig();
    expect(engine.isRunning).toBe(false);
    ui.handle(UiEvent.ShiftPlayPress);
    expect(engine.isRunning).toBe(false);
    ui.handle(UiEvent.PlayPress);
    expect(engine.isRunning).toBe(true);
    ui.handle(UiEvent.ShiftPlayPress);
    expect(engine.isRunning).toBe(true);
  });
});

describe("PLAY et la source d horloge", () => {
  it("ne fait rien quand l horloge n est pas interne", () => {
    const r = rig();
    expect(r.ui.setClockSource(1)).toBe(true);
    r.ui.handle(UiEvent.PlayPress);
    expect(r.engine.isRunning).toBe(false);

    r.engine.start();
    r.ui.handle(UiEvent.PlayPress);
    expect(r.engine.isRunning).toBe(true);

    expect(r.ui.setClockSource(0)).toBe(true);
    r.ui.handle(UiEvent.PlayPress);
    expect(r.engine.isRunning).toBe(false);
  });
});

describe("UiController — step cursor bound", () => {
  // 35 et 36 en toutes lettres : une boucle bornee par STEP_COUNT suivrait la
  // constante et ne prouverait rien de sa valeur.
  it("reaches 35 and wraps at 36", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    expect(ui.stepCursor).toBe(0);
    for (let i = 0; i < 35; ++i) ui.handle(UiEvent.Rotate, 1);
    expect(ui.stepCursor).toBe(35);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.stepCursor).toBe(0);
    expect(STEP_COUNT).toBe(36);
  });
});

function modeRig(mode: ChannelMode) {
  const engine = new SequencerEngine();
  engine.setPatternBank(new PatternBank());
  for (let ch = 0; ch < engine.channelCount(); ++ch) engine.setChannelMode(ch, mode);
  const transport = new Transport(engine);
  const ui = new UiController(engine, transport);
  ui.handle(UiEvent.Press);
  return { engine, transport, ui };
}

describe("UiController — les trois modes et leurs champs", () => {
  it("un onglet CLOCK porte les trois lignes de l'original", () => {
    const { ui } = modeRig(ChannelMode.CLOCK);
    expect(ui.fieldCount).toBe(3);
    expect(ui.fieldAt(0)).toBe(UiField.Mode);
    expect(ui.fieldAt(1)).toBe(UiField.Offset);
    expect(ui.fieldAt(2)).toBe(UiField.Mod);
  });

  it("un onglet RANDOM met la subdivision sur la deuxieme ligne", () => {
    const { ui } = modeRig(ChannelMode.RANDOM);
    expect(ui.fieldCount).toBe(3);
    expect(ui.fieldAt(0)).toBe(UiField.Mode);
    expect(ui.fieldAt(1)).toBe(UiField.Subdiv);
    expect(ui.fieldAt(2)).toBe(UiField.Mod);
  });


  it("les index publies sont d'accord avec fieldAt", () => {
    const { ui } = modeRig(ChannelMode.SEQ);
    expect(SEQ_FIELD_INDEX_PATTERN).toBe(0);
    expect(SEQ_FIELD_INDEX_MODE).toBe(1);
    expect(SEQ_FIELD_INDEX_EDIT_ENTRY).toBe(2);
    expect(SEQ_FIELD_INDEX_CONFIG).toBe(3);
    expect(ui.fieldAt(SEQ_FIELD_INDEX_MODE)).toBe(UiField.Mode);
    expect(ui.fieldAt(SEQ_FIELD_INDEX_EDIT_ENTRY)).toBe(UiField.EditEntry);
    expect(ui.fieldAt(SEQ_FIELD_INDEX_CONFIG)).toBe(UiField.Config);
  });

  it("la page CONFIG porte LENGTH, SUBDIV et MOD", () => {
    const { ui } = modeRig(ChannelMode.SEQ);
    // ⚠️ On tourne JUSQU AU champ, jamais d un nombre de crans fixe : une
    // position ajoutee a l onglet casserait le compte, et c est arrive.
    while (ui.field !== UiField.Config) ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    expect(ui.isOnConfigPage).toBe(true);
    expect(ui.fieldCount).toBe(3);
    expect(CONFIG_FIELD_INDEX_LENGTH).toBe(0);
    expect(CONFIG_FIELD_INDEX_SUBDIV).toBe(1);
    expect(ui.fieldAt(CONFIG_FIELD_INDEX_LENGTH)).toBe(UiField.Length);
    expect(ui.fieldAt(CONFIG_FIELD_INDEX_SUBDIV)).toBe(UiField.Subdiv);
    expect(ui.fieldAt(2)).toBe(UiField.Mod);
    expect(ui.field).toBe(UiField.Length);
  });

  it("un appui long quitte la page CONFIG pour l'onglet", () => {
    const { ui } = modeRig(ChannelMode.SEQ);
    while (ui.field !== UiField.Config) ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.LongPress);
    expect(ui.isOnConfigPage).toBe(false);
    expect(ui.level).toBe(UiLevel.Tab);
    expect(ui.field).toBe(UiField.Config);
  });

  // ⚠️ QUATRE positions de curseur, mais TOUJOURS TROIS LIGNES. La grande
  // valeur n est pas une ligne : c est le grand chiffre a gauche.
  it("un onglet SEQ prend les trois lignes de l'original", () => {
    const { ui } = modeRig(ChannelMode.SEQ);
    expect(ui.fieldCount).toBe(4);
    expect(ui.fieldAt(0)).toBe(UiField.Pattern);
    expect(ui.fieldAt(1)).toBe(UiField.Mode);
    expect(ui.fieldAt(2)).toBe(UiField.EditEntry);
    expect(ui.fieldAt(3)).toBe(UiField.Config);
  });

  it("SEP vit dans l'en-tete de l'ecran EDIT", () => {
    const { ui, engine } = modeRig(ChannelMode.SEQ);
    while (ui.field !== UiField.EditEntry) ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    expect(ui.level).toBe(UiLevel.Edit);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.isOnHeader).toBe(true);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.isOnHeader, "l en-tete est un butoir").toBe(true);
    ui.handle(UiEvent.Press);
    const before = engine.getBarLength(0);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getBarLength(0)).not.toBe(before);
    ui.handle(UiEvent.LongPress);
    expect(ui.isOnHeader).toBe(false);
    expect(ui.stepCursor).toBe(0);
    expect(ui.level).toBe(UiLevel.Edit);
  });

  it("on peut toujours ressortir de SEQ", () => {
    const { ui, engine } = modeRig(ChannelMode.SEQ);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field, "MODE reste a un cran de la grande valeur").toBe(UiField.Mode);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, -1);
    expect(engine.getChannelMode(0)).toBe(ChannelMode.RANDOM);
    ui.handle(UiEvent.Rotate, -1);
    expect(engine.getChannelMode(0)).toBe(ChannelMode.CLOCK);
  });

  it("le champ MODE parcourt les trois modes", () => {
    const { ui, engine } = modeRig(ChannelMode.CLOCK);
    expect(ui.field).toBe(UiField.Mode);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getChannelMode(0)).toBe(ChannelMode.RANDOM);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getChannelMode(0)).toBe(ChannelMode.SEQ);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getChannelMode(0)).toBe(ChannelMode.SEQ);
  });

  it("le champ OFFSET deplace l'offset d'un pas par cran", () => {
    const { ui, engine } = modeRig(ChannelMode.CLOCK);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field).toBe(UiField.Offset);
    ui.handle(UiEvent.Press);
    const before = engine.getOffset(0);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getOffset(0)).toBe(before + 1);
    ui.handle(UiEvent.Rotate, -1);
    expect(engine.getOffset(0)).toBe(before);
  });

  it("l'offset ne descend jamais sous zero", () => {
    const { ui, engine } = modeRig(ChannelMode.CLOCK);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    for (let i = 0; i < 5; i += 1) ui.handle(UiEvent.Rotate, -1);
    expect(engine.getOffset(0)).toBe(0);
  });

  it("l'offset s'arrete au dernier tick du pas", () => {
    const { ui, engine } = modeRig(ChannelMode.CLOCK);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    for (let i = 0; i < 200; i += 1) ui.handle(UiEvent.Rotate, 1);
    expect(engine.getOffset(0)).toBe(95);
  });

  it("un pas de plus de 255 ticks garde toute la plage d'offset", () => {
    const { ui, engine } = modeRig(ChannelMode.CLOCK);
    engine.setSubdiv(0, 4);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    for (let i = 0; i < 400; i += 1) ui.handle(UiEvent.Rotate, 1);
    expect(engine.getOffset(0)).toBe(255);
  });

  it("le champ MOD est navigable et ne fait rien encore", () => {
    const { ui, engine } = modeRig(ChannelMode.CLOCK);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field).toBe(UiField.Mod);
    const a1 = engine.getCvDestination(0, 0);
    const a2 = engine.getCvDestination(0, 1);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    expect(engine.getCvDestination(0, 0)).toBe(a1);
    expect(engine.getCvDestination(0, 1)).toBe(a2);
  });
});

describe("le champ MOD, PRD 10.2", () => {
  const initiale = (d: CvDestination): string =>
    d === CvDestination.PATTERN ? "P"
    : d === CvDestination.LENGTH ? "L"
    : d === CvDestination.RESET ? "R"
    : d === CvDestination.STEP ? "S" : "-";

  it("porte les vingt et une valeurs du PRD, dans leur ordre", () => {
    const attendu = [
      "-/-",
      "P/-", "L/-", "R/-", "S/-",
      "-/P", "-/L", "-/R", "-/S",
      "P/L", "P/R", "P/S",
      "L/P", "L/R", "L/S",
      "R/P", "R/L", "R/S",
      "S/P", "S/L", "S/R",
    ];
    expect(MOD_CHOICE_COUNT).toBe(21);
    for (let index = 0; index < 21; ++index) {
      const [a, b] = modChoiceAt(index);
      expect(`${initiale(a)}/${initiale(b)}`, `valeur ${index}`).toBe(attendu[index]);
      expect(modIndexOf(a, b), `retour ${index}`).toBe(index);
    }
  });

  it("ne peut pas produire deux entrees sur la meme destination", () => {
    for (const d of [CvDestination.PATTERN, CvDestination.LENGTH,
                     CvDestination.RESET, CvDestination.STEP]) {
      expect(modIndexOf(d, d)).toBe(-1);
    }
  });

  it("une rotation pose les DEUX destinations et ecrete aux deux bouts", () => {
    const r = rig();
    r.enterTab();
    r.gotoField(UiField.Mod);
    r.ui.handle(UiEvent.Press);

    for (let i = 0; i < 9; ++i) r.ui.handle(UiEvent.Rotate, 1);
    expect(r.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.PATTERN);
    expect(r.engine.getCvDestination(0, CV_SOURCE_2)).toBe(CvDestination.LENGTH);

    for (let i = 0; i < 40; ++i) r.ui.handle(UiEvent.Rotate, 1);
    expect(r.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.STEP);
    expect(r.engine.getCvDestination(0, CV_SOURCE_2)).toBe(CvDestination.RESET);

    for (let i = 0; i < 40; ++i) r.ui.handle(UiEvent.Rotate, -1);
    expect(r.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.NONE);
    expect(r.engine.getCvDestination(0, CV_SOURCE_2)).toBe(CvDestination.NONE);
  });

  it("un canal en CLOCK ne modifie pas son routage", () => {
    const r = rig();
    r.engine.setChannelMode(0, ChannelMode.CLOCK);
    r.enterTab();
    r.ui.handle(UiEvent.Rotate, 1);
    r.ui.handle(UiEvent.Rotate, 1);
    expect(r.ui.field, "la ligne 3 d un onglet CLOCK est MOD").toBe(UiField.Mod);
    r.ui.handle(UiEvent.Press);
    for (let i = 0; i < 5; ++i) r.ui.handle(UiEvent.Rotate, 1);
    expect(r.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.NONE);
    expect(r.engine.getCvDestination(0, CV_SOURCE_2)).toBe(CvDestination.NONE);
  });
});

/*
 * PRD 5.0, amendment of 2026-09-15 — the pattern is chosen in its own field, a
 * short press loads it, and nothing ever asks: the patterns are shared.
 *
 * Miroir de `test/test_ui_controller/test_ui_controller.cpp`.
 */
describe("UiController — le pattern se choisit dans son champ", () => {
  const seqChannelOnPattern = () => {
    const bank = new PatternBank();
    const engine = new SequencerEngine();
    engine.setPatternBank(bank);
    for (let ch = 0; ch < engine.channelCount(); ++ch) {
      engine.setChannelMode(ch, ChannelMode.SEQ);
    }
    const ui = new UiController(engine, new Transport(engine));
    ui.handle(UiEvent.Press);
    expect(ui.field).toBe(UiField.Pattern);
    return { bank, engine, ui };
  };

  it("un onglet SEQ met la grande valeur en premier", () => {
    const { ui } = seqChannelOnPattern();
    expect(ui.fieldCount).toBe(4);
    expect(ui.fieldAt(0)).toBe(UiField.Pattern);
    expect(ui.fieldAt(1)).toBe(UiField.Mode);
    expect(ui.fieldAt(2)).toBe(UiField.EditEntry);
    expect(ui.fieldAt(3)).toBe(UiField.Config);
  });

  it("sans choix en cours, l ecran nomme ce que le canal joue", () => {
    const { engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    expect(ui.displayedPattern).toBe(6);
  });

  it("SHIFT plus rotation nomme le pattern et ne charge rien", () => {
    const { bank, engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.displayedPattern).toBe(7);
    expect(engine.getSelectedPattern(0)).toBe(6);
    expect(engine.patternForChannel(0)).toBe(bank.getPattern(6));
  });

  it("le champ ouvert nomme le pattern aussi", () => {
    const { engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(true);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.displayedPattern).toBe(8);
    expect(engine.getSelectedPattern(0)).toBe(6);
  });

  it("le nom s arrete aux deux bouts", () => {
    const { engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 0);
    for (let i = 0; i < 20; ++i) ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.displayedPattern).toBe(15);
    for (let i = 0; i < 20; ++i) ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.displayedPattern).toBe(0);
  });

  it("un appui court charge le pattern montre, et le canal le joue", () => {
    const { bank, engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    bank.getPattern(7)!.writeStep(2, true);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(false);
    expect(engine.getSelectedPattern(0)).toBe(7);
    expect(engine.patternForChannel(0)).toBe(bank.getPattern(7));
    expect(engine.patternForChannel(0)!.readStep(2)).toBe(true);
    expect(ui.displayedPattern).toBe(7);
  });

  it("un pattern edite charge sans question, et garde son edition", () => {
    const { bank, engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    bank.getPattern(6)!.writeStep(4, true);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.Press);
    expect(ui.fieldOpen).toBe(false);
    expect(engine.getSelectedPattern(0)).toBe(7);
    expect(bank.getPattern(6)!.readStep(4)).toBe(true);
  });

  it("un appui long ferme sans charger", () => {
    const { engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.displayedPattern).toBe(7);
    ui.handle(UiEvent.LongPress);
    expect(ui.fieldOpen).toBe(false);
    expect(engine.getSelectedPattern(0)).toBe(6);
    expect(ui.displayedPattern).toBe(6);
  });

  it("changer d onglet abandonne le choix", () => {
    const { engine, ui } = seqChannelOnPattern();
    engine.setSelectedPattern(0, 6);
    engine.setSelectedPattern(1, 2);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.displayedPattern).toBe(7);
    ui.handle(UiEvent.LongPress);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.displayedPattern).toBe(2);
  });
});

/*
 * PRD 5.0, amendment of 2026-09-15 — EDIT is read only while a source of the
 * channel is routed to PATTERN.
 */
describe("UiController — EDIT en lecture seule sous un routage PATTERN", () => {
  const enterEditOf = (r: ReturnType<typeof rig>) => {
    r.gotoTab(TAB_FIRST_CHANNEL);
    r.ui.handle(UiEvent.Press);
    while (r.ui.field !== UiField.EditEntry) r.ui.handle(UiEvent.Rotate, 1);
    r.ui.handle(UiEvent.Press);
    expect(r.ui.level).toBe(UiLevel.Edit);
  };

  it("un canal non route s edite", () => {
    const r = rig();
    expect(r.ui.editIsReadOnly).toBe(false);
    enterEditOf(r);
    r.ui.handle(UiEvent.Press);
    expect(r.engine.patternForChannel(0)!.readStep(0)).toBe(true);
  });

  it("un routage PATTERN sur la premiere source rend EDIT en lecture seule", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    r.engine.patternForChannel(0)!.writeStep(3, true);
    enterEditOf(r);
    expect(r.ui.editIsReadOnly).toBe(true);
    r.ui.handle(UiEvent.Press);
    expect(r.engine.patternForChannel(0)!.readStep(0)).toBe(false);
    for (let i = 0; i < 3; ++i) r.ui.handle(UiEvent.Rotate, 1);
    expect(r.ui.stepCursor).toBe(3);
    r.ui.handle(UiEvent.ShiftRotate, 1);
    expect(r.engine.patternForChannel(0)!.getRatchet(3)).toBe(RATCHET_NONE);
    r.ui.handle(UiEvent.ShiftLongPress);
    expect(r.engine.patternForChannel(0)!.readStep(3)).toBe(true);
  });

  it("un routage PATTERN sur la seconde source aussi", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_2, CvDestination.PATTERN);
    enterEditOf(r);
    expect(r.ui.editIsReadOnly).toBe(true);
    r.ui.handle(UiEvent.Press);
    expect(r.engine.patternForChannel(0)!.readStep(0)).toBe(false);
  });

  it("un autre routage ne bloque pas l edition", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    r.engine.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP);
    expect(r.ui.editIsReadOnly).toBe(false);
    enterEditOf(r);
    r.ui.handle(UiEvent.Press);
    expect(r.engine.patternForChannel(0)!.readStep(0)).toBe(true);
  });

  it("SEP reste reglable sous un routage PATTERN", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    enterEditOf(r);
    const before = r.engine.getBarLength(0);
    r.ui.handle(UiEvent.Rotate, -1);
    expect(r.ui.isOnHeader).toBe(true);
    r.ui.handle(UiEvent.Press);
    r.ui.handle(UiEvent.Rotate, 1);
    expect(r.engine.getBarLength(0)).not.toBe(before);
  });

  it("la lecture seule suit le canal de l onglet", () => {
    const r = rig();
    r.engine.setCvDestination(1, CV_SOURCE_1, CvDestination.PATTERN);
    r.gotoTab(TAB_FIRST_CHANNEL);
    expect(r.ui.editIsReadOnly).toBe(false);
    r.gotoTab(TAB_FIRST_CHANNEL + 1);
    expect(r.ui.editIsReadOnly).toBe(true);
    r.gotoTab(TAB_CLOCK);
    expect(r.ui.editIsReadOnly).toBe(false);
  });

  it("retirer le routage rend l edition", () => {
    const r = rig();
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.PATTERN);
    enterEditOf(r);
    r.ui.handle(UiEvent.Press);
    expect(r.engine.patternForChannel(0)!.readStep(0)).toBe(false);
    r.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.NONE);
    expect(r.ui.editIsReadOnly).toBe(false);
    r.ui.handle(UiEvent.Press);
    expect(r.engine.patternForChannel(0)!.readStep(0)).toBe(true);
  });
});
