export const CV_MIN = -512;
export const CV_MAX = 512;

export const ZONE_WIDTH = 33;
export const ZONE_COUNT = 31;
export const OFFSET_MIN = -15;
export const OFFSET_MAX = 15;

export const HYSTERESIS = 8;
export const STAY_WIDTH = Math.floor(ZONE_WIDTH / 2) + HYSTERESIS;

export function zoneFor(cv: number): number {
  void cv;
  return 0;
}

export function zoneWithHysteresis(cv: number, current: number): number {
  void cv;
  void current;
  return 0;
}

export function effectiveLengthFor(base: number, offset: number): number {
  void offset;
  return base;
}
