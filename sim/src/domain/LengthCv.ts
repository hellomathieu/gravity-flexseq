import { MIN_LENGTH, MAX_LENGTH, PATTERN_COUNT } from "./SequencerEngine.js";

export const CV_MIN = -512;
export const CV_MAX = 512;

export const ZONE_WIDTH = 33;
export const ZONE_COUNT = 31;
export const OFFSET_MIN = -15;
export const OFFSET_MAX = 15;

export const HYSTERESIS = 8;
export const STAY_WIDTH = Math.floor(ZONE_WIDTH / 2) + HYSTERESIS;

const HALF_WIDTH = Math.floor(ZONE_WIDTH / 2);

function clampZone(zone: number): number {
  if (zone < OFFSET_MIN) return OFFSET_MIN;
  if (zone > OFFSET_MAX) return OFFSET_MAX;
  return zone === 0 ? 0 : zone;
}

export function zoneFor(cv: number): number {
  if (cv >= 0) {
    return clampZone(Math.floor((cv + HALF_WIDTH) / ZONE_WIDTH));
  }
  return clampZone(-Math.floor((-cv + HALF_WIDTH) / ZONE_WIDTH));
}

export function zoneWithHysteresis(cv: number, current: number): number {
  if (current < OFFSET_MIN || current > OFFSET_MAX) {
    return zoneFor(cv);
  }
  if (Math.abs(cv - ZONE_WIDTH * current) <= STAY_WIDTH) {
    return current;
  }
  return zoneFor(cv);
}

export function patternIndexFor(base: number, offset: number): number {
  const wanted = base + offset;
  if (wanted < 0) return 0;
  if (wanted > PATTERN_COUNT - 1) return PATTERN_COUNT - 1;
  return wanted;
}

export function effectiveLengthFor(base: number, offset: number): number {
  const wanted = base + offset;
  if (wanted < MIN_LENGTH) return MIN_LENGTH;
  if (wanted > MAX_LENGTH) return MAX_LENGTH;
  return wanted;
}

export function readStepFor(localStep: number, offset: number, effectiveLength: number): number {
  if (effectiveLength <= 0) return 0;
  let wanted = (localStep + offset) % effectiveLength;
  if (wanted < 0) wanted += effectiveLength;
  return wanted === 0 ? 0 : wanted;
}
