/**
 * PatternView — couche de rendu PURE (sans DOM) du domaine vers une grille
 * symbolique, conforme a la geometrie EDIT PATTERN du PRD :
 *
 *   - 24 positions exactement, 2 lignes de 12 ;
 *   - ■ = step actif (dans la longueur active) ;
 *   - □ = step existant mais inactif (dans la longueur active) ;
 *   - • = position au-dela de LENGTH ;
 *   - marqueur graphique au-dessus des groupes ternaires.
 *
 * Ce module est volontairement decouple de tout backend : il ne lit que
 * l'interface publique de `Pattern`. Il est testable sans navigateur.
 *
 * NB : les separateurs de mesure (| au milieu de la grille) dependent de
 * METER + SUBDIV + MEASURES, non encore implementes ; ils sont donc differes.
 */
import { Pattern } from "../domain/Pattern.js";

export type CellKind = "active" | "inactive" | "beyond";

export interface CellView {
  index: number;
  kind: CellKind;
  /** Code de ratchet du step (voir Pattern) : 0 = aucun. */
  ratchet: number;
}

export const CELL_SYMBOL: Record<CellKind, string> = {
  active: "■",
  inactive: "□",
  beyond: "•",
};

export const GRID_STEPS = 36;

export function viewPattern(pattern: Pattern, length: number): CellView[] {
  const cells: CellView[] = [];

  for (let index = 0; index < GRID_STEPS; ++index) {
    const withinLength = index < length;
    const active = pattern.readStep(index) === true;

    const kind: CellKind = !withinLength
      ? "beyond"
      : active
        ? "active"
        : "inactive";

    cells.push({
      index,
      kind,
      ratchet: pattern.getRatchet(index),
    });
  }

  return cells;
}

const ROW_WIDTH = 12;

/**
 * Rendu ASCII deterministe (utile pour les tests et le debug terminal).
 * Deux lignes de 12, chacune precedee d'une ligne de marqueurs de triolet
 * (`‾` au-dessus d'un step appartenant a un groupe, espace sinon).
 */
export function toAscii(cells: CellView[]): string {
  const lines: string[] = [];

  for (let start = 0; start < cells.length; start += ROW_WIDTH) {
    const row = cells.slice(start, start + ROW_WIDTH);
    const marker = row.map((c) => (c.ratchet ? "‾" : " ")).join("");
    const symbols = row.map((c) => CELL_SYMBOL[c.kind]).join("");
    lines.push(marker, symbols);
  }

  return lines.join("\n");
}
