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
// PRD 5.0 amendement 1ter : les six copies comptent comme CHANGEES a chaque
// demarrage. Le drapeau vit en RAM et aucun record de 11.1 ne le porte, donc une
// coupure laisse le module incapable de distinguer une copie editee d une copie
// propre. Ce n est pas une precaution : c est la verite.
export const ALL_CHANNELS_DIRTY = (1 << CHANNEL_COUNT) - 1;

export class ModulatedPatternState {
  // PRD 5.0 amendement 1ter : la copie du canal differe du template qu il a
  // charge. Il garde le chargement : une copie changee mange le premier cran et
  // pose la question. Il n est JAMAIS persiste, et c est pourquoi les six bits
  // partent a un.
  private dirty = ALL_CHANNELS_DIRTY;

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
