/**
 * Gravity Simulator — shell web.
 *
 * UI decouplee : ne parle qu'a `SimBackend`. Modele : banque de 16 patterns
 * PARTAGES + etat par channel (pattern selectionne, longueur, phase locale).
 *
 * - Chaque channel choisit son pattern (selectedPattern) et sa longueur.
 * - Editer le pattern d'un channel modifie le pattern PARTAGE : les autres
 *   channels qui le referencent voient le meme contenu.
 * - Play/Stop/Reset + playhead (effectiveStep) par channel ; la bande
 *   "Channels" montre les 6 playheads, fonction de la longueur de chaque channel.
 *
 * L'horloge du simulateur convertit le temps reel en ticks 96 PPQN selon le
 * BPM (role tenu plus tard par la couche Transport).
 */
import { TsReferenceBackend, type SimBackend } from "../sim/backend.js";
import { drawOled, type OledCtx } from "../sim/OledDisplay.js";
import { PPQN } from "../domain/SequencerEngine.js";

const backend: SimBackend = new TsReferenceBackend();

const state = { channel: 0, cursor: 0, tripletMode: false, bpm: 120 };

// horloge d'animation
let rafId = 0;
let lastTime = 0;
let tickAcc = 0;

const patternLabel = (i: number): string => (i < 8 ? "A" : "B") + ((i % 8) + 1);
const curPat = (): number => backend.getSelectedPattern(state.channel);

const app = document.getElementById("app")!;

function render(): void {
  const { channel } = state;
  const pat = curPat();
  const length = backend.getLength(channel);
  const playing = backend.isPlaying();

  const channelButtons = Array.from({ length: backend.channelCount }, (_, c) =>
    `<button data-ch="${c}" class="${c === channel ? "sel" : ""}">CH${c + 1}</button>`,
  ).join("");

  const patternButtons = Array.from({ length: backend.patternCount }, (_, p) =>
    `<button data-pat="${p}" class="${p === pat ? "sel" : ""}">${patternLabel(p)}</button>`,
  ).join("");

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
      <div class="row">
        <span class="label">Transport</span>
        <button id="play" class="${playing ? "sel" : ""}">▶ Play</button>
        <button id="stop">⏸ Stop</button>
        <button id="reset">⭯ Reset</button>
        <span class="label" style="width:auto;margin-left:8px">BPM</span>
        <input id="bpm" type="number" min="20" max="300" value="${state.bpm}" />
        <span id="readout" class="readout"></span>
      </div>
    </div>

    <div class="panel">
      <div class="grid">${gridRows()}</div>
      <p class="hint">
        Clic = <code>${state.tripletMode ? "triolet ⟷ (départ)" : "toggle step"}</code> ·
        pattern <b>partagé</b> (édité ici = vu par tout channel qui le sélectionne) ·
        <span style="color:var(--active)">■</span> actif ·
        <span style="color:var(--inactive)">□</span> inactif ·
        <span style="color:var(--beyond)">•</span> au-delà de LENGTH
      </p>
    </div>

    <div class="panel">
      <div class="label" style="width:auto;margin-bottom:8px">Channels — pattern &amp; step courant (playhead par channel)</div>
      <div id="chstrip" class="chstrip"></div>
    </div>

    <div class="panel">
      <div class="label" style="width:auto;margin-bottom:8px">Écran Gravity (aperçu OLED 128×64) — CH${channel + 1}</div>
      <canvas id="oled" class="oled" width="128" height="64"></canvas>
    </div>
  `;

  drawOledScreen();
  updateReadout();
  updateChannels();
  wire();
}

function gridRows(): string {
  const cells = backend.view(state.channel);
  return [0, 12]
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
}

function drawOledScreen(): void {
  const canvas = app.querySelector<HTMLCanvasElement>("#oled");
  const ctx = canvas?.getContext("2d");
  if (!ctx) return;

  drawOled(ctx as unknown as OledCtx, {
    title: `EDIT PATTERN ${patternLabel(curPat())}`,
    cells: backend.view(state.channel),
    cursor: state.cursor,
    playhead: backend.effectiveStep(state.channel),
  });
}

function updateReadout(): void {
  const el = app.querySelector<HTMLSpanElement>("#readout");
  if (!el) return;
  const step = backend.effectiveStep(state.channel);
  const len = backend.getLength(state.channel);
  el.textContent = `phase ${backend.masterPhase()} · step ${step + 1}/${len}`;
}

/** Bande multi-channels : pattern selectionne, steps et playhead de chaque channel. */
function updateChannels(): void {
  const strip = app.querySelector<HTMLDivElement>("#chstrip");
  if (!strip) return;

  let html = "";
  for (let ch = 0; ch < backend.channelCount; ++ch) {
    const pat = backend.getSelectedPattern(ch);
    const len = backend.getLength(ch);
    const step = backend.effectiveStep(ch);
    const cells = backend.view(ch);

    let row = "";
    for (let i = 0; i < len; ++i) {
      const on = cells[i]?.kind === "active" ? " on" : "";
      const play = i === step ? " play" : "";
      row += `<span class="minicell${on}${play}"></span>`;
    }
    const sel = ch === state.channel ? " sel" : "";
    html += `<div class="chrow${sel}">
      <span class="chlab">CH${ch + 1} ${patternLabel(pat)}</span>
      <span class="minirow">${row}</span>
      <span class="chstep">${step + 1}/${len}</span>
    </div>`;
  }
  strip.innerHTML = html;
}

function animate(now: number): void {
  if (lastTime === 0) lastTime = now;
  const dt = now - lastTime;
  lastTime = now;

  tickAcc += (dt / 1000) * (state.bpm / 60) * PPQN;
  const whole = Math.floor(tickAcc);
  if (whole > 0) {
    tickAcc -= whole;
    backend.advanceTicks(whole);
  }

  drawOledScreen();
  updateReadout();
  updateChannels();
  rafId = requestAnimationFrame(animate);
}

function startLoop(): void {
  if (rafId) return;
  lastTime = 0;
  rafId = requestAnimationFrame(animate);
}

function stopLoop(): void {
  if (rafId) cancelAnimationFrame(rafId);
  rafId = 0;
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
      backend.setSelectedPattern(state.channel, Number(b.dataset.pat));
      render();
    }),
  );

  app.querySelectorAll<HTMLDivElement>(".cell[data-idx]").forEach((c) =>
    c.addEventListener("click", () => {
      const idx = Number(c.dataset.idx);
      state.cursor = idx;
      if (state.tripletMode) {
        backend.toggleTriplet(state.channel, idx);
      } else {
        backend.toggleStep(state.channel, idx);
      }
      render();
    }),
  );

  const len = app.querySelector<HTMLInputElement>("#len");
  len?.addEventListener("change", () => {
    backend.setLength(state.channel, Number(len.value));
    render();
  });

  app.querySelector<HTMLButtonElement>("#triplet")?.addEventListener("click", () => {
    state.tripletMode = !state.tripletMode;
    render();
  });

  const bpm = app.querySelector<HTMLInputElement>("#bpm");
  bpm?.addEventListener("change", () => {
    const v = Number(bpm.value);
    if (Number.isFinite(v) && v >= 20 && v <= 300) state.bpm = v;
  });

  app.querySelector<HTMLButtonElement>("#play")?.addEventListener("click", () => {
    backend.play();
    startLoop();
    render();
  });

  app.querySelector<HTMLButtonElement>("#stop")?.addEventListener("click", () => {
    backend.pause();
    stopLoop();
    render();
  });

  app.querySelector<HTMLButtonElement>("#reset")?.addEventListener("click", () => {
    backend.resetPhase();
    drawOledScreen();
    updateReadout();
    updateChannels();
  });
}

render();
