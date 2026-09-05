import {
  ChannelMode,
  DEFAULT_CHANNEL_MODE,
  type SequencerEngine,
} from "./SequencerEngine.js";
import { CV_SOURCE_1, CV_SOURCE_2, CvDestination } from "./CvDestination.js";
import { UiField, UiLevel, type UiController } from "./UiController.js";

export enum MainParameter {
  None,
  Tempo,
  Subdiv,
  SkipChance,
  Pattern,
}

export interface MainScreenModel {
  tab: number;
  insideTab: boolean;
  cursor: number;
  fieldOpen: boolean;
  fieldCount: number;

  patternIndex: number;
  length: number;
  subdiv: number;
  barLength: number;

  mode: ChannelMode;
  offset: number;
  skipChance: number;
  stepTicks: number;
  mainParameter: MainParameter;

  cv1Target: number;
  cv2Target: number;
  configPage: boolean;

  tempo: number;
  clockSource: number;
}

function parameterOf(field: UiField): MainParameter {
  switch (field) {
    case UiField.Tempo:
      return MainParameter.Tempo;
    case UiField.Subdiv:
      return MainParameter.Subdiv;
    case UiField.SkipChance:
      return MainParameter.SkipChance;
    case UiField.Pattern:
      return MainParameter.Pattern;
    default:
      return MainParameter.None;
  }
}

export function mainScreenModelOf(ui: UiController, engine: SequencerEngine): MainScreenModel {
  const channel = ui.selectedChannel;
  const model: MainScreenModel = {
    tab: ui.currentTab,
    insideTab: ui.level === UiLevel.Tab,
    cursor: ui.cursor,
    fieldOpen: ui.fieldOpen,
    fieldCount: ui.fieldCount,
    patternIndex: -1,
    length: 0,
    subdiv: 0,
    barLength: 0,
    mode: DEFAULT_CHANNEL_MODE,
    offset: 0,
    skipChance: 0,
    stepTicks: 0,
    mainParameter: parameterOf(ui.mainField),
    cv1Target: CvDestination.NONE,
    cv2Target: CvDestination.NONE,
    configPage: ui.isOnConfigPage,
    tempo: ui.tempo,
    clockSource: ui.clockSource,
  };
  if (channel >= 0) {
    model.patternIndex = engine.getSelectedPattern(channel);
    model.length = engine.getBaseLength(channel);
    model.subdiv = engine.getSubdiv(channel);
    model.barLength = engine.getBarLength(channel);
    model.mode = engine.getChannelMode(channel);
    model.offset = engine.getOffset(channel);
    model.skipChance = engine.getSkipChance(channel);
    model.stepTicks = engine.currentStepTicks(channel);
    model.cv1Target = engine.getCvDestination(channel, CV_SOURCE_1);
    model.cv2Target = engine.getCvDestination(channel, CV_SOURCE_2);
  }
  return model;
}
