import { Image } from 'expo-image';
import * as SplashScreen from 'expo-splash-screen';
import { useEffect, useRef, useState } from 'react';
import { Pressable, StyleSheet, useWindowDimensions } from 'react-native';
import Animated, { Easing, FadeIn, measure, useAnimatedReaction, useAnimatedStyle, useReducedMotion, useSharedValue, withRepeat, withTiming, type SharedValue } from 'react-native-reanimated';
import { scheduleOnRN, scheduleOnUI } from 'react-native-worklets';
import { AppText } from '@/components/ui/core';
import { useAuth } from '@/providers/auth-provider';
import { useLanguage } from '@/providers/language-provider';
import { feedback } from '@/services/feedback';
import { useRemyFonts } from '@/theme/fonts';
import { useRemyTheme } from '@/theme/provider';
import { palette } from '@/theme/tokens';
import { useLaunch } from './launch-context';
import { arc, ART_FALLBACK_MS, CAPTION_MS, FOOD_SIZE, FOODS, GIVE_UP_MS, GREETING, HANDOFF_MS, handoffStart, keyframes, NO_HOME_GRACE_MS, REDUCED_HANDOFF_MS, restingLayout, TRAIL, WAVE_ANGLES, windowed, WORD_HEIGHT } from './launch-layout';

const EASE_OUT = Easing.bezierFn(0.23, 1, 0.32, 1);
const MOVE = Easing.bezierFn(0.77, 0, 0.175, 1);

const rig = {
  light: { body: require('../../../assets/launch/light-body.png'), glove: require('../../../assets/launch/light-glove.png'), eyes: require('../../../assets/launch/light-eyes.png'), steam: require('../../../assets/launch/light-steam.png') },
  dark: { body: require('../../../assets/launch/dark-body.png'), glove: require('../../../assets/launch/dark-glove.png'), eyes: require('../../../assets/launch/dark-eyes.png'), steam: require('../../../assets/launch/dark-steam.png') },
};
const foodArt = { basil: require('../../../assets/launch/food-basil.png'), carrot: require('../../../assets/launch/food-carrot.png'), tomato: require('../../../assets/launch/food-tomato.png'), lemon: require('../../../assets/launch/food-lemon.png') };
const brandWordmark = require('../../../assets/brand/wordmark.png');

type Geometry = { heroBottom: number | null; word: { x: number; y: number; width: number; height: number } | null; fade: boolean };

/**
 * The cold-start welcome. The native splash is plain hero colour, so nothing jumps: Remi rises in, the name
 * appears, he waves while ingredients fly out with trailing particles, then the lemon panel lifts into Home's
 * hero and the wordmark flies to its place.
 */
export function LaunchOverlay() {
  const launch = useLaunch();
  return launch.done ? null : <LaunchScene />;
}

function LaunchScene() {
  const { progress, swapped, targets, homeReadyAt, startedAt, finish } = useLaunch();
  const { colors, mode, ready: themeReady } = useRemyTheme();
  const { language, ready: languageReady, error: languageError } = useLanguage();
  const fonts = useRemyFonts(), auth = useAuth();
  const reduced = useReducedMotion();
  const { width, height } = useWindowDimensions();
  const rest = restingLayout(width, height);
  const geo = useSharedValue<Geometry>({ heroBottom: null, word: null, fade: true });
  const intro = useSharedValue(0), steam = useSharedValue(0);
  const [passThrough, setPassThrough] = useState(false);
  const [caption, setCaption] = useState(false);
  const [introAt, setIntroAt] = useState<number | null>(null);
  const [loaded, setLoaded] = useState(0);
  const begun = useRef(false);

  // Start once Remi and the name are decoded (or after a short fallback): fade the splash out, run the greeting.
  useEffect(() => {
    if (introAt !== null) return;
    const start = () => { SplashScreen.hideAsync().catch(() => undefined); setIntroAt(at => at ?? Date.now()); };
    if (loaded >= 2) { const frame = requestAnimationFrame(start); return () => cancelAnimationFrame(frame); }
    const fallback = setTimeout(start, ART_FALLBACK_MS);
    return () => clearTimeout(fallback);
  }, [loaded, introAt]);
  useEffect(() => {
    if (introAt === null) return;
    if (reduced) { intro.set(GREETING.end); return; }
    intro.set(withTiming(GREETING.end, { duration: GREETING.end * 1000, easing: Easing.linear }));
    steam.set(withRepeat(withTiming(1, { duration: 1400, easing: Easing.inOut(Easing.sin) }), -1, true));
  }, [introAt, reduced, intro, steam]);

  // The handoff: measure Home's hero and wordmark on the UI thread, then run one clock that every layer reads.
  const begin = () => {
    if (begun.current) return;
    begun.current = true;
    const { hero, wordmark } = targets;
    const duration = reduced ? REDUCED_HANDOFF_MS : HANDOFF_MS;
    scheduleOnUI(() => {
      'worklet';
      const h = measure(hero), w = measure(wordmark);
      geo.set({ heroBottom: h ? h.pageY + h.height : null, word: w ? { x: w.pageX, y: w.pageY, width: w.width, height: w.height } : null, fade: reduced || !h });
      progress.set(withTiming(1, { duration, easing: Easing.linear }, done => {
        'worklet';
        if (!done) return;
        swapped.set(1);
        scheduleOnRN(finish);
      }));
    });
  };

  // When to begin: Home ready and the greeting done; or no Home on screen (login, onboarding); or a give-up cap.
  const gateReady = themeReady && fonts.ready && (languageReady || languageError);
  useEffect(() => {
    if (introAt === null || begun.current) return;
    const ready = homeReadyAt === null ? null : introAt + (handoffStart(startedAt + homeReadyAt - introAt) ?? 0);
    const noHome = gateReady && auth.ready ? Math.max(Date.now(), introAt + GREETING.end * 1000) + NO_HOME_GRACE_MS : Infinity;
    const at = Math.min(ready ?? Infinity, noHome, startedAt + GIVE_UP_MS);
    const timer = setTimeout(begin, Math.max(0, at - Date.now()));
    return () => clearTimeout(timer);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [introAt, homeReadyAt, gateReady, auth.ready]);

  useEffect(() => {
    const timer = setTimeout(() => { if (!begun.current) setCaption(true); }, CAPTION_MS);
    return () => clearTimeout(timer);
  }, []);

  // Home is touchable as soon as the lifting panel has cleared its content.
  useAnimatedReaction(() => progress.get() > 0.8, (clear, before) => { if (clear && !before) scheduleOnRN(setPassThrough, true); });

  // How far the panel (and everything riding on it) has lifted.
  const lift = (g: Geometry, p: number) => { 'worklet'; return g.fade || g.heroBottom === null ? 0 : MOVE(windowed(p, 0, 0.83)) * (g.heroBottom - height); };

  const panel = useAnimatedStyle(() => {
    const g = geo.get(), p = progress.get();
    return { opacity: g.fade || g.heroBottom === null ? 1 - p : 1 - windowed(p, 0.72, 0.95), transform: [{ translateY: lift(g, p) }] };
  });

  // Remi rises in, then rides up with the panel and fades as it lifts.
  const mascot = useAnimatedStyle(() => {
    const t = intro.get(), g = geo.get(), p = progress.get();
    const r = 1 - EASE_OUT(windowed(t, GREETING.rise[0], GREETING.rise[1]));
    const leave = g.fade ? 1 - p : 1 - windowed(p, 0.05, 0.5);
    return { opacity: Math.min(1, t / 0.3) * leave, transform: [{ translateY: 24 * r + lift(g, p) }, { scale: 1 - 0.06 * r }] };
  });

  // The name appears, then flies to Home's wordmark tag.
  const word = useAnimatedStyle(() => {
    const t = intro.get(), g = geo.get(), p = progress.get();
    const inQ = EASE_OUT(windowed(t, GREETING.word[0], GREETING.word[1]));
    if (g.fade || !g.word) return { opacity: inQ * (1 - p), transform: [{ translateX: 0 }, { translateY: 12 * (1 - inQ) }, { scale: 1 }] };
    const q = MOVE(windowed(p, 0, 0.76));
    const sx = rest.wordmark.x + rest.wordmark.width / 2, sy = rest.wordmark.y + rest.wordmark.height / 2;
    const ex = g.word.x + g.word.width / 2, ey = g.word.y + g.word.height / 2;
    return { opacity: inQ, transform: [{ translateX: arc(q, sx, ex, ex) - sx }, { translateY: arc(q, sy, sy, ey) - sy + 12 * (1 - inQ) }, { scale: 1 + q * (g.word.height / WORD_HEIGHT - 1) }] };
  });
  // Over the last stretch the flying wordmark becomes Home's cobalt one (identical in light mode).
  const landing = useAnimatedStyle(() => ({ opacity: geo.get().fade ? 0 : windowed(progress.get(), 0.75, 0.95) }));
  const flying = useAnimatedStyle(() => ({ opacity: 1 - (geo.get().fade ? 0 : windowed(progress.get(), 0.75, 0.95)) }));

  const glove = useAnimatedStyle(() => ({ transform: [{ rotate: `${keyframes(intro.get(), GREETING.wave, WAVE_ANGLES)}deg` }] }));
  const eyes = useAnimatedStyle(() => {
    const b = windowed(intro.get(), GREETING.blink[0], GREETING.blink[1]);
    return { transform: [{ scaleY: b > 0 && b < 1 ? 1 - 0.92 * (1 - Math.abs(2 * b - 1)) : 1 }] };
  });
  const steamStyle = useAnimatedStyle(() => ({ transform: [{ translateX: steam.get() * 2 - 1 }, { translateY: 1 - steam.get() * 3 }] }));

  const art = rig[mode];
  const onArt = () => setLoaded(n => n + 1);
  return <Pressable style={StyleSheet.absoluteFill} pointerEvents={passThrough ? 'none' : 'auto'}
    onPress={() => { if (homeReadyAt !== null && !begun.current) { feedback.select(); begin(); } }}
    accessible accessibilityRole="progressbar" accessibilityLabel={language === 'ar' ? 'جارٍ تحميل Remi' : 'Loading Remi'} accessibilityState={{ busy: true }} accessibilityElementsHidden={passThrough}>
    <Animated.View style={[{ position: 'absolute', left: 0, right: 0, top: 0, height, backgroundColor: colors.hero, borderBottomLeftRadius: 24, borderBottomRightRadius: 24 }, panel]}>
      {FOODS.map((food, i) => <Food key={food.key} food={food} index={i} center={rest.center} intro={intro} progress={progress} tint={colors.logo} />)}
      {caption && !begun.current && <Animated.View entering={FadeIn.duration(300)} style={{ position: 'absolute', left: 20, right: 20, top: rest.wordmark.y + rest.wordmark.height + 28 }}>
        <AppText variant="caption" style={{ textAlign: 'center', color: colors.onHero }}>{language === 'ar' ? 'نجهّز مطبخك…' : 'Getting your kitchen ready…'}</AppText>
      </Animated.View>}
    </Animated.View>
    <Animated.View pointerEvents="none" style={[{ position: 'absolute', left: rest.mascot.x, top: rest.mascot.y, width: rest.mascot.width, height: rest.mascot.height }, mascot]}>
      <Animated.View style={[StyleSheet.absoluteFill, steamStyle]}><Image source={art.steam} style={StyleSheet.absoluteFill} /></Animated.View>
      <Image source={art.body} onLoad={onArt} style={StyleSheet.absoluteFill} />
      <Animated.View style={[StyleSheet.absoluteFill, { transformOrigin: '56% 45.5%' }, eyes]}><Image source={art.eyes} style={StyleSheet.absoluteFill} /></Animated.View>
      <Animated.View style={[StyleSheet.absoluteFill, { transformOrigin: '86.5% 62.74%' }, glove]}><Image source={art.glove} style={StyleSheet.absoluteFill} /></Animated.View>
    </Animated.View>
    <Animated.View pointerEvents="none" style={[{ position: 'absolute', left: rest.wordmark.x, top: rest.wordmark.y, width: rest.wordmark.width, height: rest.wordmark.height }, word]}>
      <Animated.View style={[StyleSheet.absoluteFill, flying]}><Image source={brandWordmark} tintColor={colors.logo} onLoad={onArt} style={StyleSheet.absoluteFill} /></Animated.View>
      <Animated.View style={[StyleSheet.absoluteFill, landing]}><Image source={brandWordmark} tintColor={palette['Cobalt Blue']} style={StyleSheet.absoluteFill} /></Animated.View>
    </Animated.View>
  </Pressable>;
}

/** One ingredient flies out from behind Remi along an arc with three trailing particles, then settles. */
function Food({ food, index, center, intro, progress, tint }: { food: (typeof FOODS)[number]; index: number; center: { x: number; y: number }; intro: SharedValue<number>; progress: SharedValue<number>; tint: string }) {
  const start = GREETING.foodStart + index * GREETING.foodStagger;
  const raw = (t: number) => { 'worklet'; return windowed(t, start, start + GREETING.foodFlight); };
  // Offsets from the landing spot: the flight starts behind Remi and bows upward.
  const at = (v: number) => { 'worklet'; return { x: arc(v, -food.dx, -food.dx * 0.35, 0), y: arc(v, -food.dy, -food.dy - 90, 0) }; };
  const style = useAnimatedStyle(() => {
    const q = raw(intro.get()), v = EASE_OUT(q), pos = at(v);
    const leave = 1 - windowed(progress.get(), 0.1, 0.55);
    return { opacity: Math.min(1, q * 6) * leave, transform: [{ translateX: pos.x }, { translateY: pos.y }, { rotate: `${food.turn * v}deg` }, { scale: 0.94 + 0.06 * v }] };
  });
  const left = center.x + food.dx, top = center.y + food.dy;
  return <>
    {TRAIL.map((dot, j) => <Particle key={j} left={left} top={top} size={dot.size} peak={dot.opacity} lag={0.045 * (j + 1)} raw={raw} at={at} intro={intro} tint={tint} />)}
    <Animated.View pointerEvents="none" style={[{ position: 'absolute', left: left - FOOD_SIZE / 2, top: top - FOOD_SIZE / 2, width: FOOD_SIZE, height: FOOD_SIZE * 153 / 144 }, style]}>
      <Image source={foodArt[food.key]} tintColor={tint} style={StyleSheet.absoluteFill} />
    </Animated.View>
  </>;
}

/** A trailing particle: follows its ingredient's path slightly behind it and is gone before the landing. */
function Particle({ left, top, size, peak, lag, raw, at, intro, tint }: { left: number; top: number; size: number; peak: number; lag: number; raw: (t: number) => number; at: (v: number) => { x: number; y: number }; intro: SharedValue<number>; tint: string }) {
  const style = useAnimatedStyle(() => {
    const q = raw(intro.get()), pos = at(EASE_OUT(Math.max(0, q - lag)));
    return { opacity: Math.min(1, q / 0.12) * Math.max(0, Math.min(1, (1 - q) / 0.28)) * peak, transform: [{ translateX: pos.x }, { translateY: pos.y }] };
  });
  return <Animated.View pointerEvents="none" style={[{ position: 'absolute', left: left - size / 2, top: top - size / 2, width: size, height: size, borderRadius: size / 2, backgroundColor: tint }, style]} />;
}
