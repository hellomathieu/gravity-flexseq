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
import { SUBDIVS, subdivLabel } from "../domain/subdiv.js";
import { BAR_LENGTHS } from "../domain/SequencerEngine.js";
import { RATCHET_CODES } from "../domain/Pattern.js";

const backend: SimBackend = new TsReferenceBackend();

const state = {
  channel: 0,
  cursor: 0,
  ratchet: 0, // code applique au clic quand ratchetMode est actif
  ratchetMode: false,
  bpm: 120,
};

// Trigger flash: timestamp (perf clock) until which each channel's LED stays lit.
const FLASH_MS = 120;
const triggerFlashUntil: number[] = new Array(6).fill(0);

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
  const sub = backend.getSubdiv(channel);
  const playing = backend.isPlaying();

  const subdivOptions = SUBDIVS.map(
    (v) => `<option value="${v}" ${v === sub ? "selected" : ""}>${subdivLabel(v)}</option>`,
  ).join("");

  const barLen = backend.getBarLength(channel);
  const barLabel = (n: number): string => (n === 0 ? "aucune" : `${n}/4`);
  const barOptions = BAR_LENGTHS.map(
    (n) => `<option value="${n}" ${n === barLen ? "selected" : ""}>${barLabel(n)}</option>`,
  ).join("");
  const ratchetOptions = RATCHET_CODES.map(
    (c) =>
      `<option value="${c}" ${c === state.ratchet ? "selected" : ""}>${
        c === 0 ? "aucun" : c === 7 ? "triolet ▲" : `x${c}`
      }</option>`,
  ).join("");

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
        <span class="label">MESURE</span>
        <select id="meter">${barOptions}</select>
        <span class="label" style="width:auto;margin-left:8px">SUBDIV</span>
        <select id="subdiv">${subdivOptions}</select>
        <span class="label" style="width:auto;margin-left:8px">LENGTH</span>
        <input id="len" type="number" min="1" max="24" value="${length}" />
        <span class="label" style="width:auto;margin-left:8px">RATCHET</span>
        <select id="ratchet">${ratchetOptions}</select>
        <button id="ratchetmode" class="toggle ${state.ratchetMode ? "on" : ""}">
          Appliquer au clic : ${state.ratchetMode ? "ON" : "off"}
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
        Clic = <code>${state.ratchetMode ? "applique le ratchet" : "toggle step"}</code> ·
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
      <div class="row" style="margin-bottom:8px">
        <span class="label" style="width:auto">Écran Gravity (aperçu OLED 128×64) — CH${channel + 1}</span>
      </div>
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
          const trip = cell.ratchet ? " trip" : "";
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

  const label = patternLabel(curPat());

  drawOled(ctx as unknown as OledCtx, {
    title: `EDIT PATTERN ${label}`,
    cells: backend.view(state.channel),
    cursor: state.cursor,
    playhead: backend.effectiveStep(state.channel),
    barLength: backend.getBarLength(state.channel),
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
    const lit = performance.now() < (triggerFlashUntil[ch] ?? 0) ? " on" : "";
    html += `<div class="chrow${sel}">
      <span class="trigled${lit}" title="trigger"></span>
      <span class="chlab">CH${ch + 1} ${patternLabel(pat)} · ${subdivLabel(backend.getSubdiv(ch))}</span>
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
    // Right after advancing: latch a flash for every channel that just triggered.
    for (let ch = 0; ch < backend.channelCount; ++ch) {
      if (backend.triggered(ch)) triggerFlashUntil[ch] = now + FLASH_MS;
    }
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
      if (state.ratchetMode) {
        backend.setRatchet(state.channel, idx, state.ratchet);
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

  const subdiv = app.querySelector<HTMLSelectElement>("#subdiv");
  subdiv?.addEventListener("change", () => {
    backend.setSubdiv(state.channel, Number(subdiv.value));
    render();
  });

  const meter = app.querySelector<HTMLSelectElement>("#meter");
  meter?.addEventListener("change", () => {
    backend.setBarLength(state.channel, Number(meter.value));
    render();
  });

  const ratchet = app.querySelector<HTMLSelectElement>("#ratchet");
  ratchet?.addEventListener("change", () => {
    state.ratchet = Number(ratchet.value);
    render();
  });


  app.querySelector<HTMLButtonElement>("#ratchetmode")?.addEventListener("click", () => {
    state.ratchetMode = !state.ratchetMode;
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
