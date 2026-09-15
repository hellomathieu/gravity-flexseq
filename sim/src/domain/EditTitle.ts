import type { SequencerEngine } from "./SequencerEngine.js";
import type { UiController } from "./UiController.js";

export const LBL_EDIT_TITLE = "EDIT PATTERN A1";
export const LBL_CV_TITLE = "CV PATTERN A1";
export const EDIT_TITLE_CAPACITY = 16;

export function editTitleFor(
  ui: UiController,
  engine: SequencerEngine,
  channel: number,
): string | null {
  const played = engine.patternCvIndex(channel);
  if (played < 0) return null;
  const base = ui.editIsReadOnly ? LBL_CV_TITLE : LBL_EDIT_TITLE;
  const bank = played < 8 ? "A" : "B";
  const number = String.fromCharCode("1".charCodeAt(0) + (played % 8));
  return base.slice(0, base.length - 2) + bank + number;
}

export function editCursorFor(ui: UiController): number {
  if (ui.isOnHeader || ui.editIsReadOnly) return -1;
  return ui.stepCursor;
}
