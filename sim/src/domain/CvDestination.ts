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
