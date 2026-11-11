import { Redirect, router, usePathname } from 'expo-router';
import { useAuth } from '@/providers/auth-provider';
import { useOnboarding } from '@/providers/onboarding-provider';
import { Tabs, TabList, TabTrigger, TabSlot } from 'expo-router/ui';
import { Pressable, View } from 'react-native';
import { useState } from 'react';
import { SafeAreaView, useSafeAreaInsets } from 'react-native-safe-area-context';
import { BottomNavigationInsetProvider } from '@/components/ui/core';
import { Icon } from '@/components/ui/icon';
import { useRemyTheme } from '@/theme/provider';
import { useLanguage } from '@/providers/language-provider';
import { TabButton } from '@/components/navigation-tab-button';
export default function TabLayout() {
  const { colors } = useRemyTheme(); const { language } = useLanguage(); const ar = language === 'ar';
  const { user } = useAuth(); const onboarding = useOnboarding();
  const home = usePathname() === '/'; const insets = useSafeAreaInsets();
  const [barHeight, setBarHeight] = useState(66);
  // First signed-in run on this device goes to onboarding (plans and lists need an account). A read error just shows the app.
  if (user && onboarding.status === 'ready' && !onboarding.record) return <Redirect href="/onboarding" />;
  return <SafeAreaView edges={['top','left','right']} style={{ flex: 1, backgroundColor: colors.background }}><View pointerEvents="none" testID="tab-status-background" style={{ position: 'absolute', top: 0, left: 0, right: 0, height: insets.top, backgroundColor: home ? colors.hero : colors.background }} /><Tabs style={{ flex: 1 }}><BottomNavigationInsetProvider value={barHeight + insets.bottom + 20}><TabSlot style={{ flex: 1 }} /></BottomNavigationInsetProvider>
    <TabList onLayout={event => setBarHeight(event.nativeEvent.layout.height)} style={{ flexDirection: ar ? 'row-reverse' : 'row', position: 'absolute', bottom: insets.bottom + 10, left: 12, right: 12, padding: 6, borderRadius: 33, borderWidth: 1, borderColor: colors.divider, backgroundColor: colors.surfaceElevated, boxShadow: `0 4px 12px ${colors.shadow}` }}>
      <TabTrigger name="home" href="/" asChild><TabButton icon="home">{ar ? 'الرئيسية' : 'Home'}</TabButton></TabTrigger>
      <TabTrigger name="kitchen" href="/kitchen" asChild><TabButton icon="kitchen">{ar ? 'المطبخ' : 'Kitchen'}</TabButton></TabTrigger>
      <View style={{ flex: 1, alignItems: 'center', justifyContent: 'center' }}><Pressable accessibilityRole="button" accessibilityLabel={ar ? 'إضافة سريعة' : 'Quick add'} onPress={() => router.push('/quick-add')}
        style={({ pressed }) => ({ width: 50, height: 50, borderRadius: 25, alignItems: 'center', justifyContent: 'center', backgroundColor: colors.accent, opacity: pressed ? 0.85 : 1, boxShadow: `0 6px 16px ${colors.shadow}` })}>
        <Icon name="add" size={28} color={colors.onAccent} />
      </Pressable></View>
      <TabTrigger name="nutrition" href="/nutrition" asChild><TabButton icon="nutrition">{ar ? 'التغذية' : 'Nutrition'}</TabButton></TabTrigger>
      <TabTrigger name="more" href="/more" asChild><TabButton icon="more">{ar ? 'المزيد' : 'More'}</TabButton></TabTrigger>
    </TabList>
  </Tabs></SafeAreaView>;
}
