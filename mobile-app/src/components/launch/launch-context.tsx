import { createContext, useCallback, useContext, useMemo, useState, type PropsWithChildren } from 'react';
import Animated, { interpolate, useAnimatedRef, useAnimatedStyle, useSharedValue, type AnimatedRef, type SharedValue } from 'react-native-reanimated';
import { windowed } from './launch-layout';

interface LaunchState {
  /** Handoff clock, 0→1. Set to 1 when the welcome is done, so Home renders normally afterwards. */
  progress: SharedValue<number>;
  /** 1 from the frame the welcome's wordmark lands on Home's own. */
  swapped: SharedValue<number>;
  targets: { hero: AnimatedRef<Animated.View>; wordmark: AnimatedRef<Animated.View> };
  /** ms after launch when Home was laid out with its first data. */
  homeReadyAt: number | null;
  markHomeReady: () => void;
  startedAt: number;
  done: boolean;
  finish: () => void;
}

const LaunchContext = createContext<LaunchState | null>(null);

export function LaunchProvider({ children }: PropsWithChildren) {
  const progress = useSharedValue(0), swapped = useSharedValue(0);
  const hero = useAnimatedRef<Animated.View>(), wordmark = useAnimatedRef<Animated.View>();
  const [startedAt] = useState(() => Date.now());
  const [homeReadyAt, setHomeReadyAt] = useState<number | null>(null);
  const [done, setDone] = useState(false);
  const markHomeReady = useCallback(() => setHomeReadyAt(at => at ?? Date.now() - startedAt), [startedAt]);
  const finish = useCallback(() => { progress.set(1); swapped.set(1); setDone(true); }, [progress, swapped]);
  const value = useMemo(() => ({ progress, swapped, targets: { hero, wordmark }, homeReadyAt, markHomeReady, startedAt, done, finish }),
    [progress, swapped, hero, wordmark, homeReadyAt, markHomeReady, startedAt, done, finish]);
  return <LaunchContext.Provider value={value}>{children}</LaunchContext.Provider>;
}

export function useLaunch() {
  const launch = useContext(LaunchContext);
  if (!launch) throw new Error('useLaunch must be used inside LaunchProvider');
  return launch;
}

/** Home's own wordmark stays hidden until the welcome's flying copy lands on it. */
export function useLaunchHidden() {
  const { swapped } = useLaunch();
  return useAnimatedStyle(() => ({ opacity: swapped.get() }));
}

/** A Home element that fades and rises in during the handoff window [from, to] of the 0→1 clock. */
export function useLaunchReveal(from: number, to: number, rise: number) {
  const { progress } = useLaunch();
  return useAnimatedStyle(() => {
    const q = windowed(progress.get(), from, to);
    return { opacity: q, transform: [{ translateY: interpolate(1 - (1 - q) ** 3, [0, 1], [rise, 0]) }] };
  });
}
