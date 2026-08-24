/**
 * FactoryPatterns — les huit patterns d'usine du firmware d'origine
 * (miroir de flexseq/FactoryPatterns.h).
 *
 * L'original livre A1..A8 avec du contenu et B1..B8 vides
 * (`Gravity.ino:83-98`). FlexSeq les livrait tous vides, ce qui vidait de sens
 * la regle qui gele A1..A8 : geler huit emplacements vides ne donne rien.
 *
 * Le contenu tient sur 16 bits par pattern, bit i = step i, donc 16 octets de
 * PROGMEM cote AVR. Les steps 16 et au-dela n'existent pas chez l'original et
 * restent eteints.
 */
import type { PatternBank } from "./PatternBank.js";

export const FACTORY_PATTERN_COUNT = 8;
export const FACTORY_STEP_COUNT = 16;

const FACTORY: readonly number[] = [
  0x9111, 0x0810, 0x1249, 0xcccc, 0xeeee, 0x5454, 0x7fbf, 0xb733,
];

export function loadFactoryPatterns(bank: PatternBank): void {
  for (let index = 0; index < FACTORY_PATTERN_COUNT; ++index) {
    const pattern = bank.getPattern(index);
    if (!pattern) continue;
    pattern.clear();
    pattern.setLowStepMask(FACTORY[index] ?? 0);
  }
}
