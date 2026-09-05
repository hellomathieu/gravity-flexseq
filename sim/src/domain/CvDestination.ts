/**
 * PRD 10.2. L'ORDRE est normatif : le code part en EEPROM, octets 7 et 8 du
 * record de channel. Le reordonner change le routage de toute image deja
 * ecrite. NONE vaut 0 pour qu'une image anterieure, qui porte 0 dans ces deux
 * octets, se relise sans migration.
 */
export enum CvDestination {
  NONE = 0,
  PATTERN = 1,
  LENGTH = 2,
  RESET = 3,
  STEP = 4,
}

export const CV_DESTINATION_COUNT = 5;
export const DEFAULT_CV_DESTINATION = CvDestination.NONE;

export const CV_SOURCE_COUNT = 2;
export const CV_SOURCE_1 = 0;
export const CV_SOURCE_2 = 1;

/**
 * PRD 10.2 : le champ MOD d'un canal en SEQ porte vingt et une valeurs. La
 * position nomme l'entree, CV1 avant CV2. Le cycle ne propose PAS deux entrees
 * sur la meme destination ; le moteur garde cette capacite et le nommage
 * l'accepte, seule la rotation ne peut pas la produire.
 */
export const MOD_CHOICE_COUNT = 21;
export const MOD_ROUTED_COUNT = CV_DESTINATION_COUNT - 1;

export function modChoiceAt(index: number): [CvDestination, CvDestination] {
  if (index <= 0 || index >= MOD_CHOICE_COUNT) {
    return [CvDestination.NONE, CvDestination.NONE];
  }
  if (index <= MOD_ROUTED_COUNT) {
    return [index as CvDestination, CvDestination.NONE];
  }
  if (index <= 2 * MOD_ROUTED_COUNT) {
    return [CvDestination.NONE, (index - MOD_ROUTED_COUNT) as CvDestination];
  }
  const rank = index - 1 - 2 * MOD_ROUTED_COUNT;
  const a = Math.floor(rank / (MOD_ROUTED_COUNT - 1)) + 1;
  const b = (rank % (MOD_ROUTED_COUNT - 1)) + 1;
  return [a as CvDestination, (b >= a ? b + 1 : b) as CvDestination];
}

export function modIndexOf(first: CvDestination, second: CvDestination): number {
  for (let index = 0; index < MOD_CHOICE_COUNT; ++index) {
    const [a, b] = modChoiceAt(index);
    if (a === first && b === second) return index;
  }
  return -1;
}
