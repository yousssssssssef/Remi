// Geometry and timing for the launch welcome (src/components/launch/launch-overlay.tsx).
export const COMPOSITION = { width: 240, mascotWidth: 220, gap: 12, wordWidth: 180 } as const;
export const MASCOT_HEIGHT = COMPOSITION.mascotWidth * 585 / 600;
export const WORD_HEIGHT = COMPOSITION.wordWidth * 238 / 690;
export const COMPOSITION_HEIGHT = MASCOT_HEIGHT + COMPOSITION.gap + WORD_HEIGHT;

export interface Rect { x: number; y: number; width: number; height: number }

/** Where the mascot and wordmark rest, centred in the window. */
export function restingLayout(windowWidth: number, windowHeight: number) {
  const left = (windowWidth - COMPOSITION.width) / 2, top = (windowHeight - COMPOSITION_HEIGHT) / 2;
  return {
    mascot: { x: left + (COMPOSITION.width - COMPOSITION.mascotWidth) / 2, y: top, width: COMPOSITION.mascotWidth, height: MASCOT_HEIGHT },
    wordmark: { x: left + (COMPOSITION.width - COMPOSITION.wordWidth) / 2, y: top + MASCOT_HEIGHT + COMPOSITION.gap, width: COMPOSITION.wordWidth, height: WORD_HEIGHT },
    center: { x: windowWidth / 2, y: top + MASCOT_HEIGHT * 0.55 },
  };
}

/** Ingredients land around Remi, relative to the mascot's centre (points), clear of the face and the name. */
export const FOODS = [
  { key: 'basil', dx: -148, dy: -78, turn: -18 },
  { key: 'carrot', dx: 150, dy: -96, turn: 24 },
  { key: 'tomato', dx: -150, dy: 74, turn: -12 },
  { key: 'lemon', dx: 148, dy: 58, turn: 30 },
] as const;
export const FOOD_SIZE = 48;
/** Trailing particles per flight: size (pt) and peak opacity, nearest first. */
export const TRAIL = [{ size: 6, opacity: 0.5 }, { size: 4.5, opacity: 0.34 }, { size: 3, opacity: 0.2 }] as const;

// The greeting, in seconds on one clock that starts when the welcome's art is on screen.
export const GREETING = {
  rise: [0, 0.6], word: [0.35, 0.85], wave: [0.5, 0.72, 0.94, 1.16, 1.38, 1.6], blink: [1.3, 1.45],
  foodStart: 0.7, foodStagger: 0.14, foodFlight: 0.56, end: 1.8,
} as const;
export const WAVE_ANGLES = [0, -13, 10, -11, 6, 0] as const;
export const HANDOFF_MS = 720;
export const REDUCED_HANDOFF_MS = 200;
export const GIVE_UP_MS = 8000;
export const NO_HOME_GRACE_MS = 1500;
export const CAPTION_MS = 2500;
export const ART_FALLBACK_MS = 600;

/** When the handoff may start (ms after the greeting began): Home ready and the greeting finished. */
export function handoffStart(readyAt: number | null): number | null {
  return readyAt === null ? null : Math.max(readyAt, GREETING.end * 1000);
}

/** Point on a quadratic Bézier from a to b through control c. */
export function arc(t: number, a: number, c: number, b: number): number {
  'worklet';
  const u = 1 - t;
  return u * u * a + 2 * u * t * c + t * t * b;
}

/** 0→1 progress of the window [from, to], clamped. */
export function windowed(p: number, from: number, to: number): number {
  'worklet';
  return Math.min(1, Math.max(0, (p - from) / (to - from)));
}

/** Smooth keyframes: eases in and out between each pair of keys, holds the ends. */
export function keyframes(t: number, times: readonly number[], values: readonly number[]): number {
  'worklet';
  if (t <= times[0]) return values[0];
  for (let i = 1; i < times.length; i++) {
    if (t <= times[i]) {
      const q = (t - times[i - 1]) / (times[i] - times[i - 1]);
      const s = q * q * (3 - 2 * q);
      return values[i - 1] + (values[i] - values[i - 1]) * s;
    }
  }
  return values[values.length - 1];
}
