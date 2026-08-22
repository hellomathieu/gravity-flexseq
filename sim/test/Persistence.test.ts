import { describe, expect, it } from "vitest";
import {
  BASE_ADDRESS,
  CHANNELS_OFFSET,
  CHANNEL_RECORD,
  EEPROM_SIZE,
  FORMAT_VERSION,
  GLOBAL_OFFSET,
  PATTERNS_OFFSET,
  PATTERN_STEP_BYTES,
  PersistenceScheduler,
  PersistentImage,
  PREFS_OFFSET,
  QUIET_MS,
  TOTAL_SIZE,
  defaultPreferences,
  type Preferences,
  type Storage,
} from "../src/domain/Persistence.js";
import { PatternBank } from "../src/domain/PatternBank.js";
import { CHANNEL_COUNT, DEFAULT_LENGTH, SequencerEngine } from "../src/domain/SequencerEngine.js";
import { Transport } from "../src/domain/Transport.js";
import { DEFAULT_TEMPO, UiController } from "../src/domain/UiController.js";
import { RATCHET_3, RATCHET_6, RATCHET_NONE, RATCHET_TRIPLET } from "../src/domain/Pattern.js";
import { subdivAt } from "./helpers/subdivAt.js";

const SENTINEL = 0x5a;

class FakeEeprom implements Storage {
  cell = new Uint8Array(EEPROM_SIZE).fill(SENTINEL);
  writes = 0;
  lowestWrite = EEPROM_SIZE;
  highestWrite = -1;

  read(address: number): number {
    return this.cell[address]!;
  }

  write(address: number, value: number): void {
    this.cell[address] = value;
    ++this.writes;
    if (address < this.lowestWrite) this.lowestWrite = address;
    if (address > this.highestWrite) this.highestWrite = address;
  }
}

function rig() {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  engine.setPatternBank(bank);
  const transport = new Transport(engine);
  const ui = new UiController(engine, bank, transport);
  const prefs: Preferences = defaultPreferences();
  const image = new PersistentImage(bank, engine, ui, prefs);
  const scheduler = new PersistenceScheduler();
  return { bank, engine, ui, prefs, image, scheduler };
}

type Rig = ReturnType<typeof rig>;

function finishWrite(r: Rig, eeprom: FakeEeprom, nowMs: number): number {
  let calls = 0;
  while (r.scheduler.advance(eeprom, r.image, nowMs)) ++calls;
  return calls;
}

function fillDistinctState(r: Rig): void {
  for (let index = 0; index < 16; ++index) {
    const p = r.bank.getPattern(index)!;
    p.writeStep(index % 24, true);
    p.writeStep((index * 7 + 3) % 24, true);
    p.setRatchet(index % 24, RATCHET_3);
    p.setRatchet((index * 5 + 1) % 24, RATCHET_TRIPLET);
  }
  for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
    r.engine.setSelectedPattern(ch, 15 - ch);
    r.engine.setEffectiveLength(ch, 24 - ch * 3);
    r.engine.setSubdiv(ch, subdivAt(ch * 3));
    r.engine.setBarLength(ch, ch % 2 === 0 ? 3 : 6);
  }
  r.ui.setTempo(287);
  r.ui.setClockSource(4);
  r.prefs.rotateScreen = 0;
  r.prefs.reverseEncoder = 0;
  r.prefs.cvCalibration[0] = -26;
  r.prefs.cvCalibration[1] = 300;
}

function sameState(a: Rig, b: Rig): boolean {
  for (let index = 0; index < TOTAL_SIZE; ++index) {
    if (a.image.byteAt(index) !== b.image.byteAt(index)) return false;
  }
  return true;
}

describe("Persistence — the layout fixed by PRD 11.1", () => {
  it("places the image at 384 and keeps it 286 bytes", () => {
    expect(BASE_ADDRESS).toBe(384);
    expect(TOTAL_SIZE).toBe(286);
    expect(PATTERNS_OFFSET).toBe(1);
    expect(CHANNELS_OFFSET).toBe(241);
    expect(GLOBAL_OFFSET).toBe(277);
    expect(PREFS_OFFSET).toBe(280);
  });

  it("ends below the original firmware's memCode at 1023", () => {
    expect(BASE_ADDRESS + TOTAL_SIZE).toBeLessThanOrEqual(1023);
  });
});

describe("Persistence — round trip", () => {
  it("restores the state byte for byte", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(sameState(saved, loaded)).toBe(true);
    expect(loaded.ui.tempo).toBe(287);
    expect(loaded.ui.clockSource).toBe(4);
    expect(loaded.prefs.cvCalibration[0]).toBe(-26);
    expect(loaded.prefs.cvCalibration[1]).toBe(300);
  });

  it("keeps the patterns with their ratchets", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    const p = saved.bank.getPattern(5)!;
    p.writeStep(0, true);
    p.writeStep(23, true);
    p.setRatchet(0, RATCHET_6);
    p.setRatchet(23, RATCHET_TRIPLET);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    loaded.scheduler.load(eeprom, loaded.image);
    const q = loaded.bank.getPattern(5)!;
    expect(q.readStep(0)).toBe(true);
    expect(q.readStep(23)).toBe(true);
    expect(q.readStep(11)).toBe(false);
    expect(q.getRatchet(0)).toBe(RATCHET_6);
    expect(q.getRatchet(23)).toBe(RATCHET_TRIPLET);
  });
});

describe("Persistence — the version byte", () => {
  it("returns the defaults when the version does not match", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS] = FORMAT_VERSION + 1;

    const loaded = rig();
    fillDistinctState(loaded);
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(false);
    expect(sameState(rig(), loaded)).toBe(true);
    expect(loaded.ui.tempo).toBe(DEFAULT_TEMPO);
  });

  it("returns the defaults on a blank EEPROM", () => {
    const eeprom = new FakeEeprom();
    const loaded = rig();
    fillDistinctState(loaded);
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(false);
    expect(sameState(rig(), loaded)).toBe(true);
  });
});

describe("Persistence — never outside its own zone", () => {
  it("touches no byte below 384 nor above 669", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, QUIET_MS);

    expect(eeprom.lowestWrite).toBe(BASE_ADDRESS);
    expect(eeprom.highestWrite).toBe(BASE_ADDRESS + TOTAL_SIZE - 1);
    for (let address = 0; address < BASE_ADDRESS; ++address) {
      expect(eeprom.cell[address]).toBe(SENTINEL);
    }
    for (let address = BASE_ADDRESS + TOTAL_SIZE; address < EEPROM_SIZE; ++address) {
      expect(eeprom.cell[address]).toBe(SENTINEL);
    }
  });
});

describe("Persistence — the quiet delay and the spread write", () => {
  it("writes nothing before the delay has passed", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    r.scheduler.markDirty(1000);
    expect(r.scheduler.advance(eeprom, r.image, 1000)).toBe(false);
    expect(r.scheduler.advance(eeprom, r.image, 1000 + QUIET_MS - 1)).toBe(false);
    expect(eeprom.writes).toBe(0);
    expect(r.scheduler.advance(eeprom, r.image, 1000 + QUIET_MS)).toBe(true);
    expect(eeprom.writes).toBe(1);
  });

  it("restarts the countdown on a change during the delay", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    r.scheduler.markDirty(0);
    r.scheduler.advance(eeprom, r.image, QUIET_MS - 1);
    r.scheduler.markDirty(QUIET_MS - 1);
    r.scheduler.advance(eeprom, r.image, QUIET_MS);
    expect(eeprom.writes).toBe(0);
  });

  it("writes at most one byte per call", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    let previous = 0;
    while (r.scheduler.advance(eeprom, r.image, QUIET_MS)) {
      expect(eeprom.writes).toBe(previous + 1);
      previous = eeprom.writes;
    }
  });

  it("costs no write when nothing changed", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, QUIET_MS);
    const first = eeprom.writes;
    expect(first).toBeGreaterThan(0);
    r.scheduler.markDirty(QUIET_MS);
    finishWrite(r, eeprom, 2 * QUIET_MS);
    expect(eeprom.writes).toBe(first);
  });

  it("costs one byte for one edited step", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, QUIET_MS);
    const baseline = eeprom.writes;
    r.bank.getPattern(0)!.writeStep(3, true);
    r.scheduler.markDirty(QUIET_MS);
    finishWrite(r, eeprom, 2 * QUIET_MS);
    expect(eeprom.writes - baseline).toBe(1);
  });

  it("is clean once the image is written", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    fillDistinctState(r);
    r.scheduler.markDirty(0);
    expect(r.scheduler.isDirty).toBe(true);
    finishWrite(r, eeprom, QUIET_MS);
    expect(r.scheduler.isDirty).toBe(false);
    expect(r.scheduler.isWriting).toBe(false);
  });
});

describe("Persistence — what a corrupted image may not do", () => {
  it("replaces a previous ratchet when the stored nibble is invalid", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS + PATTERNS_OFFSET + PATTERN_STEP_BYTES] = 0x51;

    const loaded = rig();
    loaded.bank.getPattern(0)!.setRatchet(0, RATCHET_3);
    loaded.bank.getPattern(0)!.setRatchet(1, RATCHET_6);
    loaded.scheduler.load(eeprom, loaded.image);
    expect(loaded.bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_NONE);
    expect(loaded.bank.getPattern(0)!.getRatchet(1)).toBe(RATCHET_NONE);
  });

  it("refuses an out-of-range stored value instead of applying it", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    r.scheduler.markDirty(0);
    finishWrite(r, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS + CHANNELS_OFFSET + 1] = 99;
    eeprom.cell[BASE_ADDRESS + GLOBAL_OFFSET + 2] = 99;

    const loaded = rig();
    loaded.scheduler.load(eeprom, loaded.image);
    expect(loaded.engine.getEffectiveLength(0)).toBe(DEFAULT_LENGTH);
    expect(loaded.ui.clockSource).toBeLessThan(6);
  });

  it("stores SUBDIV as an index and restores its value", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    saved.engine.setSubdiv(0, -24);
    saved.engine.setSubdiv(1, 128);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    loaded.scheduler.load(eeprom, loaded.image);
    expect(loaded.engine.getSubdiv(0)).toBe(-24);
    expect(loaded.engine.getSubdiv(1)).toBe(128);
  });

  it("keeps one channel record per channel", () => {
    expect(CHANNEL_RECORD * CHANNEL_COUNT).toBe(36);
  });
});
