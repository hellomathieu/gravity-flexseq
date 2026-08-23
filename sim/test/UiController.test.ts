import { describe, expect, it } from "vitest";
import {
  CHANNEL_TAB_FIELDS,
  CLOCK_SOURCE_COUNT,
  CLOCK_TAB_FIELDS,
  DEFAULT_TEMPO,
  MAX_TEMPO,
  MIN_TEMPO,
  STEP_COUNT,
  TAB_CLOCK,
  TAB_COUNT,
  TAB_FIRST_CHANNEL,
  TAB_SETTINGS,
  UiController,
  UiEvent,
  UiField,
  UiLevel,
} from "../src/domain/UiController.js";
import { Transport } from "../src/domain/Transport.js";
import { PatternBank, PATTERN_COUNT } from "../src/domain/PatternBank.js";
import {
  CHANNEL_COUNT,
  DEFAULT_BAR_LENGTH,
  DEFAULT_LENGTH,
  MAX_LENGTH,
  MIN_LENGTH,
  PPQN,
  SequencerEngine,
  ChannelMode,
} from "../src/domain/SequencerEngine.js";
import { RATCHET_CODES, RATCHET_NONE, RATCHET_2, RATCHET_3, RATCHET_TRIPLET } from "../src/domain/Pattern.js";
import { DEFAULT_SUBDIV, SUBDIVS } from "../src/domain/subdiv.js";

function rig() {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  for (let ch = 0; ch < engine.channelCount(); ++ch) {
    engine.setChannelMode(ch, ChannelMode.SEQ);
  }
  engine.setPatternBank(bank);
  const transport = new Transport(engine);
  const ui = new UiController(engine, bank, transport);

  const gotoTab = (tab: number) => {
    while (ui.currentTab !== tab) ui.handle(UiEvent.Rotate, 1);
  };
  const gotoField = (field: UiField) => {
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

  return { bank, engine, transport, ui, gotoTab, gotoField, enterTab, enterEdit };
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
    expect(ui.field).toBe(UiField.Pattern);
  });

  it("press does nothing on the settings tab while it is deferred", () => {
    const { ui, gotoTab, enterTab } = rig();
    gotoTab(TAB_SETTINGS);
    expect(ui.fieldCount).toBe(0);
    enterTab();
    expect(ui.level).toBe(UiLevel.TabBar);
  });

  it("the clock tab exposes tempo then source and owns no channel", () => {
    const { ui, gotoTab } = rig();
    gotoTab(TAB_CLOCK);
    expect(ui.fieldCount).toBe(CLOCK_TAB_FIELDS);
    expect(ui.fieldAt(0)).toBe(UiField.Tempo);
    expect(ui.fieldAt(1)).toBe(UiField.ClockSource);
    expect(ui.selectedChannel).toBe(-1);
  });

  it("the other gestures do nothing on the tab bar", () => {
    const { ui } = rig();
    for (const event of [
      UiEvent.RotateHeld,
      UiEvent.LongPress,
      UiEvent.ShiftRotate,
      UiEvent.ShiftPress,
      UiEvent.ShiftLongPress,
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
    expect(ui.field).toBe(UiField.Length);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.field).toBe(UiField.Pattern);
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.field).toBe(UiField.EditEntry);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.field).toBe(UiField.Pattern);
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

  it("the clock source field never reaches the sentinel", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    gotoField(UiField.ClockSource);
    for (let i = 0; i < CLOCK_SOURCE_COUNT + 5; i += 1) {
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(ui.clockSource).toBeLessThan(CLOCK_SOURCE_COUNT);
    }
    expect(ui.clockSource).toBe(CLOCK_SOURCE_COUNT - 1);
    for (let i = 0; i < CLOCK_SOURCE_COUNT + 5; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.clockSource).toBe(0);
  });

  it("the pattern field is clamped to the bank", () => {
    const { ui, engine, enterTab } = rig();
    enterTab();
    for (let i = 0; i < PATTERN_COUNT + 5; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSelectedPattern(0)).toBe(PATTERN_COUNT - 1);
    for (let i = 0; i < PATTERN_COUNT + 5; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getSelectedPattern(0)).toBe(0);
  });

  it("the length field is clamped to one and twenty four", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Length);
    for (let i = 0; i < MAX_LENGTH + 5; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getEffectiveLength(0)).toBe(MAX_LENGTH);
    for (let i = 0; i < MAX_LENGTH + 5; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getEffectiveLength(0)).toBe(MIN_LENGTH);
  });

  it("keeps the acceleration for the tempo, and only for it", () => {
    const { ui, gotoTab, enterTab, gotoField } = rig();
    gotoTab(TAB_CLOCK);
    enterTab();
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(ui.tempo).toBe(DEFAULT_TEMPO + 3);
    gotoField(UiField.ClockSource);
    const before = ui.clockSource;
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(ui.clockSource).toBe(before + 1);
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
    for (let i = 0; i < 7; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getEffectiveLength(0)).toBe(23);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(engine.getEffectiveLength(0)).toBe(MAX_LENGTH);

    for (let i = 0; i < 22; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getEffectiveLength(0)).toBe(2);
    ui.handle(UiEvent.ShiftRotate, -3);
    expect(engine.getEffectiveLength(0)).toBe(MIN_LENGTH);

    gotoField(UiField.Pattern);
    for (let i = 0; i < 14; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSelectedPattern(0)).toBe(14);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(engine.getSelectedPattern(0)).toBe(PATTERN_COUNT - 1);
  });

  it("the subdiv field walks the libGravity list and clamps", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.Subdiv);
    expect(engine.getSubdiv(0)).toBe(DEFAULT_SUBDIV);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSubdiv(0)).toBe(2);
    ui.handle(UiEvent.ShiftRotate, -2); // ecrete a un pas
    expect(engine.getSubdiv(0)).toBe(DEFAULT_SUBDIV);
    ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getSubdiv(0)).toBe(-2);
    for (let i = 0; i < SUBDIVS.length + 5; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getSubdiv(0)).toBe(SUBDIVS[SUBDIVS.length - 1]);
    for (let i = 0; i < SUBDIVS.length + 5; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getSubdiv(0)).toBe(SUBDIVS[0]);
  });

  it("the bar length field walks only the allowed values", () => {
    const { ui, engine, enterTab, gotoField } = rig();
    enterTab();
    gotoField(UiField.BarLength);
    expect(engine.getBarLength(0)).toBe(DEFAULT_BAR_LENGTH);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getBarLength(0)).toBe(6);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getBarLength(0)).toBe(6);
    for (let i = 0; i < 8; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getBarLength(0)).toBe(0);
    ui.handle(UiEvent.ShiftRotate, 1);
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

  it("rotate moves the step cursor and wraps at twenty four", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.stepCursor).toBe(STEP_COUNT - 1);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.stepCursor).toBe(0);
    for (let i = 0; i < 5; ++i) ui.handle(UiEvent.Rotate, 5);
    expect(ui.stepCursor).toBe(5);
  });

  it("press toggles the step under the cursor", () => {
    const { ui, bank, enterEdit } = rig();
    enterEdit();
    for (let i = 0; i < 7; ++i) ui.handle(UiEvent.Rotate, 1);
    expect(bank.getPattern(0)!.readStep(7)).toBe(false);
    ui.handle(UiEvent.Press);
    expect(bank.getPattern(0)!.readStep(7)).toBe(true);
    ui.handle(UiEvent.Press);
    expect(bank.getPattern(0)!.readStep(7)).toBe(false);
  });

  it("rotate held sets the ratchet and clamps at both ends", () => {
    const { ui, bank, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.RotateHeld, 1);
    expect(bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_2);
    ui.handle(UiEvent.RotateHeld, 1);
    expect(bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_3);
    for (let i = 0; i < RATCHET_CODES.length + 3; i += 1) ui.handle(UiEvent.RotateHeld, 1);
    expect(bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_TRIPLET);
    for (let i = 0; i < RATCHET_CODES.length + 3; i += 1) ui.handle(UiEvent.RotateHeld, -1);
    expect(bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_NONE);
  });

  it("a ratchet edit takes effect on the current step immediately", () => {
    const { ui, engine, bank, enterEdit } = rig();
    enterEdit();
    engine.start();
    expect(engine.currentStepTicks(0)).toBe(PPQN);
    for (let i = 0; i < 5; ++i) ui.handle(UiEvent.RotateHeld, 5);
    expect(bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_TRIPLET);
    expect(engine.currentStepTicks(0)).toBe(2 * PPQN);
  });

  it("shift rotate changes channel and wraps", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.selectedChannel).toBe(1);
    expect(ui.level).toBe(UiLevel.Edit);
    ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.selectedChannel).toBe(0);
    ui.handle(UiEvent.ShiftRotate, -1);
    expect(ui.selectedChannel).toBe(CHANNEL_COUNT - 1);
  });

  it("the grid follows the pattern of the channel selected in edit", () => {
    const { ui, engine, bank, enterEdit } = rig();
    engine.setSelectedPattern(1, 5);
    enterEdit();
    ui.handle(UiEvent.ShiftRotate, 1);
    ui.handle(UiEvent.Press);
    expect(bank.getPattern(5)!.readStep(0)).toBe(true);
    expect(bank.getPattern(0)!.readStep(0)).toBe(false);
  });

  it("shift long press clears the pattern steps and ratchets", () => {
    const { ui, bank, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.RotateHeld, 3);
    ui.handle(UiEvent.Rotate, 4);
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftLongPress);
    for (let step = 0; step < STEP_COUNT; step += 1) {
      expect(bank.getPattern(0)!.readStep(step)).toBe(false);
      expect(bank.getPattern(0)!.getRatchet(step)).toBe(RATCHET_NONE);
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
    const { ui, bank, enterEdit } = rig();
    enterEdit();
    for (let i = 0; i < 3; ++i) ui.handle(UiEvent.Rotate, 1);
    ui.handle(UiEvent.ShiftPress);
    expect(ui.level).toBe(UiLevel.Edit);
    expect(ui.stepCursor).toBe(3);
    expect(bank.getPattern(0)!.readStep(3)).toBe(false);
  });
});
