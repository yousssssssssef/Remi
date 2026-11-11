import { View } from 'react-native';
import Svg, { Circle, Path, Rect } from 'react-native-svg';
import Animated, { cubicBezier, useReducedMotion } from 'react-native-reanimated';
import { Brand } from './brand';
import { palette } from '@/theme/tokens';

export type CookingMood = 'happy' | 'focused' | 'wow' | 'wink' | 'cheer';

/** Keep Remi's real chef-robot silhouette; only its screen-face changes expression. */
export function CookingMascot({ mood = 'happy', size = 76, reaction = 0 }: { mood?: CookingMood; size?: number; reaction?: number }) {
  const reduced = useReducedMotion();
  const ink = palette['Cobalt Blue'], ground = palette['Lemon Yellow'];
  const width = size * 600 / 585;
  return <View accessibilityElementsHidden importantForAccessibility="no-hide-descendants" style={{ width: width + 12, height: size + 12, alignItems: 'center', justifyContent: 'center', borderRadius: 28, backgroundColor: ground }}>
    <Animated.View key={reaction} style={[{ width, height: size }, reaction > 0 && !reduced && {
      animationName: { '0%': { transform: [{ scale: 1 }, { rotate: '0deg' }] }, '40%': { transform: [{ scale: 1.08 }, { rotate: mood === 'cheer' ? '-6deg' : '3deg' }] }, '100%': { transform: [{ scale: 1 }, { rotate: '0deg' }] } },
      animationDuration: 320, animationTimingFunction: cubicBezier(0.23, 1, 0.32, 1),
    }]}>
      <Brand kind="mascot" height={size} color={ink} />
      <Svg width={width} height={size} viewBox="0 0 600 585" style={{ position: 'absolute', top: 0, left: 0 }}>
        <Rect x={264} y={208} width={150} height={143} fill={ground} />
        {mood === 'focused' ? <Path d="M282 264h28 M370 264h28 M317 319h47" stroke={ink} strokeWidth={10} strokeLinecap="round" /> : <>
          {mood === 'cheer' ? <Path d="M280 267q15-25 30 0 M369 267q15-25 30 0" stroke={ink} strokeWidth={9} fill="none" strokeLinecap="round" /> : <>
            <Circle cx={296} cy={265} r={10} fill={ink} />
            {mood === 'wink' ? <Path d="M371 264h27" stroke={ink} strokeWidth={10} strokeLinecap="round" /> : <Circle cx={384} cy={265} r={10} fill={ink} />}
          </>}
          {mood === 'wow' ? <Circle cx={340} cy={315} r={16} stroke={ink} strokeWidth={9} fill="none" /> : <Path d="M307 310q33 36 66 0" stroke={ink} strokeWidth={10} fill="none" strokeLinecap="round" />}
        </>}
      </Svg>
    </Animated.View>
  </View>;
}

export function CookingConfetti() {
  const reduced = useReducedMotion();
  if (reduced) return null;
  return <View pointerEvents="none" accessibilityElementsHidden style={{ position: 'absolute', top: 45, left: 0, right: 0, height: 160 }}>
    {Array.from({ length: 12 }, (_, i) => <Animated.View key={i} style={{ position: 'absolute', left: `${8 + i * 7}%`, top: 70 + i % 3 * 16, width: 7, height: 12, borderRadius: 2,
      backgroundColor: [palette['Cobalt Blue'], palette['Hot Pink'], palette['Lemon Yellow']][i % 3], opacity: 0,
      animationName: { '0%': { opacity: 1, transform: [{ translateY: 0 }, { rotate: '0deg' }] }, '100%': { opacity: 0, transform: [{ translateY: -80 - i % 3 * 20 }, { rotate: `${i % 2 ? 120 : -120}deg` }] } },
      animationDuration: 800, animationDelay: i * 25, animationTimingFunction: cubicBezier(0.23, 1, 0.32, 1) }} />)}
  </View>;
}
