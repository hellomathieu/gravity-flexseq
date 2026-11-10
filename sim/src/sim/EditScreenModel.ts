import type { SequencerEngine } from "../domain/SequencerEngine.js";
import type { UiController } from "../domain/UiController.js";
import { RATCHET_NONE } from "../domain/Pattern.js";
import { editCursorFor } from "../domain/EditTitle.js";
import { GRID_STEPS, type PatternScreenPixelModel } from "./PatternScreenPixels.js";

export function editScreenModelOf(
  ui: UiController,
  engine: SequencerEngine,
  channel: number,
  title: string | null,
): PatternScreenPixelModel {
  const pattern = engine.patternForChannel(channel);
  const steps = new Array<boolean>(GRID_STEPS).fill(false);
  const ratchets = new Array<number>(GRID_STEPS).fill(RATCHET_NONE);
  if (pattern !== null) {
    for (let i = 0; i < GRID_STEPS; ++i) {
      steps[i] = pattern.readStep(i) === true;
      ratchets[i] = pattern.getRatchet(i);
    }
  }
  const sepSelected = ui.isOnHeader;
  return {
    title,
    steps,
    ratchets,
    length: engine.getEffectiveLength(channel),
    cursor: editCursorFor(ui),
    playhead: engine.effectiveStep(channel),
    barLength: engine.getBarLength(channel),
    sepSelected,
    sepOpen: sepSelected && ui.fieldOpen,
    ticksPerStep: engine.getTicksPerStep(channel),
  };
}
