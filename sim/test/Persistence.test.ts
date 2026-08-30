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
  PersistentImageV3,
  bootstrap,
  PREFS_OFFSET,
  QUIET_MS,
  TOTAL_SIZE,
  v3,
  defaultPreferences,
  type Preferences,
  type Storage,
} from "../src/domain/Persistence.js";
import { PatternBank } from "../src/domain/PatternBank.js";
import {
  CHANNEL_COUNT,
  ChannelMode,
  DEFAULT_CHANNEL_MODE,
  DEFAULT_LENGTH,
  MAX_LENGTH,
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

function rigV3() {
  const engine = new SequencerEngine();
  const transport = new Transport(engine);
  const ui = new UiController(engine, transport);
  const prefs: Preferences = defaultPreferences();
  const image = new PersistentImageV3(engine, ui, prefs);
  const scheduler = new PersistenceScheduler();
  return { engine, transport, ui, prefs, image, scheduler };
}

function scanEverything(r: ReturnType<typeof rigV3>): JournalEeprom {
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

describe("Persistence — the version 3 layout, declared but not in service", () => {
  it("keeps the zones where PRD 11.1 puts them", () => {
    expect(v3.HEADER_SIZE).toBe(1);
    expect(v3.TEMPLATES_OFFSET).toBe(1);
    expect(v3.TEMPLATES_SIZE).toBe(384);
    expect(v3.INSTANCES_OFFSET).toBe(385);
    expect(v3.INSTANCES_SIZE).toBe(138);
    expect(v3.CHANNELS_OFFSET).toBe(523);
    expect(v3.CHANNELS_SIZE).toBe(54);
    expect(v3.GLOBAL_OFFSET).toBe(577);
    expect(v3.GLOBAL_SIZE).toBe(5);
    expect(v3.PREFS_OFFSET).toBe(582);
    expect(v3.PREFS_SIZE).toBe(6);
    expect(v3.TOTAL_SIZE).toBe(588);
    expect(v3.LAST_ADDRESS).toBe(971);
  });

  it("is version 3, and leaves version 2 alone", () => {
    expect(v3.FORMAT_VERSION).toBe(3);
    expect(FORMAT_VERSION).toBe(2);
  });

  it("carries thirty-six steps in each record", () => {
    expect(v3.STEP_BYTES).toBe(5);
    expect(v3.RATCHET_BYTES).toBe(18);
    expect(v3.CONTENT_BYTES).toBe(23);
    expect(v3.LENGTH_BYTES).toBe(1);
    expect(v3.TEMPLATE_RECORD).toBe(24);
    expect(v3.INSTANCE_RECORD).toBe(23);
    expect(v3.TEMPLATE_COUNT).toBe(16);
    expect(v3.INSTANCE_COUNT).toBe(6);
    expect(v3.CHANNEL_RECORD).toBe(9);
    expect(v3.RECORD_STEPS_AT).toBe(0);
    expect(v3.RECORD_RATCHETS_AT).toBe(5);
    expect(v3.RECORD_LENGTH_AT).toBe(23);
  });

  it("reserves MOD and RANGE in the global zone", () => {
    expect(v3.GLOBAL_TEMPO_LO_AT).toBe(0);
    expect(v3.GLOBAL_TEMPO_HI_AT).toBe(1);
    expect(v3.GLOBAL_CLOCK_SOURCE_AT).toBe(2);
    expect(v3.GLOBAL_MOD_AT).toBe(3);
    expect(v3.GLOBAL_RANGE_AT).toBe(4);
  });

  it("leaves the original firmware's memCode alone", () => {
    expect(BASE_ADDRESS + v3.TOTAL_SIZE).toBeLessThanOrEqual(1023);
    expect(1022 - v3.LAST_ADDRESS).toBe(51);
  });
});

function fillDistinctPattern(pattern: Pattern): void {
  for (const step of [0, 7, 8, 23, 31, 32, 35]) pattern.writeStep(step, true);
  pattern.setRatchet(0, RATCHET_3);
  pattern.setRatchet(1, RATCHET_6);
  pattern.setRatchet(34, RATCHET_TRIPLET);
  pattern.setRatchet(35, RATCHET_4);
}

describe("Persistence — the version 3 content codec", () => {
  it("round trips the twenty-three bytes", () => {
    const source = new Pattern();
    fillDistinctPattern(source);

    const image = Array.from({ length: 23 }, (_, offset) => v3.contentByte(source, offset));

    const loaded = new Pattern();
    for (let offset = 0; offset < 23; ++offset) v3.applyContentByte(loaded, offset, image[offset] ?? 0);

    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      expect(loaded.readStep(step)).toBe(source.readStep(step));
      expect(loaded.getRatchet(step)).toBe(source.getRatchet(step));
    }
    for (let offset = 0; offset < 23; ++offset) {
      expect(v3.contentByte(loaded, offset)).toBe(image[offset]);
    }
  });

  it("drops the four bits above the last step on load", () => {
    const pattern = new Pattern();
    v3.applyContentByte(pattern, 4, 0xff);

    expect(v3.contentByte(pattern, 4)).toBe(0x0f);
    for (let step = 32; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      expect(pattern.readStep(step)).toBe(true);
    }
  });

  it("has no representation at all for the four bits above the last step", () => {
    const pattern = new Pattern();
    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) pattern.writeStep(step, true);

    expect(v3.contentByte(pattern, 4)).toBe(0x0f);
    expect(pattern.writeStep(36, true)).toBe(false);
    expect(pattern.readStep(36)).toBe(null);
    expect(v3.contentByte(pattern, 4)).toBe(0x0f);
  });

  it("leaves the other step bytes whole", () => {
    const pattern = new Pattern();
    for (let offset = 0; offset < 4; ++offset) {
      v3.applyContentByte(pattern, offset, 0xff);
      expect(v3.contentByte(pattern, offset)).toBe(0xff);
    }
  });

  it("normalises an invalid ratchet nibble", () => {
    const pattern = new Pattern();
    v3.applyContentByte(pattern, 5, 0x53);

    expect(pattern.getRatchet(0)).toBe(RATCHET_3);
    expect(pattern.getRatchet(1)).toBe(RATCHET_NONE);
    expect(v3.contentByte(pattern, 5)).toBe(0x03);
  });

  it("replaces a previous ratchet when the stored nibble is invalid", () => {
    const pattern = new Pattern();
    v3.applyContentByte(pattern, 5, 0x36);
    expect(pattern.getRatchet(0)).toBe(RATCHET_6);
    expect(pattern.getRatchet(1)).toBe(RATCHET_3);

    v3.applyContentByte(pattern, 5, 0x51);
    expect(pattern.getRatchet(0)).toBe(RATCHET_NONE);
    expect(pattern.getRatchet(1)).toBe(RATCHET_NONE);
    expect(v3.contentByte(pattern, 5)).toBe(0);
  });

  it("ignores an offset past the record", () => {
    const pattern = new Pattern();
    for (let offset = 0; offset < 23; ++offset) v3.applyContentByte(pattern, offset, (0x11 * offset) & 0xff);

    const before = Array.from({ length: 23 }, (_, offset) => v3.contentByte(pattern, offset));

    v3.applyContentByte(pattern, 23, 0xff);
    v3.applyContentByte(pattern, 200, 0xff);

    for (let offset = 0; offset < 23; ++offset) {
      expect(v3.contentByte(pattern, offset)).toBe(before[offset]);
    }
    expect(v3.contentByte(pattern, 23)).toBe(0);
    expect(v3.contentByte(pattern, 200)).toBe(0);
  });
});

describe("Persistence — the version 3 template record", () => {
  it("round trips its twenty-four bytes", () => {
    const source = new Pattern();
    fillDistinctPattern(source);

    const image = Array.from({ length: 24 }, (_, offset) => v3.templateByte(source, 20, offset));

    const loaded = new Pattern();
    const length = { value: 1 };
    for (let offset = 0; offset < 24; ++offset) {
      expect(v3.applyTemplateByte(loaded, length, offset, image[offset] ?? 0)).toBe(true);
    }

    expect(length.value).toBe(20);
    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      expect(loaded.readStep(step)).toBe(source.readStep(step));
      expect(loaded.getRatchet(step)).toBe(source.getRatchet(step));
    }
    for (let offset = 0; offset < 24; ++offset) {
      expect(v3.templateByte(loaded, length.value, offset)).toBe(image[offset]);
    }
  });

  it("accepts every length in range", () => {
    for (const wanted of [1, 16, 24, 35, 36]) {
      const pattern = new Pattern();
      const length = { value: 8 };
      expect(v3.applyTemplateByte(pattern, length, 23, wanted)).toBe(true);
      expect(length.value).toBe(wanted);
      expect(v3.templateByte(pattern, length.value, 23)).toBe(wanted);
    }
  });

  it("refuses a length out of range", () => {
    for (const refused of [0, 37, 255]) {
      const pattern = new Pattern();
      const length = { value: 12 };
      expect(v3.applyTemplateByte(pattern, length, 23, refused)).toBe(false);
      expect(length.value).toBe(12);
    }
  });

  it("keeps the loaded content when the length is refused", () => {
    const source = new Pattern();
    fillDistinctPattern(source);

    const loaded = new Pattern();
    const length = { value: 12 };
    for (let offset = 0; offset < 23; ++offset) {
      expect(v3.applyTemplateByte(loaded, length, offset, v3.contentByte(source, offset))).toBe(true);
    }

    expect(v3.applyTemplateByte(loaded, length, 23, 0)).toBe(false);
    expect(length.value).toBe(12);

    for (let offset = 0; offset < 23; ++offset) {
      expect(v3.contentByte(loaded, offset)).toBe(v3.contentByte(source, offset));
    }
  });

  it("clamps the length it emits", () => {
    const pattern = new Pattern();
    expect(v3.templateByte(pattern, 0, 23)).toBe(1);
    expect(v3.templateByte(pattern, 37, 23)).toBe(36);
    expect(v3.templateByte(pattern, 255, 23)).toBe(36);
    expect(v3.templateByte(pattern, 1, 23)).toBe(1);
    expect(v3.templateByte(pattern, 36, 23)).toBe(36);
    expect(v3.templateByte(pattern, 20, 23)).toBe(20);
  });

  it("lets the length byte touch no content byte", () => {
    const pattern = new Pattern();
    fillDistinctPattern(pattern);

    for (let offset = 0; offset < 23; ++offset) {
      expect(v3.templateByte(pattern, 4, offset)).toBe(v3.templateByte(pattern, 33, offset));
      expect(v3.templateByte(pattern, 4, offset)).toBe(v3.contentByte(pattern, offset));
    }
    expect(v3.templateByte(pattern, 4, 23)).toBe(4);
    expect(v3.templateByte(pattern, 33, 23)).toBe(33);
  });

  it("carries no length in the instance record", () => {
    const source = new Pattern();
    fillDistinctPattern(source);

    const loaded = new Pattern();
    for (let offset = 0; offset < 23; ++offset) {
      v3.applyContentByte(loaded, offset, v3.contentByte(source, offset));
    }

    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      expect(loaded.readStep(step)).toBe(source.readStep(step));
      expect(loaded.getRatchet(step)).toBe(source.getRatchet(step));
    }
  });

  it("bounds the length by the pattern capacity, never by the engine cap", () => {
    expect(v3.MIN_TEMPLATE_LENGTH).toBe(1);
    expect(v3.MAX_TEMPLATE_LENGTH).toBe(36);
    expect(MAX_LENGTH).toBe(36);

    const pattern = new Pattern();
    const length = { value: 1 };
    expect(v3.applyTemplateByte(pattern, length, 23, 36)).toBe(true);
    expect(length.value).toBe(36);
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

describe("le demarrage v3", () => {
  it("seme les templates et remplit les six instances depuis A1", () => {
    const ee = new JournalEeprom();
    const r = rigV3();
    const restored = bootstrap(ee, r.image, r.scheduler, 0);

    expect(restored).toBe(false);
    expect(ee.cell[385]).toBe(0x11);
    expect(ee.cell[386]).toBe(0x91);
    expect(ee.cell[385 + 23]).toBe(16);
    expect(ee.order.length).toBe(384);
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      expect(r.engine.getSelectedPattern(channel)).toBe(0);
      expect(instanceStepMask(r.engine.instanceForChannel(channel)!)).toBe(0x9111);
    }
  });

  it("restaure les instances et ne les ecrase JAMAIS au demarrage nominal", () => {
    const ee = new FakeEeprom();
    const saved = rigV3();
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      saved.engine.instanceForChannel(channel)!.writeStep(30 + channel, true);
    }
    saved.scheduler.markDirty(0);
    while (saved.scheduler.advance(ee, saved.image, QUIET_MS)) {
      // draine le parcours complet
    }

    const loaded = rigV3();
    expect(bootstrap(ee, loaded.image, loaded.scheduler, 0)).toBe(true);
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      expect(instanceStepMask(loaded.engine.instanceForChannel(channel)!)).toBe(
        1 << (30 + channel),
      );
    }
  });

  it("refuse une image v2 valide, sans migration", () => {
    const ee = new FakeEeprom();
    const legacy = rig();
    legacy.scheduler.markDirty(0);
    while (legacy.scheduler.advance(ee, legacy.image, QUIET_MS)) {
      // draine le parcours complet
    }
    expect(ee.cell[384]).toBe(2);

    const r = rigV3();
    expect(bootstrap(ee, r.image, r.scheduler, 0)).toBe(false);
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      expect(instanceStepMask(r.engine.instanceForChannel(channel)!)).toBe(0x9111);
    }
  });

  it("retombe sur les defauts pour toute version inconnue", () => {
    for (const version of [0xff, 1, 4]) {
      const ee = new FakeEeprom();
      ee.cell[384] = version;
      const r = rigV3();
      expect(bootstrap(ee, r.image, r.scheduler, 0)).toBe(false);
      expect(instanceStepMask(r.engine.instanceForChannel(0)!)).toBe(0x9111);
    }
  });
});

describe("l'image balayee v3", () => {
  it("expose 204 octets logiques et la version au dernier index", () => {
    const r = rigV3();
    expect(r.image.size).toBe(204);
    expect(r.image.versionIndex).toBe(203);
    expect(r.image.size).toBe(PersistentImageV3.SIZE);
    expect(r.image.versionIndex).toBe(PersistentImageV3.VERSION_INDEX);
  });

  it("place la version a la premiere adresse et la lit comme 3", () => {
    const r = rigV3();
    expect(r.image.addressAt(203)).toBe(384);
    expect(r.image.byteAt(203)).toBe(3);
  });

  it("ne mappe jamais un octet logique dans la zone des templates", () => {
    const r = rigV3();
    for (let index = 0; index < r.image.size; ++index) {
      const address = r.image.addressAt(index);
      expect(address < 385 || address > 768).toBe(true);
    }
  });

  it("couvre la version et la zone de donnees, une adresse par index", () => {
    const r = rigV3();
    const seen = new Set<number>();
    for (let index = 0; index < r.image.size; ++index) {
      const address = r.image.addressAt(index);
      expect(seen.has(address)).toBe(false);
      seen.add(address);
    }
    expect(seen.has(384)).toBe(true);
    for (let address = 385; address <= 768; ++address) expect(seen.has(address)).toBe(false);
    for (let address = 769; address <= 971; ++address) expect(seen.has(address)).toBe(true);
    expect(seen.has(972)).toBe(false);
  });

  it("lit MOD et RANGE comme zero", () => {
    const r = rigV3();
    expect(r.image.byteAt(192 + 3)).toBe(0);
    expect(r.image.byteAt(192 + 4)).toBe(0);
  });

  it("ramene a zero une valeur stockee dans MOD ou RANGE", () => {
    const r = rigV3();
    r.image.applyByte(192 + 3, 0x5a);
    r.image.applyByte(192 + 4, 0xa5);
    expect(r.image.byteAt(192 + 3)).toBe(0);
    expect(r.image.byteAt(192 + 4)).toBe(0);
    expect(r.ui.tempo).toBe(DEFAULT_TEMPO);
    expect(r.ui.clockSource).toBe(0);
  });

  it("fait un tour complet sur les six instances", () => {
    const saved = rigV3();
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      const instance = saved.engine.instanceForChannel(channel)!;
      instance.writeStep(channel, true);
      instance.setRatchet(channel, RATCHET_3);
    }
    saved.ui.setTempo(143);
    const ee = scanEverything(saved);

    const loaded = rigV3();
    expect(loaded.scheduler.load(ee, loaded.image)).toBe(true);
    expect(loaded.ui.tempo).toBe(143);
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      const instance = loaded.engine.instanceForChannel(channel)!;
      expect(instance.readStep(channel)).toBe(true);
      expect(instance.getRatchet(channel)).toBe(RATCHET_3);
    }
  });

  it("vide les six instances sans banque", () => {
    const r = rigV3();
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      r.engine.instanceForChannel(channel)!.writeStep(0, true);
    }
    r.image.resetToDefaults();
    for (let channel = 0; channel < CHANNEL_COUNT; ++channel) {
      expect(r.engine.instanceForChannel(channel)!.readStep(0)).toBe(false);
    }
    expect(r.ui.tempo).toBe(DEFAULT_TEMPO);
  });
});

describe("l'ordre des ecritures", () => {
  it("ecrit les 204 octets logiques quand tous different", () => {
    const ee = scanEverything(rigV3());
    expect(ee.order.length).toBe(204);
  });

  it("ne touche que la version et la zone de donnees", () => {
    const ee = scanEverything(rigV3());
    for (const address of ee.order) {
      expect(address === 384 || (address >= 769 && address <= 971)).toBe(true);
      expect(address < 385 || address > 768).toBe(true);
      expect(address).toBeLessThan(972);
    }
  });

  it("ecrit la version en DERNIER en v3", () => {
    const ee = scanEverything(rigV3());
    expect(ee.order[ee.order.length - 1]).toBe(384);
    for (let i = 0; i + 1 < ee.order.length; ++i) {
      expect(ee.order[i]).toBeGreaterThanOrEqual(769);
    }
  });

  it("ecrit la version en PREMIER en v2, et ce contrat ne change pas", () => {
    const ee = new JournalEeprom();
    const r = rig();
    for (let index = 0; index < r.image.size; ++index) {
      ee.cell[r.image.addressAt(index)] = r.image.byteAt(index) ^ 0xff;
    }
    ee.order = [];
    r.scheduler.markDirty(0);
    while (r.scheduler.advance(ee, r.image, QUIET_MS)) {
      // draine le parcours complet
    }
    expect(ee.order.length).toBe(304);
    expect(ee.order[0]).toBe(384);
  });
});

describe("la disposition de l'image balayee v3", () => {
  it("compte 204 octets logiques et place la version en dernier", () => {
    expect(v3.IMAGE_SIZE).toBe(204);
    expect(v3.IMAGE_VERSION_AT).toBe(203);
    expect(v3.TOTAL_SIZE).toBe(588);
    expect(v3.TEMPLATES_SIZE).toBe(384);
  });

  it("couvre le format entier avec la zone des templates", () => {
    expect(v3.IMAGE_SIZE + v3.TEMPLATES_SIZE).toBe(v3.TOTAL_SIZE);
  });

  it("donne a la version le dernier index logique", () => {
    expect(v3.IMAGE_VERSION_AT).toBe(v3.IMAGE_SIZE - 1);
  });

  it("enchaine les zones sans trou ni recouvrement", () => {
    expect(v3.IMAGE_INSTANCES_AT).toBe(0);
    expect(v3.IMAGE_CHANNELS_AT).toBe(138);
    expect(v3.IMAGE_GLOBAL_AT).toBe(192);
    expect(v3.IMAGE_PREFS_AT).toBe(197);
    expect(v3.IMAGE_CHANNELS_AT - v3.IMAGE_INSTANCES_AT).toBe(v3.INSTANCES_SIZE);
    expect(v3.IMAGE_GLOBAL_AT - v3.IMAGE_CHANNELS_AT).toBe(v3.CHANNELS_SIZE);
    expect(v3.IMAGE_PREFS_AT - v3.IMAGE_GLOBAL_AT).toBe(v3.GLOBAL_SIZE);
    expect(v3.IMAGE_VERSION_AT - v3.IMAGE_PREFS_AT).toBe(v3.PREFS_SIZE);
  });
});

describe("le contrat d'image partage par le scheduler", () => {
  it("expose les metadonnees d'instance derivees des constantes statiques", () => {
    const r = rig();
    expect(r.image.size).toBe(PersistentImage.SIZE);
    expect(r.image.versionIndex).toBe(PersistentImage.VERSION_INDEX);
    expect(r.image.size).toBe(304);
    expect(r.image.versionIndex).toBe(0);
  });

  it("mappe l'index logique v2 sur l'identite que le scheduler inlinait", () => {
    const r = rig();
    for (let index = 0; index < r.image.size; ++index) {
      expect(r.image.addressAt(index)).toBe(384 + index);
    }
    expect(r.image.byteAt(r.image.versionIndex)).toBe(2);
  });

  it("fait lire au scheduler la taille de l'instance et non celle de la classe", () => {
    const eeprom = new FakeEeprom();
    const r = rig();
    const shortened = {
      size: 4,
      versionIndex: r.image.versionIndex,
      addressAt: (index: number) => r.image.addressAt(index),
      byteAt: (index: number) => r.image.byteAt(index),
      applyByte: (index: number, value: number) => r.image.applyByte(index, value),
      resetToDefaults: () => r.image.resetToDefaults(),
      templateRecordSize: r.image.templateRecordSize,
      canWriteTemplate: () => false,
      templateAddressAt: () => 0,
      templateByteAt: () => 0,
    };
    r.scheduler.markDirty(0);
    let calls = 0;
    while (r.scheduler.advance(eeprom, shortened, QUIET_MS)) {
      ++calls;
    }
    expect(calls).toBe(4);
    expect(eeprom.writes).toBe(4);
    expect(eeprom.highestWrite).toBe(384 + 3);
  });
});

describe("loadTemplate — template EEPROM vers instance (ADR 0009)", () => {
  function seedTemplate(ee: FakeEeprom, index: number, content: Pattern, length: number): void {
    for (let offset = 0; offset < v3.TEMPLATE_RECORD; ++offset) {
      ee.write(v3.templateAddress(index, offset), v3.templateByte(content, length, offset));
    }
  }

  function distinctContent(seed: number): Pattern {
    const p = new Pattern();
    p.writeStep(seed % Pattern.DEFAULT_TOTAL_STEPS, true);
    p.writeStep((seed + 7) % Pattern.DEFAULT_TOTAL_STEPS, true);
    p.setRatchet(seed % Pattern.DEFAULT_TOTAL_STEPS, RATCHET_3);
    return p;
  }

  function sameContent(a: Pattern, b: Pattern): boolean {
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      if (a.readStep(i) !== b.readStep(i)) return false;
      if (a.getRatchet(i) !== b.getRatchet(i)) return false;
    }
    return true;
  }

  it("copie le contenu dans l'instance du channel vise", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const wanted = distinctContent(5);
    seedTemplate(ee, 5, wanted, 12);
    expect(r.image.loadTemplate(ee, 2, 5)).toBe(true);
    expect(sameContent(wanted, r.engine.instanceForChannel(2)!)).toBe(true);
  });

  it("laisse les cinq autres instances intactes", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 5, distinctContent(5), 12);
    const before = new Pattern();
    expect(r.image.loadTemplate(ee, 2, 5)).toBe(true);
    for (let ch = 0; ch < 6; ++ch) {
      if (ch === 2) continue;
      expect(sameContent(before, r.engine.instanceForChannel(ch)!)).toBe(true);
    }
  });

  it("garde une longueur de trente-six dans la base", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 9, distinctContent(3), 36);
    expect(r.image.loadTemplate(ee, 0, 9)).toBe(true);
    expect(r.engine.getBaseLength(0)).toBe(36);
    expect(r.engine.getEffectiveLength(0)).toBe(36);
    expect(r.engine.getSelectedPattern(0)).toBe(9);
  });

  it("n'ecrete pas une longueur sous le plafond", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 4, distinctContent(1), 12);
    expect(r.image.loadTemplate(ee, 1, 4)).toBe(true);
    expect(r.engine.getBaseLength(1)).toBe(12);
    expect(r.engine.getEffectiveLength(1)).toBe(12);
  });

  it("refuse une longueur invalide sans perdre le contenu", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const wanted = distinctContent(2);
    seedTemplate(ee, 6, wanted, 20);
    ee.write(v3.templateAddress(6, v3.RECORD_LENGTH_AT), 37);
    expect(r.engine.setBaseLength(3, 8)).toBe(true);
    expect(r.image.loadTemplate(ee, 3, 6)).toBe(true);
    expect(sameContent(wanted, r.engine.instanceForChannel(3)!)).toBe(true);
    expect(r.engine.getBaseLength(3)).toBe(8);
  });

  it("accepte un slot d'usine gele", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const wanted = distinctContent(8);
    seedTemplate(ee, 0, wanted, 16);
    expect(r.image.loadTemplate(ee, 4, 0)).toBe(true);
    expect(sameContent(wanted, r.engine.instanceForChannel(4)!)).toBe(true);
    expect(r.engine.getSelectedPattern(4)).toBe(0);
  });

  it("refuse un channel ou un index invalide", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 1, distinctContent(4), 10);
    expect(r.image.loadTemplate(ee, 6, 1)).toBe(false);
    expect(r.image.loadTemplate(ee, 0, 16)).toBe(false);
    const before = new Pattern();
    for (let ch = 0; ch < 6; ++ch) {
      expect(sameContent(before, r.engine.instanceForChannel(ch)!)).toBe(true);
    }
  });

  it("n'ecrit rien dans l'EEPROM", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 7, distinctContent(6), 18);
    const before = ee.writes;
    expect(r.image.loadTemplate(ee, 5, 7)).toBe(true);
    expect(ee.writes).toBe(before);
  });
});

describe("saveTemplate — instance vers template EEPROM (ADR 0009)", () => {
  function distinctContent(seed: number): Pattern {
    const p = new Pattern();
    p.writeStep(seed % Pattern.DEFAULT_TOTAL_STEPS, true);
    p.writeStep((seed + 7) % Pattern.DEFAULT_TOTAL_STEPS, true);
    p.setRatchet(seed % Pattern.DEFAULT_TOTAL_STEPS, RATCHET_3);
    return p;
  }

  function copyInto(target: Pattern, source: Pattern): void {
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      target.writeStep(i, source.readStep(i) === true);
      target.setRatchet(i, source.getRatchet(i));
    }
  }

  it("ecrit le contenu de l'instance dans le record", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const wanted = distinctContent(3);
    copyInto(r.engine.instanceForChannel(1)!, wanted);
    expect(r.image.saveTemplate(ee, 1, 9)).toBe(true);
    for (let offset = 0; offset < v3.CONTENT_BYTES; ++offset) {
      expect(ee.read(v3.templateAddress(9, offset))).toBe(v3.contentByte(wanted, offset));
    }
  });

  it("ecrit la longueur du canal dans l'enregistrement", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    expect(r.engine.setBaseLengthFromStorage(1, 36)).toBe(true);
    expect(r.engine.getEffectiveLength(1)).toBe(36);
    expect(r.image.saveTemplate(ee, 1, 9)).toBe(true);
    expect(ee.read(v3.templateAddress(9, v3.RECORD_LENGTH_AT))).toBe(36);
  });

  it("refuse les huit slots geles sans ecrire", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    for (let index = 0; index < 8; ++index) {
      const before = ee.writes;
      expect(r.image.saveTemplate(ee, 0, index)).toBe(false);
      expect(ee.writes).toBe(before);
    }
  });

  it("accepte les huit slots inscriptibles", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    for (let index = 8; index < 16; ++index) {
      expect(r.image.saveTemplate(ee, 0, index)).toBe(true);
    }
  });

  it("refuse un channel ou un index invalide sans ecrire", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const before = ee.writes;
    expect(r.image.saveTemplate(ee, 6, 9)).toBe(false);
    expect(r.image.saveTemplate(ee, 0, 16)).toBe(false);
    expect(ee.writes).toBe(before);
  });

  it("un aller-retour save puis load rend le meme pattern et la meme longueur", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const wanted = distinctContent(6);
    copyInto(r.engine.instanceForChannel(2)!, wanted);
    expect(r.engine.setBaseLengthFromStorage(2, 30)).toBe(true);
    expect(r.image.saveTemplate(ee, 2, 12)).toBe(true);
    expect(r.image.loadTemplate(ee, 5, 12)).toBe(true);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      expect(r.engine.instanceForChannel(5)!.readStep(i)).toBe(wanted.readStep(i));
      expect(r.engine.instanceForChannel(5)!.getRatchet(i)).toBe(wanted.getRatchet(i));
    }
    expect(r.engine.getBaseLength(5)).toBe(30);
    expect(r.engine.getEffectiveLength(5)).toBe(30);
  });
});

describe("ecriture differee d'un template par l'ordonnanceur (B4b.6.2b)", () => {
  function distinctContent(seed: number): Pattern {
    const p = new Pattern();
    p.writeStep(seed % Pattern.DEFAULT_TOTAL_STEPS, true);
    p.setRatchet(seed % Pattern.DEFAULT_TOTAL_STEPS, RATCHET_3);
    return p;
  }

  it("une demande valide s'arme et se declare", () => {
    const r = rigV3();
    expect(r.scheduler.isWritingTemplate).toBe(false);
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 9)).toBe(true);
    expect(r.scheduler.isWritingTemplate).toBe(true);
  });

  it("ecrit un octet par appel a advance", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 9)).toBe(true);
    for (let written = 0; written < v3.TEMPLATE_RECORD; ++written) {
      const before = ee.writes;
      expect(r.scheduler.advance(ee, r.image, 0)).toBe(true);
      expect(ee.writes).toBe(before + 1);
    }
  });

  it("se termine apres son record et laisse le bon contenu", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const wanted = distinctContent(4);
    for (let i = 0; i < Pattern.DEFAULT_TOTAL_STEPS; ++i) {
      r.engine.instanceForChannel(0)!.writeStep(i, wanted.readStep(i) === true);
      r.engine.instanceForChannel(0)!.setRatchet(i, wanted.getRatchet(i));
    }
    expect(r.engine.setBaseLengthFromStorage(0, 21)).toBe(true);
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 11)).toBe(true);
    for (let i = 0; i < v3.TEMPLATE_RECORD; ++i) r.scheduler.advance(ee, r.image, 0);
    expect(r.scheduler.isWritingTemplate).toBe(false);
    for (let offset = 0; offset < v3.CONTENT_BYTES; ++offset) {
      expect(ee.read(v3.templateAddress(11, offset))).toBe(v3.contentByte(wanted, offset));
    }
    expect(ee.read(v3.templateAddress(11, v3.RECORD_LENGTH_AT))).toBe(21);
  });

  it("une demande sur un slot gele n'arme rien", () => {
    const r = rigV3();
    for (let index = 0; index < 8; ++index) {
      expect(r.scheduler.requestTemplateWrite(r.image, 0, index)).toBe(false);
    }
    expect(r.scheduler.isWritingTemplate).toBe(false);
  });

  it("une seconde demande est refusee pendant une ecriture en vol", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 9)).toBe(true);
    expect(r.scheduler.advance(ee, r.image, 0)).toBe(true);
    expect(r.scheduler.requestTemplateWrite(r.image, 1, 10)).toBe(false);
  });

  it("le template passe avant le balayage de l'image", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    r.scheduler.markDirty(0);
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 9)).toBe(true);
    expect(r.scheduler.advance(ee, r.image, QUIET_MS)).toBe(true);
    expect(ee.lowestWrite).toBeGreaterThanOrEqual(v3.templateAddress(9, 0));
    expect(r.scheduler.isDirty).toBe(true);
  });

  it("le balayage reprend apres le template", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    r.scheduler.markDirty(0);
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 9)).toBe(true);
    for (let i = 0; i < v3.TEMPLATE_RECORD; ++i) r.scheduler.advance(ee, r.image, QUIET_MS);
    expect(r.scheduler.isWritingTemplate).toBe(false);
    expect(r.scheduler.advance(ee, r.image, QUIET_MS)).toBe(true);
    expect(ee.highestWrite).toBeGreaterThanOrEqual(769);
    expect(r.scheduler.isDirty).toBe(true);
  });

  it("aucun appel a advance n'ecrit plus d'un octet", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    r.scheduler.markDirty(0);
    expect(r.scheduler.requestTemplateWrite(r.image, 0, 13)).toBe(true);
    for (let pass = 0; pass < 400; ++pass) {
      const before = ee.writes;
      if (!r.scheduler.advance(ee, r.image, QUIET_MS)) break;
      expect(ee.writes).toBe(before + 1);
    }
  });
});

describe("isTemplateEmpty — les 36 cases inactives (B4b.6.4)", () => {
  function seedTemplate(ee: FakeEeprom, index: number, content: Pattern, length: number): void {
    for (let offset = 0; offset < v3.TEMPLATE_RECORD; ++offset) {
      ee.write(v3.templateAddress(index, offset), v3.templateByte(content, length, offset));
    }
  }

  it("un template a zero est vide", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 10, new Pattern(), 16);
    expect(r.image.isTemplateEmpty(ee, 10)).toBe(true);
  });

  it("un seul pas actif rend le template occupe", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    for (let step = 0; step < Pattern.DEFAULT_TOTAL_STEPS; ++step) {
      const one = new Pattern();
      one.writeStep(step, true);
      seedTemplate(ee, 10, one, 16);
      expect(r.image.isTemplateEmpty(ee, 10)).toBe(false);
    }
  });

  it("le trente-sixieme pas seul rend le template occupe", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    const last = new Pattern();
    last.writeStep(35, true);
    seedTemplate(ee, 10, last, 16);
    expect(r.image.isTemplateEmpty(ee, 10)).toBe(false);
  });

  it("des ratchets seuls laissent le template vide", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 10, new Pattern(), 16);
    for (let offset = 0; offset < v3.RATCHET_BYTES; ++offset) {
      ee.write(v3.templateAddress(10, v3.RECORD_RATCHETS_AT + offset), 0x66);
    }
    expect(r.image.isTemplateEmpty(ee, 10)).toBe(true);
  });

  it("les quatre bits au-dela du dernier pas n'occupent pas le template", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 10, new Pattern(), 16);
    ee.write(v3.templateAddress(10, v3.RECORD_STEPS_AT + v3.STEP_BYTES - 1), 0xf0);
    expect(r.image.isTemplateEmpty(ee, 10)).toBe(true);
  });

  it("la requete n'ecrit rien", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    seedTemplate(ee, 10, new Pattern(), 16);
    const before = ee.writes;
    expect(r.image.isTemplateEmpty(ee, 10)).toBe(true);
    expect(ee.writes).toBe(before);
  });

  it("un index hors plage n'est pas vide", () => {
    const ee = new FakeEeprom();
    const r = rigV3();
    expect(r.image.isTemplateEmpty(ee, 16)).toBe(false);
  });
});

describe("les defauts de la version 3 (B4b.6.5)", () => {
  it("remettent le mode, l'offset et le skip chance", () => {
    const ee = new FakeEeprom();
    ee.write(384, 0xff);
    const r = rigV3();
    for (let ch = 0; ch < 6; ++ch) {
      expect(r.engine.setChannelMode(ch, ChannelMode.RANDOM)).toBe(true);
      expect(r.engine.setOffset(ch, 7)).toBe(true);
      expect(r.engine.setSkipChance(ch, 9)).toBe(true);
    }
    expect(bootstrap(ee, r.image, r.scheduler, 0)).toBe(false);
    for (let ch = 0; ch < 6; ++ch) {
      expect(r.engine.getChannelMode(ch)).toBe(ChannelMode.CLOCK);
      expect(r.engine.getOffset(ch)).toBe(0);
      expect(r.engine.getSkipChance(ch)).toBe(0);
    }
  });
});
