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
import { DEFAULT_SUBDIV, SUBDIVS } from "../src/domain/subdiv.js";
import { CvDestination, CV_SOURCE_1 } from "../src/domain/CvDestination.js";

function rig() {
  const engine = new SequencerEngine();
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
    for (let i = 0; i < 19; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.getEffectiveLength(0)).toBe(35);
    ui.handle(UiEvent.ShiftRotate, 3);
    expect(engine.getEffectiveLength(0)).toBe(36);

    for (let i = 0; i < 34; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.getEffectiveLength(0)).toBe(2);
    ui.handle(UiEvent.ShiftRotate, -3);
    expect(engine.getEffectiveLength(0)).toBe(1);

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

  it("rotate moves the step cursor and wraps at thirty six", () => {
    const { ui, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Rotate, -1);
    expect(ui.stepCursor).toBe(35);
    ui.handle(UiEvent.Rotate, 1);
    expect(ui.stepCursor).toBe(0);
    for (let i = 0; i < 5; ++i) ui.handle(UiEvent.Rotate, 5);
    expect(ui.stepCursor).toBe(5);
  });

  it("press toggles the step under the cursor", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    for (let i = 0; i < 7; ++i) ui.handle(UiEvent.Rotate, 1);
    expect(engine.instanceForChannel(0)!.readStep(7)).toBe(false);
    ui.handle(UiEvent.Press);
    expect(engine.instanceForChannel(0)!.readStep(7)).toBe(true);
    ui.handle(UiEvent.Press);
    expect(engine.instanceForChannel(0)!.readStep(7)).toBe(false);
  });

  it("shift rotate sets the ratchet of an active step and clamps", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_2);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_3);
    for (let i = 0; i < RATCHET_CODES.length + 3; i += 1) ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_TRIPLET);
    for (let i = 0; i < RATCHET_CODES.length + 3; i += 1) ui.handle(UiEvent.ShiftRotate, -1);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_NONE);
  });

  it("a ratchet edit takes effect on the current step immediately", () => {
    const { ui, engine, enterEdit } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    engine.start();
    expect(engine.currentStepTicks(0)).toBe(PPQN);
    for (let i = 0; i < 5; ++i) ui.handle(UiEvent.ShiftRotate, 5);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_TRIPLET);
    expect(engine.currentStepTicks(0)).toBe(2 * PPQN);
  });

  it("shift rotate in edit no longer changes channel", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    ui.handle(UiEvent.Press);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(ui.selectedChannel).toBe(0);
    expect(ui.level).toBe(UiLevel.Edit);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_2);
  });

  it("shift rotate on an inactive step does nothing", () => {
    const { ui, enterEdit, engine } = rig();
    enterEdit();
    expect(engine.instanceForChannel(0)!.readStep(0)).toBe(false);
    ui.handle(UiEvent.ShiftRotate, 1);
    expect(engine.instanceForChannel(0)!.getRatchet(0)).toBe(RATCHET_NONE);
  });

  it("the grid follows the pattern of the channel selected in edit", () => {
    const { ui, engine, enterEdit, gotoTab } = rig();
    engine.setSelectedPattern(1, 5);
    gotoTab(TAB_FIRST_CHANNEL + 1);
    enterEdit();
    ui.handle(UiEvent.Press);
    expect(engine.instanceForChannel(1)!.readStep(0)).toBe(true);
    expect(engine.instanceForChannel(0)!.readStep(0)).toBe(false);
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
      expect(engine.instanceForChannel(0)!.readStep(step)).toBe(false);
      expect(engine.instanceForChannel(0)!.getRatchet(step)).toBe(RATCHET_NONE);
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
    expect(engine.instanceForChannel(0)!.readStep(3)).toBe(false);
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

  it("shift rotate on the bar changes the main parameter", () => {
    {
      const { ui, engine } = rig();
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(engine.getSelectedPattern(0)).toBe(1);
    }
    {
      const { ui, engine } = rig();
      engine.setChannelMode(0, ChannelMode.CLOCK);
      ui.handle(UiEvent.ShiftRotate, 1);
      expect(engine.getSubdiv(0)).toBe(2);
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
