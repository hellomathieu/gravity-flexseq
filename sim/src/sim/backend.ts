/**
 * Seam backend du Gravity Simulator.
 *
 * L'UI ne parle qu'a cette interface : elle est ainsi decouplee de la source
 * des donnees. Aujourd'hui `TsReferenceBackend` (modele TS) ; demain un
 * `Avr8jsBackend` lira l'etat du firmware C++ execute par avr8js.
 *
 * Modele : une BANQUE de 16 patterns partages + un SequencerEngine qui detient
 * l'etat d'execution par channel (pattern selectionne, longueur, phase locale).
 * Editer le pattern d'un channel modifie le pattern PARTAGE : les autres
 * channels qui le referencent voient le meme contenu.
 */
import { PATTERN_COUNT } from "../domain/PatternBank.js";
import { SequencerEngine, CHANNEL_COUNT, ChannelMode } from "../domain/SequencerEngine.js";
import { TriggerSequencer } from "../domain/TriggerSequencer.js";
import { viewPattern, type CellView } from "./PatternView.js";

export interface SimBackend {
  readonly channelCount: number;
  readonly patternCount: number;

  // --- selection & edition (par channel, sur le pattern partage) ---
  getSelectedPattern(channel: number): number;
  setSelectedPattern(channel: number, index: number): void;
  getLength(channel: number): number;
  setLength(channel: number, length: number): boolean;
  getSubdiv(channel: number): number;
  setSubdiv(channel: number, subdiv: number): boolean;
  /** Separation de mesure (graphique) : une barre tous les N steps. */
  getBarLength(channel: number): number;
  setBarLength(channel: number, steps: number): boolean;
  view(channel: number): CellView[];
  toggleStep(channel: number, index: number): void;
  /** Code de ratchet d'un step du pattern selectionne. */
  getRatchet(channel: number, index: number): number;
  setRatchet(channel: number, index: number, code: number): boolean;

  // --- transport & modele temporel ---
  play(): void;
  pause(): void;
  resetPhase(): void;
  isPlaying(): boolean;
  advanceTicks(ticks: number): void;
  masterPhase(): number;
  effectiveStep(channel: number): number;
  /** Vrai si le channel vient de declencher un trigger. Valide apres advanceTicks(). */
  triggered(channel: number): boolean;
  /** Nombre de declenchements dus pour le dernier advanceTicks() (ratchets inclus). */
  triggerCount(channel: number): number;
}

/** Backend de reference : banque partagee + moteur, en TypeScript. */
export class TsReferenceBackend implements SimBackend {
  readonly channelCount = CHANNEL_COUNT;
  readonly patternCount = PATTERN_COUNT;

  private readonly engine = new SequencerEngine();
  private readonly triggers = new TriggerSequencer(this.engine);

  constructor() {
    // Le simulateur est l'editeur de patterns : ses six channels sont en SEQ.
    // Le defaut du domaine reste CLOCK, comme l'original (PRD 4.2).
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      this.engine.setChannelMode(ch, ChannelMode.SEQ);
    }
  }

  private patternOf(channel: number) {
    return this.engine.patternForChannel(channel);
  }

  // --- selection & edition ----------------------------------------------

  getSelectedPattern(channel: number): number {
    return this.engine.getSelectedPattern(channel);
  }

  setSelectedPattern(channel: number, index: number): void {
    this.engine.setSelectedPattern(channel, index);
  }

  getLength(channel: number): number {
    return this.engine.getEffectiveLength(channel);
  }

  setLength(channel: number, length: number): boolean {
    return this.engine.setBaseLength(channel, length);
  }

  getSubdiv(channel: number): number {
    return this.engine.getSubdiv(channel);
  }

  setSubdiv(channel: number, subdiv: number): boolean {
    return this.engine.setSubdiv(channel, subdiv);
  }

  getBarLength(channel: number): number {
    return this.engine.getBarLength(channel);
  }

  setBarLength(channel: number, steps: number): boolean {
    return this.engine.setBarLength(channel, steps);
  }

  view(channel: number): CellView[] {
    const pattern = this.patternOf(channel);
    return pattern ? viewPattern(pattern, this.engine.getEffectiveLength(channel)) : [];
  }

  toggleStep(channel: number, index: number): void {
    const pattern = this.patternOf(channel);
    if (!pattern) return;
    const current = pattern.readStep(index);
    if (current === null) return;
    pattern.writeStep(index, !current);
  }

  getRatchet(channel: number, index: number): number {
    return this.patternOf(channel)?.getRatchet(index) ?? 0;
  }

  setRatchet(channel: number, index: number, code: number): boolean {
    const ok = this.patternOf(channel)?.setRatchet(index, code) ?? false;
    // Le pattern est PARTAGE : l'edition peut concerner plusieurs channels.
    if (ok) this.engine.refreshTiming();
    return ok;
  }

  // --- transport & modele temporel --------------------------------------

  play(): void {
    this.engine.start();
  }

  pause(): void {
    this.engine.stop();
  }

  resetPhase(): void {
    this.engine.reset();
  }

  isPlaying(): boolean {
    return this.engine.isRunning;
  }

  advanceTicks(ticks: number): void {
    this.engine.advance(ticks);
    this.triggers.update();
  }

  masterPhase(): number {
    return this.engine.masterPhase;
  }

  effectiveStep(channel: number): number {
    return this.engine.effectiveStep(channel);
  }

  triggered(channel: number): boolean {
    return this.triggers.triggered(channel);
  }

  triggerCount(channel: number): number {
    return this.triggers.triggerCount(channel);
  }
}
