/**
 * SUBDIV → ticksPerStep (convention libGravity officielle, 96 PPQN).
 *
 * Source de verite : firmware/Gravity/channel.h de libGravity (CLOCK_MOD /
 * CLOCK_MOD_PULSES). L'unite (1) est la NOIRE = 96 ticks. Valeur positive =
 * division (plus lent) => 96 x v ; valeur negative = multiplication (plus
 * rapide) => 96 / |v|. `processClockTick` y declenche un step quand
 * `tick % mod_pulses == 0`, donc `mod_pulses` == ticksPerStep.
 *
 * NB : le pas HISTORIQUE du Trigger Sequencer Sitka est 1/16 ; ici 1/16
 * correspond a SUBDIV = -4 (96/4 = 24 ticks). Ce n'est plus le defaut : le
 * defaut par channel est /1, la noire, comme le channel Sitka d'origine.
 *
 * L'ORDRE de SUBDIVS est normatif : le format de persistance stocke l'INDEX
 * (PRD 11.1), et le miroir C++ (include/flexseq/Subdiv.h) suit exactement le
 * meme ordre, du plus rapide au plus lent.
 */

/** Ticks par noire a 96 PPQN (unite SUBDIV). Egal a SequencerEngine.PPQN. */
export const QUARTER_TICKS = 96;

/** Valeur SUBDIV par defaut : /1 = noire (96 ticks), comme le channel Sitka d'origine. */
export const DEFAULT_SUBDIV = 1;

/**
 * Valeurs SUBDIV officielles (libGravity CLOCK_MOD), ordonnees du plus rapide
 * au plus lent. Positif = division (plus lent), negatif = multiplication.
 */
export const SUBDIVS: readonly number[] = [
  -24, -16, -12, -8, -6, -4, -3, -2,
  1,
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 24, 32, 64, 128,
];

/**
 * Convertit une valeur SUBDIV en ticksPerStep (96 PPQN). Retourne 0 si la
 * valeur est invalide (0, non entiere, ou multiplicateur ne divisant pas 96).
 */
export function subdivToTicks(subdiv: number): number {
  if (!Number.isInteger(subdiv) || subdiv === 0) return 0;
  if (subdiv > 0) return QUARTER_TICKS * subdiv;
  const mult = -subdiv;
  if (QUARTER_TICKS % mult !== 0) return 0; // periode non entiere -> invalide
  return QUARTER_TICKS / mult;
}

/**
 * Libelle SUBDIV facon Gravity original (UI.ino) : positif = division => `/N`,
 * negatif = multiplication => `xN`.
 */
export function subdivLabel(subdiv: number): string {
  return subdiv > 0 ? `/${subdiv}` : `x${-subdiv}`;
}
