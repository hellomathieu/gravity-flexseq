/**
 * Gravity Simulator — shell web minimal (P2).
 *
 * UI decouplee : ne parle qu'a `SimBackend`. Ici on branche le backend TS de
 * reference ; le meme rendu pourra afficher un `Avr8jsBackend` plus tard.
 *
 * Perimetre P2 : EDIT PATTERN d'un pattern (24 steps, 2x12, LENGTH, triolets),
 * selection channel/pattern. Transport / masterPhase / sorties trigger =
 * etapes suivantes.
 */
import { TsReferenceBackend, type SimBackend } from "../sim/backend.js";
import { drawOled, type OledCtx } from "../sim/OledDisplay.js";

const backend: SimBackend = new TsReferenceBackend();

const state = { channel: 0, patternIndex: 0, cursor: 0, tripletMode: false };

const patternLabel = (i: number): string =>
  (i < 8 ? "A" : "B") + ((i % 8) + 1);

const app = document.getElementById("app")!;

function render(): void {
  const { channel, patternIndex } = state;
  const cells = backend.view(channel, patternIndex);
  const length = backend.getLength(channel, patternIndex);

  const channelButtons = Array.from({ length: backend.channelCount }, (_, c) =>
    `<button data-ch="${c}" class="${c === channel ? "sel" : ""}">CH${c + 1}</button>`,
  ).join("");

  const patternButtons = Array.from({ length: backend.patternsPerChannel }, (_, p) =>
    `<button data-pat="${p}" class="${p === patternIndex ? "sel" : ""}">${patternLabel(p)}</button>`,
  ).join("");

  const gridRows = [0, 12]
    .map((rowStart) => {
      const rowCells = cells
        .slice(rowStart, rowStart + 12)
        .map((cell) => {
          const trip = cell.tripletStep ? " trip" : "";
          return `
            <div class="cellwrap${trip}">
              <div class="tmark"></div>
              <div class="cell ${cell.kind}" data-idx="${cell.index}">${cell.index + 1}</div>
            </div>`;
        })
        .join("");
      return `<div class="gridrow">${rowCells}</div>`;
    })
    .join("");

  app.innerHTML = `
    <div class="panel">
      <div class="row"><span class="label">Channel</span>${channelButtons}</div>
      <div class="row"><span class="label">Pattern</span>${patternButtons}</div>
      <div class="row">
        <span class="label">LENGTH</span>
        <input id="len" type="number" min="1" max="24" value="${length}" />
        <button id="triplet" class="toggle ${state.tripletMode ? "on" : ""}">
          Mode triolet : ${state.tripletMode ? "ON" : "off"}
        </button>
      </div>
    </div>

    <div class="panel">
      <div class="grid">${gridRows}</div>
      <p class="hint">
        Clic = <code>${state.tripletMode ? "triolet ⟷ (départ)" : "toggle step"}</code> ·
        <span style="color:var(--active)">■</span> actif ·
        <span style="color:var(--inactive)">□</span> inactif ·
        <span style="color:var(--beyond)">•</span> au-delà de LENGTH ·
        barre orange = groupe ternaire
      </p>
    </div>

    <div class="panel">
      <div class="label" style="width:auto;margin-bottom:8px">Écran Gravity (aperçu OLED 128×64)</div>
      <canvas id="oled" class="oled" width="128" height="64"></canvas>
    </div>
  `;

  drawOledScreen();
  wire();
}

function drawOledScreen(): void {
  const canvas = app.querySelector<HTMLCanvasElement>("#oled");
  const ctx = canvas?.getContext("2d");
  if (!ctx) return;

  drawOled(ctx as unknown as OledCtx, {
    title: `EDIT PATTERN ${patternLabel(state.patternIndex)}`,
    cells: backend.view(state.channel, state.patternIndex),
    cursor: state.cursor,
  });
}

function wire(): void {
  app.querySelectorAll<HTMLButtonElement>("button[data-ch]").forEach((b) =>
    b.addEventListener("click", () => {
      state.channel = Number(b.dataset.ch);
      render();
    }),
  );

  app.querySelectorAll<HTMLButtonElement>("button[data-pat]").forEach((b) =>
    b.addEventListener("click", () => {
      state.patternIndex = Number(b.dataset.pat);
      render();
    }),
  );

  app.querySelectorAll<HTMLDivElement>(".cell[data-idx]").forEach((c) =>
    c.addEventListener("click", () => {
      const idx = Number(c.dataset.idx);
      state.cursor = idx;
      if (state.tripletMode) {
        backend.toggleTriplet(state.channel, state.patternIndex, idx);
      } else {
        backend.toggleStep(state.channel, state.patternIndex, idx);
      }
      render();
    }),
  );

  const len = app.querySelector<HTMLInputElement>("#len");
  len?.addEventListener("change", () => {
    backend.setLength(state.channel, state.patternIndex, Number(len.value));
    render();
  });

  app.querySelector<HTMLButtonElement>("#triplet")?.addEventListener("click", () => {
    state.tripletMode = !state.tripletMode;
    render();
  });
}

render();
