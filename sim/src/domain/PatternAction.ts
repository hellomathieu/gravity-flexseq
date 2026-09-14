/**
 * Le miroir de `include/flexseq/PatternAction.h`.
 *
 * PRD 5.0 amendement 1ter : SHIFT plus une rotation charge le template, et le
 * champ d action a quitte l onglet de canal. SAVE et la question ne sont plus
 * des codes — SAVE n a plus d interface, PRD 12.9 le rend au lot E, et la
 * question est un drapeau du controleur.
 *
 * Les codes vivent ici et dans aucun autre fichier de ce cote : le controleur
 * les pose et le service les consomme, et deux definitions du meme code
 * finiraient par diverger.
 */
export enum PatternAction {
  Load = 0,
  // Aucune action demandee. Le controleur POSE une demande, et le service qui
  // connait l EEPROM la consomme.
  None = 0xff,
}
