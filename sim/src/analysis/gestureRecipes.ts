import {
  STEP_COUNT,
  TAB_COUNT,
  UiController,
  UiEvent,
  UiField,
  UiLevel,
} from '../domain/UiController.js';
import type { PatternBank } from '../domain/PatternBank.js';
import type { SequencerEngine } from '../domain/SequencerEngine.js';
import { RATCHET_CODES, ratchetFitsStep } from '../domain/Pattern.js';

export interface Gesture {
  event: UiEvent;
  delta: number;
}

export interface RecipeOutcome {
  applied: boolean;
  reason?: string;
}

export class GestureDriver {
  private readonly log: Gesture[] = [];

  constructor(
    private readonly ui: UiController,
    private readonly bank: PatternBank,
    private readonly engine: SequencerEngine,
  ) {}

  get events(): readonly Gesture[] {
    return this.log;
  }

  rotate(detents: number): void {
    const step = detents >= 0 ? 1 : -1;
    for (let i = 0; i < Math.abs(detents); ++i) this.emit(UiEvent.Rotate, step);
  }

  shiftRotate(detents: number): void {
    const step = detents >= 0 ? 1 : -1;
    for (let i = 0; i < Math.abs(detents); ++i) this.emit(UiEvent.ShiftRotate, step);
  }

  press(): void {
    this.emit(UiEvent.Press, 0);
  }

  longPress(): void {
    this.emit(UiEvent.LongPress, 0);
  }

  goToTab(tab: number): void {
    for (let guard = 0; guard <= TAB_COUNT; ++guard) {
      if (this.ui.currentTab === tab) break;
      this.rotate(1);
      if (guard === TAB_COUNT) throw new Error(`onglet ${tab} jamais atteint`);
    }
    if (this.ui.level === UiLevel.TabBar) this.press();
  }

  selectField(field: UiField): void {
    for (let guard = 0; guard <= this.ui.fieldCount; ++guard) {
      if (this.ui.field === field) return;
      this.rotate(1);
      if (guard === this.ui.fieldCount) throw new Error(`champ ${field} jamais atteint`);
    }
  }

  enterEdit(): void {
    this.selectField(UiField.EditEntry);
    this.press();
  }

  backOneLevel(): void {
    this.longPress();
  }

  setLength(detents: number): void {
    this.selectField(UiField.Length);
    this.shiftRotate(detents);
  }

  setSubdiv(detents: number): void {
    this.selectField(UiField.Subdiv);
    this.shiftRotate(detents);
  }

  moveStepCursor(step: number): void {
    for (let guard = 0; guard <= STEP_COUNT; ++guard) {
      if (this.ui.stepCursor === step) return;
      this.rotate(1);
      if (guard === STEP_COUNT) throw new Error(`step ${step} jamais atteint`);
    }
  }

  toggleStep(step: number): void {
    this.moveStepCursor(step);
    this.press();
  }

  setRatchet(step: number, code: number): RecipeOutcome {
    const channel = this.ui.selectedChannel;
    if (channel < 0) return { applied: false, reason: 'aucun channel selectionne' };

    const pattern = this.engine.patternForChannel(channel);
    if (pattern === null) return { applied: false, reason: 'pattern introuvable' };

    const targetIndex = RATCHET_CODES.indexOf(code);
    if (targetIndex < 0) return { applied: false, reason: `code ${code} hors de la liste` };

    this.moveStepCursor(step);

    if (pattern.readStep(step) !== true) {
      return { applied: false, reason: `step ${step} inactif : le firmware refuse le ratchet` };
    }

    const ticks = this.engine.getTicksPerStep(channel);
    if (!ratchetFitsStep(code, ticks)) {
      return {
        applied: false,
        reason: `cadence : le code ${code} ne tient pas dans un step de ${ticks} ticks`,
      };
    }

    let current = pattern.getRatchet(step);
    if (current === code) return { applied: true };

    const direction = RATCHET_CODES.indexOf(current) < targetIndex ? 1 : -1;
    for (let tried = 0; tried < RATCHET_CODES.length; ++tried) {
      this.shiftRotate(direction);
      const next = pattern.getRatchet(step);
      if (next === code) return { applied: true };
      if (next === current) {
        return { applied: false, reason: `cible ${code} inatteignable : la rotation ne bouge plus` };
      }
      const nextIndex = RATCHET_CODES.indexOf(next);
      if ((direction > 0 && nextIndex > targetIndex) || (direction < 0 && nextIndex < targetIndex)) {
        return { applied: false, reason: `cadence : la cible ${code} est sautee a ${ticks} ticks` };
      }
      current = next;
    }
    return { applied: false, reason: `cible ${code} non atteinte apres ${RATCHET_CODES.length} crans` };
  }

  private emit(event: UiEvent, delta: number): void {
    this.log.push({ event, delta });
    this.ui.handle(event, delta);
  }
}
