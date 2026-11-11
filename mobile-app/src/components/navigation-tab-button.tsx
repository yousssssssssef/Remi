import { useEffect, type ComponentRef, type Ref } from 'react';
import { Pressable, View } from 'react-native';
import type { TabTriggerSlotProps } from 'expo-router/ui';
import Animated, { ReduceMotion, useAnimatedStyle, useReducedMotion, useSharedValue, withSpring } from 'react-native-reanimated';
import { AppText } from '@/components/ui/core';
import { Icon, type IconName } from '@/components/ui/icon';
import { useRemyTheme } from '@/theme/provider';

// Retarget the live spring: rapid taps never queue animations or delay routing.
const selectionSpring = { duration: 150, dampingRatio: 1, reduceMotion: ReduceMotion.System };
export function TabButton({ children, isFocused, icon, ref, onPressIn, onPressOut, ...props }: TabTriggerSlotProps & { icon: IconName }) {
  const { colors } = useRemyTheme();
  const reduced = useReducedMotion();
  const selection = useSharedValue(isFocused ? 1 : 0);
  const press = useSharedValue(1);
  useEffect(() => {
    selection.set(withSpring(isFocused ? 1 : 0, selectionSpring));
  }, [isFocused, selection]);
  const highlightStyle = useAnimatedStyle(() => ({
    opacity: selection.get(),
    transform: [{ scale: reduced ? 1 : 0.94 + selection.get() * 0.06 }],
  }));
  const contentStyle = useAnimatedStyle(() => ({ transform: [{ scale: press.get() }] }));
  return <Pressable {...props} ref={ref as Ref<ComponentRef<typeof Pressable>>}
    accessibilityRole="tab" accessibilityState={{ selected: !!isFocused }}
    onPressIn={event => { press.set(withSpring(reduced ? 1 : 0.97, selectionSpring)); onPressIn?.(event); }}
    onPressOut={event => { press.set(withSpring(1, selectionSpring)); onPressOut?.(event); }}
    style={({ pressed }) => ({ flex: 1, minHeight: 52, borderRadius: 26, backgroundColor: pressed ? colors.pressedSurface : 'transparent' })}>
    <Animated.View pointerEvents="none" style={[{ position: 'absolute', inset: 0, borderRadius: 26, backgroundColor: colors.primarySoft }, highlightStyle]} />
    <Animated.View pointerEvents="none" style={[{ flex: 1, padding: 4, gap: 2, alignItems: 'center', justifyContent: 'center' }, contentStyle]}>
      <View style={{ height: 26, alignItems: 'center', justifyContent: 'center' }}>
        <Icon name={icon} size={22} color={isFocused ? colors.primary : colors.textSecondary} />
      </View>
      <AppText variant="caption" style={{ fontSize: 11, lineHeight: 15, fontWeight: '600', color: isFocused ? colors.primary : colors.textSecondary }}>{children}</AppText>
    </Animated.View>
  </Pressable>;
}
