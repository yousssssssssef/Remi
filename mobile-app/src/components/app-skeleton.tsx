import { palette } from '@/theme/tokens';
import { Brand } from './brand';
import { ScrollView, View, type ColorValue, type DimensionValue } from 'react-native';
import { SafeAreaView, useSafeAreaInsets } from 'react-native-safe-area-context';
import { StatusBar } from 'expo-status-bar';
import * as SplashScreen from 'expo-splash-screen';
import Animated, { useReducedMotion } from 'react-native-reanimated';
import { useRemyTheme } from '@/theme/provider';
import { useLanguage } from '@/providers/language-provider';

const Pulse = Animated.createAnimatedComponent(View);
export function AppSkeleton({ navigation = true }: { navigation?: boolean }) {
  const { colors, mode } = useRemyTheme(); const { language } = useLanguage(); const reduced = useReducedMotion();
  const rtl = language === 'ar'; const insets = useSafeAreaInsets();
  const block = (width: DimensionValue, height: number, color: ColorValue = colors.border) => <View style={{ width, height, borderRadius: 8, backgroundColor: color }} />;
  const body = <View accessible testID={navigation ? 'app-startup-skeleton' : 'home-loading-skeleton'} accessibilityRole="progressbar" accessibilityLabel={rtl ? 'جارٍ تحميل Remi' : 'Loading Remi'} accessibilityState={{ busy: true }} style={{ flex: 1, backgroundColor: colors.background }} onLayout={navigation ? () => SplashScreen.hide() : undefined}>
    <StatusBar style={mode === 'dark' ? 'light' : 'dark'} />
    <ScrollView scrollEnabled={false} contentContainerStyle={{ padding: 20, gap: 20, direction: rtl ? 'rtl' : 'ltr' }} accessibilityElementsHidden importantForAccessibility="no-hide-descendants">
      <View style={{ backgroundColor: colors.hero, marginHorizontal: -20, marginTop: -20, paddingHorizontal: 20, paddingTop: 8, paddingBottom: 16, gap: 8, borderBottomLeftRadius: 24, borderBottomRightRadius: 24 }}>
        <Pulse style={[{ gap: 8 }, !reduced && { animationName: { from: { opacity: 0.5 }, to: { opacity: 0.9 } }, animationDuration: 900, animationDirection: 'alternate', animationIterationCount: 'infinite', animationTimingFunction: 'ease-in-out' }]}>
          <View style={{ flexDirection: 'row', alignItems: 'center', gap: 16 }}><View style={{ flex: 1, gap: 4 }}><View style={{ alignSelf: 'flex-start', backgroundColor: palette['Lemon Yellow'], borderRadius: 6, paddingHorizontal: 4, paddingVertical: 2 }}><Brand kind="wordmark" height={24} color={palette['Cobalt Blue']} /></View>{block('85%', 26, colors.onHero)}{block('65%', 16, colors.onHero)}</View>{block(44, 44, colors.onHero)}</View>
          {block('100%', 48, colors.onHero)}
        </Pulse>
      </View>
      <Pulse style={[{ gap: 24 }, !reduced && { animationName: { from: { opacity: 0.5 }, to: { opacity: 0.9 } }, animationDuration: 900, animationDirection: 'alternate', animationIterationCount: 'infinite', animationTimingFunction: 'ease-in-out' }]}>
        {block('65%', 26)}
        <View style={{ backgroundColor: colors.surface, borderRadius: 18, overflow: 'hidden' }}>{block('100%', 160)}<View style={{ padding: 20, gap: 12 }}>{block('85%', 22)}{block('50%', 14)}</View></View>
      </Pulse>
    </ScrollView>
    {navigation && <View accessibilityElementsHidden importantForAccessibility="no-hide-descendants" style={{ flexDirection: 'row', alignItems: 'center', justifyContent: 'space-around', padding: 12, gap: 16, backgroundColor: colors.background }}>
      {[0, 1, 2, 3, 4].map(tab => <View key={tab} style={{ alignItems: 'center', gap: 8 }}>{block(tab === 2 ? 52 : 24, tab === 2 ? 52 : 24)}{tab !== 2 && block(36, 10)}</View>)}
    </View>}
  </View>;
  return navigation ? <SafeAreaView style={{ flex: 1, backgroundColor: colors.background }}><View pointerEvents="none" testID="startup-status-background" style={{ position: 'absolute', top: 0, left: 0, right: 0, height: insets.top, backgroundColor: colors.hero }} />{body}</SafeAreaView> : body;
}
