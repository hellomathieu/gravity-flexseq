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
  PersistentImageV4,
  bootstrap,
  PREFS_OFFSET,
  QUIET_MS,
  TOTAL_SIZE,
  v4,
  defaultPreferences,
  type Preferences,
  type Storage,
} from "../src/domain/Persistence.js";
import { PatternBank } from "../src/domain/PatternBank.js";
import { CvDestination, CV_SOURCE_1, CV_SOURCE_2 } from "../src/domain/CvDestination.js";
import {
  CHANNEL_COUNT,
  ChannelMode,
  DEFAULT_CHANNEL_MODE,
  DEFAULT_LENGTH,
  SequencerEngine,
} from "../src/domain/SequencerEngine.js";
import { Transport } from "../src/domain/Transport.js";
import { DEFAULT_TEMPO, UiController } from "../src/domain/UiController.js";
import {
  Pattern,
  RATCHET_3,
  RATCHET_4,
  RATCHET_6,
  RATCHET_NONE,
  RATCHET_TRIPLET,
} from "../src/domain/Pattern.js";
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

class JournalEeprom implements Storage {
  cell = new Uint8Array(EEPROM_SIZE).fill(SENTINEL);
  order: number[] = [];

  read(address: number): number {
    return this.cell[address]!;
  }

  write(address: number, value: number): void {
    this.cell[address] = value;
    this.order.push(address);
  }
}

function rigV4() {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  engine.setPatternBank(bank);
  const transport = new Transport(engine);
  const ui = new UiController(engine, transport);
  const prefs: Preferences = defaultPreferences();
  const image = new PersistentImageV4(engine, ui, prefs);
  const scheduler = new PersistenceScheduler();
  return { bank, engine, transport, ui, prefs, image, scheduler };
}

type RigV4 = ReturnType<typeof rigV4>;

function finishWriteV4(r: RigV4, eeprom: FakeEeprom, nowMs: number): number {
  let calls = 0;
  while (r.scheduler.advance(eeprom, r.image, nowMs)) ++calls;
  return calls;
}

function bankStepMask(pattern: Pattern): bigint {
  let mask = 0n;
  for (let step = 0; step < 36; ++step) {
    if (pattern.readStep(step) === true) mask |= 1n << BigInt(step);
  }
  return mask;
}

function fillDistinctStateV4(r: RigV4): void {
  for (let index = 0; index < 16; ++index) {
    const p = r.bank.getPattern(index)!;
    p.writeStep(index % 36, true);
    p.writeStep((index * 7 + 3) % 36, true);
    p.writeStep(35, index % 2 === 0);
    p.setRatchet(index % 36, RATCHET_3);
    p.setRatchet((index * 5 + 1) % 36, RATCHET_TRIPLET);
  }
  for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
    r.engine.setSelectedPattern(ch, 15 - ch);
    r.engine.setBaseLength(ch, 36 - ch * 3);
    r.engine.setSubdiv(ch, subdivAt(ch * 3));
    r.engine.setBarLength(ch, ch % 2 === 0 ? 3 : 6);
    r.engine.setChannelMode(ch, (ch % 3) as ChannelMode);
    r.engine.setOffset(ch, ch * 2 + 1);
    r.engine.setSkipChance(ch, ch + 2);
    r.engine.setCvDestination(ch, CV_SOURCE_1, (ch + 1) % 5);
    r.engine.setCvDestination(ch, CV_SOURCE_2, (ch + 3) % 5);
  }
  r.ui.setTempo(287);
  r.ui.setClockSource(4);
  r.prefs.rotateScreen = 0;
  r.prefs.reverseEncoder = 0;
  r.prefs.cvCalibration[0] = -26;
  r.prefs.cvCalibration[1] = 300;
}

function sameStateV4(a: RigV4, b: RigV4): boolean {
  for (let index = 0; index < a.image.size; ++index) {
    if (a.image.byteAt(index) !== b.image.byteAt(index)) return false;
  }
  return true;
}

function scanEverything(r: RigV4): JournalEeprom {
  const ee = new JournalEeprom();
  for (let index = 0; index < r.image.size; ++index) {
    ee.cell[r.image.addressAt(index)] = r.image.byteAt(index) ^ 0xff;
  }
  ee.order = [];
  r.scheduler.markDirty(0);
  while (r.scheduler.advance(ee, r.image, QUIET_MS)) {
    // draine le parcours complet
  }
  return ee;
}

function rig() {
  const bank = new PatternBank();
  const engine = new SequencerEngine();
  const transport = new Transport(engine);
  const ui = new UiController(engine, transport);
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
    r.engine.setBaseLength(ch, 24 - ch * 3);
    r.engine.setSubdiv(ch, subdivAt(ch * 3));
    r.engine.setBarLength(ch, ch % 2 === 0 ? 3 : 6);
  }
  for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
    r.engine.setChannelMode(ch, (ch % 3) as ChannelMode);
    r.engine.setOffset(ch, ch * 2 + 1);
    r.engine.setSkipChance(ch, ch + 2);
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
  it("places the image at 384 and keeps it 304 bytes", () => {
    expect(BASE_ADDRESS).toBe(384);
    expect(TOTAL_SIZE).toBe(304);
    expect(PATTERNS_OFFSET).toBe(1);
    expect(CHANNELS_OFFSET).toBe(241);
    expect(GLOBAL_OFFSET).toBe(295);
    expect(PREFS_OFFSET).toBe(298);
  });

  it("is version 2", () => {
    expect(FORMAT_VERSION).toBe(2);
  });

  it("ends below the original firmware's memCode at 1023", () => {
    expect(BASE_ADDRESS + TOTAL_SIZE).toBeLessThanOrEqual(1023);
  });
});


function fillDistinctPattern(pattern: Pattern): void {
  for (const step of [0, 7, 8, 23, 31, 32, 35]) pattern.writeStep(step, true);
  pattern.setRatchet(0, RATCHET_3);
  pattern.setRatchet(1, RATCHET_6);
  pattern.setRatchet(34, RATCHET_TRIPLET);
  pattern.setRatchet(35, RATCHET_4);
}

describe("Persistence — the version 4 content codec", () => {
  it("round trips the twenty-three bytes", () => {
    const source = new Pattern();
    fillDistinctPattern(source);

    const image = Array.from({ length: 23 }, (_, offset) => v4.contentByte(source, offset));

    const loaded = new Pattern();
    for (let offset = 0; offset < 23; ++offset) v4.applyContentByte(loaded, offset, image[offset] ?? 0);

    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      expect(loaded.readStep(step)).toBe(source.readStep(step));
      expect(loaded.getRatchet(step)).toBe(source.getRatchet(step));
    }
    for (let offset = 0; offset < 23; ++offset) {
      expect(v4.contentByte(loaded, offset)).toBe(image[offset]);
    }
  });

  it("drops the four bits above the last step on load", () => {
    const pattern = new Pattern();
    v4.applyContentByte(pattern, 4, 0xff);

    expect(v4.contentByte(pattern, 4)).toBe(0x0f);
    for (let step = 32; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      expect(pattern.readStep(step)).toBe(true);
    }
  });

  it("has no representation at all for the four bits above the last step", () => {
    const pattern = new Pattern();
    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) pattern.writeStep(step, true);

    expect(v4.contentByte(pattern, 4)).toBe(0x0f);
    expect(pattern.writeStep(36, true)).toBe(false);
    expect(pattern.readStep(36)).toBe(null);
    expect(v4.contentByte(pattern, 4)).toBe(0x0f);
  });

  it("leaves the other step bytes whole", () => {
    const pattern = new Pattern();
    for (let offset = 0; offset < 4; ++offset) {
      v4.applyContentByte(pattern, offset, 0xff);
      expect(v4.contentByte(pattern, offset)).toBe(0xff);
    }
  });

  it("normalises an invalid ratchet nibble", () => {
    const pattern = new Pattern();
    v4.applyContentByte(pattern, 5, 0x53);

    expect(pattern.getRatchet(0)).toBe(RATCHET_3);
    expect(pattern.getRatchet(1)).toBe(RATCHET_NONE);
    expect(v4.contentByte(pattern, 5)).toBe(0x03);
  });

  it("replaces a previous ratchet when the stored nibble is invalid", () => {
    const pattern = new Pattern();
    v4.applyContentByte(pattern, 5, 0x36);
    expect(pattern.getRatchet(0)).toBe(RATCHET_6);
    expect(pattern.getRatchet(1)).toBe(RATCHET_3);

    v4.applyContentByte(pattern, 5, 0x51);
    expect(pattern.getRatchet(0)).toBe(RATCHET_NONE);
    expect(pattern.getRatchet(1)).toBe(RATCHET_NONE);
    expect(v4.contentByte(pattern, 5)).toBe(0);
  });

  it("ignores an offset past the record", () => {
    const pattern = new Pattern();
    for (let offset = 0; offset < 23; ++offset) v4.applyContentByte(pattern, offset, (0x11 * offset) & 0xff);

    const before = Array.from({ length: 23 }, (_, offset) => v4.contentByte(pattern, offset));

    v4.applyContentByte(pattern, 23, 0xff);
    v4.applyContentByte(pattern, 200, 0xff);

    for (let offset = 0; offset < 23; ++offset) {
      expect(v4.contentByte(pattern, offset)).toBe(before[offset]);
    }
    expect(v4.contentByte(pattern, 23)).toBe(0);
    expect(v4.contentByte(pattern, 200)).toBe(0);
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

  it("restaure les destinations CV des deux sources", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    expect(saved.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH)).toBe(true);
    expect(saved.engine.setCvDestination(0, CV_SOURCE_2, CvDestination.STEP)).toBe(true);
    expect(saved.engine.setCvDestination(3, CV_SOURCE_1, CvDestination.RESET)).toBe(true);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(loaded.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.LENGTH);
    expect(loaded.engine.getCvDestination(0, CV_SOURCE_2)).toBe(CvDestination.STEP);
    expect(loaded.engine.getCvDestination(3, CV_SOURCE_1)).toBe(CvDestination.RESET);
    expect(loaded.engine.getCvDestination(3, CV_SOURCE_2)).toBe(CvDestination.NONE);
  });

  it("ne restaure jamais une zone d'hysteresis", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    expect(saved.engine.setChannelMode(0, ChannelMode.SEQ)).toBe(true);
    expect(saved.engine.setBaseLength(0, 18)).toBe(true);
    expect(saved.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH)).toBe(true);
    saved.engine.setCvInput(CV_SOURCE_1, 330);
    saved.engine.start();
    saved.engine.advance(96);
    expect(saved.engine.lengthCvOffset(0)).toBe(10);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(loaded.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.LENGTH);
    expect(loaded.engine.lengthCvOffset(0)).toBe(0);
    expect(loaded.engine.getEffectiveLength(0)).toBe(18);
  });

  it("keeps a base length of thirty six across a round trip (ADR 0009)", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    expect(saved.engine.setBaseLengthFromStorage(0, 36)).toBe(true);
    expect(saved.engine.getBaseLength(0)).toBe(36);
    expect(saved.engine.getEffectiveLength(0)).toBe(36);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(loaded.engine.getBaseLength(0)).toBe(36);
    expect(loaded.engine.getEffectiveLength(0)).toBe(36);
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
    expect(CHANNEL_RECORD * CHANNEL_COUNT).toBe(54);
  });
});

describe("Persistence — format v2, nine bytes per channel", () => {
  it("carries the mode, the offset and the skip chance", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    saved.engine.setChannelMode(2, ChannelMode.SEQ);
    saved.engine.setOffset(2, 7);
    saved.engine.setSkipChance(2, 9);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(loaded.engine.getChannelMode(2)).toBe(ChannelMode.SEQ);
    expect(loaded.engine.getOffset(2)).toBe(7);
    expect(loaded.engine.getSkipChance(2)).toBe(9);
  });

  it("puts the three new fields at their fixed place in the record", () => {
    const r = rig();
    r.engine.setChannelMode(0, ChannelMode.RANDOM);
    r.engine.setOffset(0, 13);
    r.engine.setSkipChance(0, 4);
    expect(r.image.byteAt(CHANNELS_OFFSET + 4)).toBe(ChannelMode.RANDOM);
    expect(r.image.byteAt(CHANNELS_OFFSET + 5)).toBe(13);
    expect(r.image.byteAt(CHANNELS_OFFSET + 6)).toBe(4);
  });

  it("refuses a version 1 image and returns the defaults", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS] = 1;

    const loaded = rig();
    fillDistinctState(loaded);
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(false);
    expect(sameState(rig(), loaded)).toBe(true);
  });

  it("refuses a bad mode byte while the next record still loads", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    saved.engine.setChannelMode(1, ChannelMode.SEQ);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS + CHANNELS_OFFSET + 4] = 3;

    const loaded = rig();
    loaded.scheduler.load(eeprom, loaded.image);
    expect(loaded.engine.getChannelMode(1)).toBe(ChannelMode.SEQ);
    expect(loaded.engine.getChannelMode(0)).toBe(DEFAULT_CHANNEL_MODE);
  });

  it("refuses a bad skip chance byte while the next record still loads", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    saved.engine.setSkipChance(1, 7);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS + CHANNELS_OFFSET + 6] = 99;

    const loaded = rig();
    loaded.scheduler.load(eeprom, loaded.image);
    expect(loaded.engine.getSkipChance(1)).toBe(7);
    expect(loaded.engine.getSkipChance(0)).toBe(0);
  });

  it("never lets the offset exceed the single byte the format gives it", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    saved.engine.setSubdiv(0, 128);
    saved.engine.setOffset(0, 300);
    expect(saved.engine.getOffset(0)).toBe(255);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);

    const loaded = rig();
    loaded.scheduler.load(eeprom, loaded.image);
    expect(loaded.engine.getOffset(0)).toBe(255);
  });

  it("reserves the two CV target bytes and reads them as zero", () => {
    const r = rig();
    fillDistinctState(r);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      const base = CHANNELS_OFFSET + ch * CHANNEL_RECORD;
      expect(r.image.byteAt(base + 7)).toBe(0);
      expect(r.image.byteAt(base + 8)).toBe(0);
    }
  });

  it("ignores a stored CV target without disturbing the record", () => {
    const eeprom = new FakeEeprom();
    const saved = rig();
    fillDistinctState(saved);
    saved.scheduler.markDirty(0);
    finishWrite(saved, eeprom, QUIET_MS);
    eeprom.cell[BASE_ADDRESS + CHANNELS_OFFSET + 7] = 0xff;
    eeprom.cell[BASE_ADDRESS + CHANNELS_OFFSET + 8] = 0xff;

    const loaded = rig();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(loaded.engine.getSelectedPattern(0)).toBe(saved.engine.getSelectedPattern(0));
    expect(loaded.engine.getEffectiveLength(0)).toBe(saved.engine.getEffectiveLength(0));
    expect(loaded.engine.getSkipChance(0)).toBe(saved.engine.getSkipChance(0));
    expect(loaded.image.byteAt(CHANNELS_OFFSET + 7)).toBe(0);
  });
});

function instanceStepMask(pattern: Pattern): number {
  let mask = 0;
  for (let step = 0; step < 36; ++step) {
    if (pattern.readStep(step)) mask |= 1 << step;
  }
  return mask;
}


describe("Persistence — the version 4 layout, PRD 11.1 and ADR 0014", () => {
  it("keeps the zones where PRD 11.1 puts them", () => {
    expect(v4.FORMAT_VERSION).toBe(4);
    expect(FORMAT_VERSION).toBe(2);
    expect(v4.HEADER_SIZE).toBe(1);
    expect(v4.BANK_OFFSET).toBe(1);
    expect(v4.BANK_SIZE).toBe(368);
    expect(v4.BANK_RECORD).toBe(23);
    expect(v4.BANK_COUNT).toBe(16);
    expect(v4.CHANNELS_OFFSET).toBe(369);
    expect(v4.CHANNELS_SIZE).toBe(54);
    expect(v4.GLOBAL_OFFSET).toBe(423);
    expect(v4.GLOBAL_SIZE).toBe(5);
    expect(v4.PREFS_OFFSET).toBe(428);
    expect(v4.PREFS_SIZE).toBe(6);
    expect(v4.TOTAL_SIZE).toBe(434);
    expect(v4.LAST_ADDRESS).toBe(817);
  });

  it("carries thirty-six steps in each record, and no length", () => {
    expect(v4.STEP_BYTES).toBe(5);
    expect(v4.RATCHET_BYTES).toBe(18);
    expect(v4.CONTENT_BYTES).toBe(23);
    expect(v4.RECORD_STEPS_AT).toBe(0);
    expect(v4.RECORD_RATCHETS_AT).toBe(5);
    expect(v4.LAST_STEP_BYTE_MASK).toBe(0x0f);
  });

  it("scans the whole image and ends on its version", () => {
    expect(PersistentImageV4.SIZE).toBe(434);
    expect(PersistentImageV4.VERSION_INDEX).toBe(433);
    expect(v4.IMAGE_BANK_AT).toBe(0);
    expect(v4.IMAGE_CHANNELS_AT).toBe(368);
    expect(v4.IMAGE_GLOBAL_AT).toBe(422);
    expect(v4.IMAGE_PREFS_AT).toBe(427);
    expect(v4.IMAGE_VERSION_AT).toBe(433);
  });

  it("reserves MOD and RANGE in the global zone", () => {
    expect(v4.GLOBAL_TEMPO_LO_AT).toBe(0);
    expect(v4.GLOBAL_TEMPO_HI_AT).toBe(1);
    expect(v4.GLOBAL_CLOCK_SOURCE_AT).toBe(2);
    expect(v4.GLOBAL_MOD_AT).toBe(3);
    expect(v4.GLOBAL_RANGE_AT).toBe(4);
  });

  it("leaves the original firmware's memCode alone", () => {
    expect(BASE_ADDRESS + v4.TOTAL_SIZE).toBeLessThanOrEqual(1023);
    expect(1022 - v4.LAST_ADDRESS).toBe(205);
  });

  it("holds the version at the last logical index and the first address", () => {
    const r = rigV4();
    expect(r.image.addressAt(433)).toBe(384);
    expect(r.image.byteAt(433)).toBe(4);
    expect(r.image.addressAt(0)).toBe(385);
    expect(r.image.addressAt(432)).toBe(817);
  });

  it("maps every address of the format exactly once", () => {
    const r = rigV4();
    const seen = new Set<number>();
    for (let index = 0; index < r.image.size; ++index) {
      const address = r.image.addressAt(index);
      expect(seen.has(address)).toBe(false);
      seen.add(address);
    }
    for (let address = 384; address <= 817; ++address) expect(seen.has(address)).toBe(true);
    expect(seen.has(383)).toBe(false);
    expect(seen.has(818)).toBe(false);
  });

  it("factory bytes carry the content alone", () => {
    expect(v4.factoryPatternByte(0, 0)).toBe(0x11);
    expect(v4.factoryPatternByte(0, 1)).toBe(0x91);
    for (let offset = 2; offset < 23; ++offset) expect(v4.factoryPatternByte(0, offset)).toBe(0);
    expect(v4.factoryPatternByte(0, 23)).toBe(0);
    expect(v4.factoryPatternByte(8, 0)).toBe(0);
    expect(v4.factoryPatternByte(16, 0)).toBe(0);
  });
});

describe("Persistence — the version 4 scan and boot", () => {
  it("writes the 434 bytes when all differ, and the version last", () => {
    const r = rigV4();
    const ee = scanEverything(r);
    expect(ee.order.length).toBe(434);
    expect(ee.order[ee.order.length - 1]).toBe(384);
    for (let i = 0; i + 1 < ee.order.length; ++i) {
      expect(ee.order[i]! >= 385 && ee.order[i]! <= 817).toBe(true);
    }
  });

  it("reads MOD and RANGE as zero and normalises a stored value", () => {
    const r = rigV4();
    expect(r.image.byteAt(422 + 3)).toBe(0);
    expect(r.image.byteAt(422 + 4)).toBe(0);
    r.image.applyByte(422 + 3, 0x5a);
    r.image.applyByte(422 + 4, 0xa5);
    expect(r.image.byteAt(422 + 3)).toBe(0);
    expect(r.image.byteAt(422 + 4)).toBe(0);
    expect(r.ui.tempo).toBe(DEFAULT_TEMPO);
    expect(r.ui.clockSource).toBe(0);
  });

  it("round trips the bank and the channels byte for byte", () => {
    const eeprom = new FakeEeprom();
    const saved = rigV4();
    fillDistinctStateV4(saved);
    saved.scheduler.markDirty(0);
    finishWriteV4(saved, eeprom, QUIET_MS);

    const loaded = rigV4();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(sameStateV4(saved, loaded)).toBe(true);
    for (let index = 0; index < 16; ++index) {
      expect(bankStepMask(loaded.bank.getPattern(index)!)).toBe(
        bankStepMask(saved.bank.getPattern(index)!),
      );
      expect(loaded.bank.getPattern(index)!.getRatchet(index % 36)).toBe(RATCHET_3);
    }
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      expect(loaded.engine.getSelectedPattern(ch)).toBe(15 - ch);
      expect(loaded.engine.getBaseLength(ch)).toBe(36 - ch * 3);
      expect(loaded.engine.getCvDestination(ch, CV_SOURCE_1)).toBe((ch + 1) % 5);
      expect(loaded.engine.getCvDestination(ch, CV_SOURCE_2)).toBe((ch + 3) % 5);
    }
    expect(loaded.ui.tempo).toBe(287);
    expect(loaded.prefs.cvCalibration[0]).toBe(-26);
  });

  it("the defaults seed the factory bank and clear the free slots", () => {
    const r = rigV4();
    r.bank.getPattern(9)!.writeStep(2, true);
    r.bank.getPattern(0)!.writeStep(1, true);
    r.bank.getPattern(0)!.setRatchet(0, RATCHET_6);
    r.image.resetToDefaults();
    expect(bankStepMask(r.bank.getPattern(0)!)).toBe(0x9111n);
    expect(bankStepMask(r.bank.getPattern(1)!)).toBe(0x0810n);
    expect(bankStepMask(r.bank.getPattern(7)!)).toBe(0xb733n);
    expect(r.bank.getPattern(0)!.getRatchet(0)).toBe(RATCHET_NONE);
    for (let index = 8; index < 16; ++index) expect(bankStepMask(r.bank.getPattern(index)!)).toBe(0n);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) expect(r.engine.getSelectedPattern(ch)).toBe(0);
    expect(r.ui.tempo).toBe(DEFAULT_TEMPO);
  });

  it("the defaults clear the cv routing and the three settings", () => {
    const r = rigV4();
    r.engine.setCvDestination(2, CV_SOURCE_1, CvDestination.PATTERN);
    r.engine.setCvDestination(2, CV_SOURCE_2, CvDestination.RESET);
    r.engine.setCvDestination(5, CV_SOURCE_2, CvDestination.STEP);
    r.prefs.rotateScreen = 0;
    r.prefs.reverseEncoder = 0;
    r.prefs.cvCalibration[0] = -39;
    r.prefs.cvCalibration[1] = 41;
    r.ui.setTempo(77);
    r.ui.setClockSource(3);

    r.image.resetToDefaults();

    for (let channel = 0; channel < 6; channel += 1) {
      expect(r.engine.getCvDestination(channel, CV_SOURCE_1)).toBe(CvDestination.NONE);
      expect(r.engine.getCvDestination(channel, CV_SOURCE_2)).toBe(CvDestination.NONE);
    }
    expect(r.prefs.rotateScreen).toBe(1);
    expect(r.prefs.reverseEncoder).toBe(1);
    expect(r.prefs.cvCalibration[0]).toBe(0);
    expect(r.prefs.cvCalibration[1]).toBe(0);
    expect(r.ui.tempo).toBe(120);
    expect(r.ui.clockSource).toBe(0);
  });

  it("the defaults reset the mode, the offset and the skip chance", () => {
    const eeprom = new FakeEeprom();
    eeprom.cell[384] = 0xff;
    const r = rigV4();
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      expect(r.engine.setChannelMode(ch, ChannelMode.RANDOM)).toBe(true);
      expect(r.engine.setOffset(ch, 7)).toBe(true);
      expect(r.engine.setSkipChance(ch, 9)).toBe(true);
    }
    expect(bootstrap(eeprom, r.image, r.scheduler, 0)).toBe(false);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) {
      expect(r.engine.getChannelMode(ch)).toBe(DEFAULT_CHANNEL_MODE);
      expect(r.engine.getOffset(ch)).toBe(0);
      expect(r.engine.getSkipChance(ch)).toBe(0);
    }
  });

  it("the first boot seeds the bank in RAM and writes the EEPROM after the quiet delay", () => {
    const ee = new JournalEeprom();
    const r = rigV4();
    expect(bootstrap(ee, r.image, r.scheduler, 0)).toBe(false);
    expect(ee.order.length).toBe(0);
    expect(r.scheduler.isDirty).toBe(true);
    expect(bankStepMask(r.engine.patternForChannel(0)!)).toBe(0x9111n);
    for (let ch = 0; ch < CHANNEL_COUNT; ++ch) expect(r.engine.getSelectedPattern(ch)).toBe(0);

    expect(r.scheduler.advance(ee, r.image, QUIET_MS - 1)).toBe(false);
    expect(ee.order.length).toBe(0);
    while (r.scheduler.advance(ee, r.image, QUIET_MS)) {
      // drains the scan
    }
    expect(ee.order.length).toBe(434);
    expect(ee.cell[384]).toBe(4);
    expect(ee.cell[385]).toBe(0x11);
    expect(ee.cell[386]).toBe(0x91);
    expect(ee.cell[385 + 8 * 23]).toBe(0);
    expect(ee.order[ee.order.length - 1]).toBe(384);
  });

  it("a nominal boot restores the bank and writes nothing", () => {
    const eeprom = new FakeEeprom();
    const saved = rigV4();
    saved.image.resetToDefaults();
    saved.bank.getPattern(3)!.writeStep(30, true);
    saved.bank.getPattern(0)!.writeStep(2, true);
    saved.engine.setSelectedPattern(2, 3);
    saved.scheduler.markDirty(0);
    finishWriteV4(saved, eeprom, QUIET_MS);
    const written = eeprom.writes;

    const loaded = rigV4();
    expect(bootstrap(eeprom, loaded.image, loaded.scheduler, 0)).toBe(true);
    expect(loaded.scheduler.isDirty).toBe(false);
    expect(eeprom.writes).toBe(written);
    expect(bankStepMask(loaded.bank.getPattern(0)!)).toBe(0x9111n | (1n << 2n));
    expect(bankStepMask(loaded.bank.getPattern(3)!)).toBe(0xccccn | (1n << 30n));
    expect(loaded.engine.getSelectedPattern(2)).toBe(3);
    expect(loaded.engine.patternForChannel(2)).toBe(loaded.bank.getPattern(3));
  });

  it("refuses a valid version 2 image without migration", () => {
    const eeprom = new FakeEeprom();
    const legacy = rig();
    legacy.scheduler.markDirty(0);
    finishWrite(legacy, eeprom, QUIET_MS);
    expect(eeprom.cell[384]).toBe(2);

    const r = rigV4();
    expect(bootstrap(eeprom, r.image, r.scheduler, 0)).toBe(false);
    expect(bankStepMask(r.bank.getPattern(0)!)).toBe(0x9111n);
  });

  it("refuses a valid version 3 image without migration", () => {
    const eeprom = new FakeEeprom();
    for (let address = 384; address <= 971; ++address) eeprom.cell[address] = 0;
    eeprom.cell[384] = 3;
    eeprom.cell[385] = 0x11;
    eeprom.cell[386] = 0x91;

    const r = rigV4();
    r.bank.getPattern(0)!.writeStep(20, true);
    expect(bootstrap(eeprom, r.image, r.scheduler, 0)).toBe(false);
    expect(bankStepMask(r.bank.getPattern(0)!)).toBe(0x9111n);
    expect(r.scheduler.isDirty).toBe(true);
  });

  it("falls back to the defaults for every unknown version byte", () => {
    for (const version of [0xff, 1, 5]) {
      const eeprom = new FakeEeprom();
      eeprom.cell[384] = version;
      const r = rigV4();
      r.bank.getPattern(0)!.writeStep(20, true);
      expect(bootstrap(eeprom, r.image, r.scheduler, 0)).toBe(false);
      expect(bankStepMask(r.bank.getPattern(0)!)).toBe(0x9111n);
    }
  });

  it("one edited step of a shared pattern costs one write", () => {
    const eeprom = new FakeEeprom();
    const r = rigV4();
    r.image.resetToDefaults();
    r.scheduler.markDirty(0);
    finishWriteV4(r, eeprom, QUIET_MS);
    const before = eeprom.writes;
    r.bank.getPattern(9)!.writeStep(3, true);
    r.scheduler.markDirty(1000);
    finishWriteV4(r, eeprom, 1000 + QUIET_MS);
    expect(eeprom.writes - before).toBe(1);
    expect(eeprom.cell[385 + 9 * 23]).toBe(0x08);
  });

  it("never leaves its zone", () => {
    const eeprom = new FakeEeprom();
    const r = rigV4();
    fillDistinctStateV4(r);
    r.scheduler.markDirty(0);
    finishWriteV4(r, eeprom, QUIET_MS);
    expect(eeprom.lowestWrite).toBeGreaterThanOrEqual(384);
    expect(eeprom.highestWrite).toBeLessThanOrEqual(817);
    for (let address = 0; address < 384; ++address) expect(eeprom.cell[address]).toBe(SENTINEL);
    for (let address = 818; address < EEPROM_SIZE; ++address) expect(eeprom.cell[address]).toBe(SENTINEL);
  });

  it("a power cut in the middle of a write is caught by the version byte", () => {
    const eeprom = new FakeEeprom();
    const r = rigV4();
    fillDistinctStateV4(r);
    r.scheduler.markDirty(0);
    for (let i = 0; i < 100; ++i) expect(r.scheduler.advance(eeprom, r.image, QUIET_MS)).toBe(true);
    expect(eeprom.cell[384]).not.toBe(4);

    const loaded = rigV4();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(false);
    expect(bankStepMask(loaded.bank.getPattern(0)!)).toBe(0x9111n);
  });

  it("an image without a bank reads zero and applies nothing", () => {
    const engine = new SequencerEngine();
    const ui = new UiController(engine, new Transport(engine));
    const image = new PersistentImageV4(engine, ui, defaultPreferences());
    for (let index = 0; index < 368; ++index) expect(image.byteAt(index)).toBe(0);
    image.applyByte(0, 0xff);
    image.resetToDefaults();
    expect(image.byteAt(0)).toBe(0);
    expect(image.byteAt(433)).toBe(4);
  });

  it("never restores a hysteresis zone", () => {
    const eeprom = new FakeEeprom();
    const saved = rigV4();
    saved.engine.setChannelMode(0, ChannelMode.SEQ);
    saved.engine.setCvDestination(0, CV_SOURCE_1, CvDestination.LENGTH);
    saved.engine.setCvInput(CV_SOURCE_1, 330);
    saved.engine.start();
    saved.engine.advance(96);
    expect(saved.engine.getEffectiveLength(0)).toBe(26);
    saved.scheduler.markDirty(0);
    finishWriteV4(saved, eeprom, QUIET_MS);

    const loaded = rigV4();
    expect(loaded.scheduler.load(eeprom, loaded.image)).toBe(true);
    expect(loaded.engine.getCvDestination(0, CV_SOURCE_1)).toBe(CvDestination.LENGTH);
    expect(loaded.engine.getBaseLength(0)).toBe(16);
    expect(loaded.engine.getEffectiveLength(0)).toBe(16);
  });
});
