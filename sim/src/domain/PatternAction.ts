/**
 * Le miroir de `include/flexseq/PatternAction.h`.
 *
 * PRD 5.0 amendement 1bis : la grande valeur d un canal en SEQ s ouvre et porte
 * une action. SAVE n existe que si la copie du canal a change, PRD 12.9 point 5.
 *
 * Les codes vivent ici et dans aucun autre fichier de ce cote : le controleur
 * les choisit et l ecran les nomme, et deux definitions du meme code finiraient
 * par diverger.
 */
export enum PatternAction {
  Load = 0,
  Save = 1,
  // La question posee avant une action destructrice — PRD 5.0 amendement 1bis.
  // Elle vit DANS l octet de l action, et non dans un drapeau a part : la RAM
  // est la ressource critique du circuit que ce modele reflete.
  Ask = 2,
  // Aucune action demandee. Le controleur POSE une demande, et le service qui
  // connait l EEPROM la consomme.
  None = 0xff,
}

export const PATTERN_ACTION_COUNT = 2;
