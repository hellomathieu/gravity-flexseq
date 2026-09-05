/**
 * mockups — le flux de navigation de l ecran principal et de l editeur, rendu
 * par les VRAIS moteurs de rendu du simulateur.
 *
 * Ce n est pas un dessin approche : chaque vignette sort de `renderMainScreen`
 * ou de `renderPatternScreen`, qui reproduisent le panneau au pixel (786 et 965
 * pixels d encre, rangee par rangee). La geometrie qu ils lisent est gardee par
 * `test/vectors/screen_geometry_vectors.tsv`, ADR 0012.
 *
 * Deux vignettes portent une reference MESUREE sur le panneau. Les autres n en
 * portent pas, et la page le dit plutot que de laisser croire le contraire.
 */
import { renderMainScreen } from "../sim/MainScreenPixels.js";
import {
  GRID_STEPS,
  renderPatternScreen,
  type PatternScreenPixelModel,
} from "../sim/PatternScreenPixels.js";
import { MainParameter, type MainScreenModel } from "../domain/MainScreenModel.js";
import { ChannelMode } from "../domain/SequencerEngine.js";
import {
  RATCHET_2,
  RATCHET_3,
  RATCHET_4,
  RATCHET_6,
  RATCHET_NONE,
  RATCHET_TRIPLET,
} from "../domain/Pattern.js";
import { OLED_H, OLED_W } from "../sim/OledDisplay.js";

interface Shot {
  title: string;
  note: string;
  /** L encre mesuree sur le panneau, quand elle existe. */
  panelInk?: number;
  pixels: Set<string>;
  count: number;
}

function mainModel(over: Partial<MainScreenModel>): MainScreenModel {
  return {
    tab: 2,
    insideTab: false,
    cursor: 0,
    fieldOpen: false,
    fieldCount: 3,
    patternIndex: 0,
    length: 16,
    subdiv: -4,
    barLength: 4,
    mode: ChannelMode.CLOCK,
    offset: 0,
    skipChance: 3,
    stepTicks: 24,
    mainParameter: MainParameter.Subdiv,
    configPage: false,
    tempo: 120,
    clockSource: 0,
    ...over,
  };
}

function editModel(over: Partial<PatternScreenPixelModel> = {}): PatternScreenPixelModel {
  const steps = new Array<boolean>(GRID_STEPS).fill(false);
  for (const i of [0, 2, 5, 6, 7, 8, 13, 15, 16, 19]) steps[i] = true;
  const ratchets = new Array<number>(GRID_STEPS).fill(RATCHET_NONE);
  ratchets[2] = RATCHET_2;
  ratchets[6] = RATCHET_6;
  ratchets[8] = RATCHET_3;
  ratchets[15] = RATCHET_TRIPLET;
  ratchets[16] = RATCHET_4;
  return {
    title: "EDIT PATTERN A1",
    steps,
    ratchets,
    length: 20,
    cursor: 5,
    playhead: 0,
    barLength: 3,
    sepSelected: false,
    sepOpen: false,
    ...over,
  };
}

function fromMain(title: string, note: string, model: MainScreenModel, panelInk?: number): Shot {
  const r = renderMainScreen(model);
  return { title, note, pixels: r.pixels, count: r.count, ...(panelInk ? { panelInk } : {}) };
}

function fromEdit(
  title: string,
  note: string,
  model: PatternScreenPixelModel,
  panelInk?: number,
): Shot {
  const r = renderPatternScreen(model);
  return { title, note, pixels: r.pixels, count: r.count, ...(panelInk ? { panelInk } : {}) };
}

const SHOTS: Shot[] = [
  fromMain(
    "1 — la barre, sur le canal 1",
    "Le canal est en CLOCK, donc la page prend les trois lignes de l original. "
      + "Aucun curseur de champ n est dessine sur la barre.",
    mainModel({}),
  ),
  fromMain(
    "2 — dans l onglet, sur MODE",
    "Un appui court entre dans l onglet. Le curseur se pose sur la ligne 1, et "
      + "l etiquette est inversee.",
    mainModel({ insideTab: true, cursor: 0 }),
  ),
  fromMain(
    "3 — MODE ouvert",
    "Un second appui ouvre le champ. Le pave quitte l etiquette et un cadre "
      + "entoure la valeur : c est elle que la rotation change.",
    mainModel({ insideTab: true, cursor: 0, fieldOpen: true }),
  ),
  fromMain(
    "4 — sur OFFSET",
    "La ligne 2 d un canal en CLOCK. La valeur lit la position dans le pas et le "
      + "nombre d impulsions du pas.",
    mainModel({ insideTab: true, cursor: 1 }),
  ),
  fromMain(
    "5 — sur MOD",
    "La ligne 3. Elle lit OFF, et c est vrai : le routage CV arrive au lot 13. "
      + "C est cet ecran que le panneau a capture.",
    mainModel({ insideTab: true, cursor: 2, subdiv: -4, stepTicks: 24 }),
    965,
  ),
  fromMain(
    "6 — un canal en RANDOM",
    "Le parametre principal devient la probabilite de saut, et la ligne 2 devient "
      + "la subdivision. Meme disposition, autre contenu.",
    mainModel({
      insideTab: true,
      cursor: 0,
      mode: ChannelMode.RANDOM,
      mainParameter: MainParameter.SkipChance,
      skipChance: 3,
    }),
  ),
  fromMain(
    "7 — un canal en SEQ",
    "SEQ garde la disposition FlexSeq : le lot 12 la convertit. Cinq champs plus "
      + "MODE, donc six, et l entree EDIT PATTERN.",
    mainModel({
      insideTab: true,
      cursor: 1,
      mode: ChannelMode.SEQ,
      mainParameter: MainParameter.Pattern,
      fieldCount: 6,
    }),
  ),
  fromMain(
    "8 — l onglet de l horloge",
    "Le tempo en grand, la source en dessous. Le glyphe de l onglet est celui de "
      + "l original.",
    mainModel({ tab: 0, insideTab: true, cursor: 1 }),
  ),
  fromMain(
    "9 — l onglet des reglages",
    "Il ne porte aucun champ pour l instant, donc un appui n y fait rien plutot "
      + "que d ouvrir un niveau vide.",
    mainModel({ tab: 7, insideTab: false }),
  ),
  fromMain(
    "10 — CONFIG PATTERN",
    "La quatrieme page, ouverte par la ligne 3 d un canal en SEQ. Meme forme que "
      + "l onglet : le nom du pattern en gros avec son etiquette, trois lignes a "
      + "droite. Elle porte LENGTH et SUBDIV, qui quittent l onglet, et MOD. "
      + "⚠️ Le dessin existe ; la navigation qui y mene est le lot 12.",
    mainModel({
      tab: 2,
      insideTab: true,
      cursor: 0,
      configPage: true,
      mode: ChannelMode.SEQ,
      mainParameter: MainParameter.Pattern,
      patternIndex: 9,
      length: 20,
      subdiv: -4,
      fieldCount: 3,
    }),
  ),
  fromEdit(
    "11 — EDIT PATTERN",
    "Trois rangees de douze. Disque plein pour un step actif, anneau pour un step "
      + "vide, triangle pour un triolet, chiffre sous le step pour un ratchet, "
      + "simple point au-dela de la longueur. Les barres verticales sont les "
      + "separations de mesure. C est cet ecran que le panneau a capture.",
    editModel(),
    786,
  ),
  fromEdit(
    "12 — EDIT PATTERN, sans separation",
    "La meme grille, la separation de mesure a zero.",
    editModel({ barLength: 0, cursor: 15 }),
  ),
];

const ZOOMS = [1, 2, 3, 4];
let zoom = 2;

function draw(canvas: HTMLCanvasElement, shot: Shot): void {
  canvas.width = OLED_W * zoom;
  canvas.height = OLED_H * zoom;
  canvas.style.width = `${OLED_W * zoom}px`;
  canvas.style.height = `${OLED_H * zoom}px`;
  const ctx = canvas.getContext("2d");
  if (!ctx) return;
  ctx.fillStyle = "#05070a";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = "#8ce6b0";
  for (const key of shot.pixels) {
    const [xs, ys] = key.split(",");
    ctx.fillRect(Number(xs) * zoom, Number(ys) * zoom, zoom, zoom);
  }
}

function verdict(shot: Shot): string {
  if (shot.panelInk === undefined) {
    return `${shot.count} px — aucune mesure de panneau pour cet ecran`;
  }
  return shot.count === shot.panelInk
    ? `${shot.count} px — IDENTIQUE au panneau (${shot.panelInk} px mesures)`
    : `${shot.count} px — DIFFERE du panneau (${shot.panelInk} px mesures)`;
}

function mount(): void {
  const root = document.getElementById("shots");
  if (!root) return;

  const canvases: HTMLCanvasElement[] = [];
  for (const shot of SHOTS) {
    const card = document.createElement("section");
    card.className = "card";

    const h = document.createElement("h2");
    h.textContent = shot.title;
    card.appendChild(h);

    const canvas = document.createElement("canvas");
    canvas.className = "oled";
    canvases.push(canvas);
    card.appendChild(canvas);

    const ink = document.createElement("p");
    ink.className = shot.panelInk === undefined
      ? "ink"
      : shot.count === shot.panelInk ? "ink ok" : "ink bad";
    ink.textContent = verdict(shot);
    card.appendChild(ink);

    const note = document.createElement("p");
    note.className = "note";
    note.textContent = shot.note;
    card.appendChild(note);

    root.appendChild(card);
  }

  const redraw = () => SHOTS.forEach((s, i) => draw(canvases[i]!, s));

  const bar = document.getElementById("zoom");
  if (bar) {
    for (const z of ZOOMS) {
      const b = document.createElement("button");
      b.textContent = `x${z}`;
      b.className = z === zoom ? "on" : "";
      b.addEventListener("click", () => {
        zoom = z;
        for (const other of bar.querySelectorAll("button")) other.className = "";
        b.className = "on";
        redraw();
      });
      bar.appendChild(b);
    }
  }

  redraw();
}

mount();
