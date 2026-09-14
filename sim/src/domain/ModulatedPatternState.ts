import { CHANNEL_COUNT } from "./SequencerEngine.js";

/**
 * Le MIROIR PARTIEL de `ModulatedPatternState`, l etat que le C++ tient hors du
 * moteur — `include/flexseq/SequencerEngine.h`.
 *
 * ⚠️ TypeScript n en porte QUE le drapeau de changement, un bit par canal. Le
 * tampon de modulation, son service, et l editeur de templates n existent pas de
 * ce cote : c est l ecart ouvert que `docs/open-risks.md` suit, et le
 * proprietaire ne l a pas encore tranche.
 *
 * La forme et les noms sont ceux du C++, deliberement. Le jour ou le reste
 * arrive, il pousse ICI au lieu d etre reconcilie avec une autre forme.
 */
export class ModulatedPatternState {
  // PRD 5.0 amendement 1bis : la copie du canal differe du template qu il a
  // charge. Il garde le chargement, et il commande l apparition de SAVE. Il
  // n est JAMAIS persiste.
  private dirty = 0;

  isDirty(channel: number): boolean {
    if (!Number.isInteger(channel) || channel < 0 || channel >= CHANNEL_COUNT) {
      return false;
    }
    return (this.dirty & (1 << channel)) !== 0;
  }

  markDirty(channel: number): void {
    if (!Number.isInteger(channel) || channel < 0 || channel >= CHANNEL_COUNT) {
      return;
    }
    this.dirty |= 1 << channel;
  }

  clearDirty(channel: number): void {
    if (!Number.isInteger(channel) || channel < 0 || channel >= CHANNEL_COUNT) {
      return;
    }
    this.dirty &= ~(1 << channel);
  }
}
