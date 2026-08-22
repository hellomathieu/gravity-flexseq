import { SUBDIVS } from "../../src/domain/subdiv.js";

export function subdivAt(index: number): number {
  return SUBDIVS[index]!;
}
