import { useCookingJourney } from '@/hooks/use-cooking-journey';
import { cookingCareer } from '@/services/cooking-journal';
import { recipeContent, recipeContentDirection } from '@/i18n/recipe-content';
import { recipeName } from '@/i18n/recipe-names';
import { CookingMascot, CookingConfetti, type CookingMood } from '@/components/cooking-mascot';
import { Icon } from '@/components/ui/icon';
import { scaleMealAmount } from '@/domain/cooked-it';
import { useEffect, useRef, useState } from 'react';
import { router, useLocalSearchParams } from 'expo-router';
import { Pressable, ScrollView, View, useWindowDimensions, type ScrollViewInstance } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useReducedMotion } from 'react-native-reanimated';
import { useKeepAwake } from 'expo-keep-awake';
import { AppText, Button, Card, EmptyState, PageHeader, Screen, uiStyles } from '@/components/ui/core';
import { StepTimer } from '@/components/step-timer';
import { useBackendRecipes } from '@/providers/backend-recipes-provider';
import { useLanguage } from '@/providers/language-provider';
import { useRemyTheme } from '@/theme/provider';
import { cookingMessages } from '@/i18n/cooking';
import { backendRecipeMessages } from '@/i18n/backend-recipes';
import { recipeFoodName, recipeIngredientStatus, recipeNumber, recipeQuantity } from '@/utils/backend-recipe-view';
import { advanceCooking, selectCookingStep, cookingPoints, recordCookingTimer, completeCookingTimer, initialCookingSession } from '@/utils/cooking-session';
import { radii, spacing } from '@/theme/tokens';
import { feedback } from '@/services/feedback';

// Mob-style cook mode: one step per page, segmented progress, ingredients one tap away, screen kept on.
export function BackendCookingScreen() {
  const { id, servings } = useLocalSearchParams<{ id: string; servings?: string }>();
  const state = useBackendRecipes(), { owner, ensureDetail } = state;
  const { language } = useLanguage(), { colors } = useRemyTheme();
  const { width } = useWindowDimensions(); const reduced = useReducedMotion();
  const pager = useRef<ScrollViewInstance>(null); const [view, setView] = useState<'method' | 'ingredients'>('method');
  useKeepAwake();
  const t = cookingMessages[language], r = backendRecipeMessages[language];
  const rtl = language === 'ar', dir = rtl ? 'rtl' : 'ltr';
  const detail = state.detail?.id === id ? state.detail : null;
  const status = state.status === 'signed-out' ? 'signed-out' : detail?.status ?? (state.status === 'error' ? 'error' : 'loading');
  const recipe = status === 'ready' ? detail?.recipe : undefined;
  const { session, update, restoring, restoreError, history, saveWarning, retry: retryProgress, continueUnsaved } = useCookingJourney(id, recipe?.steps.length);
  const career = cookingCareer(history, session.runId);
  const hasCareer = !!session.runId && Object.hasOwn(history, session.runId);
  const progressReady = useRef(false);
  const count = servings && /^\d+$/.test(servings) && Number(servings) >= 1 && Number(servings) <= 20 ? servings : String(recipe?.servings ?? 1);
  const [reaction, setReaction] = useState<{ mood: CookingMood; message: string; points: number; serial: number }>({ mood: 'happy', message: '', points: 0, serial: 0 });
  const previousProgress = useRef({ completed: session.completed.length, finished: session.finished });
  const steps = [...(recipe?.steps ?? [])].sort((a, b) => a.stepNumber - b.stepNumber);
  const current = Math.min(session.current, Math.max(0, steps.length - 1));
  useEffect(() => {
    if (restoring) { progressReady.current = false; return; }
    if (!progressReady.current) { progressReady.current = true; previousProgress.current = { completed: session.completed.length, finished: session.finished }; return; }
    const previous = previousProgress.current;
    if (session.finished && !previous.finished) feedback.cookingFinish();
    else if (session.completed.length > previous.completed) {
      feedback.cookingStep();
      setReaction(old => ({ mood: session.completed.length % 2 ? 'wink' : 'cheer', message: rtl ? 'خطوة رائعة! كمّل على راحتك.' : 'Nice work. One step at a time.', points: 10, serial: old.serial + 1 }));
    }
    previousProgress.current = { completed: session.completed.length, finished: session.finished };
  }, [session.completed.length, session.finished, rtl, restoring]);
  // Keep the pager on the session's step (buttons, progress taps, restored sessions). Declared before the load effect.
  useEffect(() => { pager.current?.scrollTo({ x: current * width, animated: !reduced }); }, [current, width, reduced, view]);
  useEffect(() => { if (owner) void ensureDetail(id); }, [owner, ensureDetail, id]);
  const review = () => router.replace({ pathname: '/recipes/[id]', params: { id } });
  const browse = () => router.replace({ pathname: '/feature', params: { feature: 'recipes' } });
  const retry = () => { if (!owner) void state.refresh(); else void state.loadDetail(id); };
  const go = (index: number) => { feedback.select(); update(session => selectCookingStep(session, index, steps.length)); };
  const duration = (minutes: number) => `${recipeNumber(minutes, language)} ${r.minutes}`;
  const exit = <Button label={t.exit} variant="text" icon="back" rtl={rtl} onPress={review} />;

  if (!recipe || !steps.length) return <SafeAreaView style={{ flex: 1, backgroundColor: colors.background }}><Screen rtl={rtl} contentGap={spacing.lg}>
    <PageHeader title={t.title} back={exit} />
    {status === 'signed-out' ? <EmptyState title={r.cookingSignIn} description={r.signInBody} action={r.signIn} onPress={() => router.push('/login')} />
      : status === 'error' ? <Card><AppText accessibilityRole="alert" dir={dir}>{r.error}</AppText><Button label={r.retry} onPress={retry} /></Card>
      : !recipe ? <AppText dir={dir} accessibilityLiveRegion="polite">{r.loading}</AppText>
      : <EmptyState title={t.missing} description={t.missingBody} action={t.choose} onPress={browse} />}
  </Screen></SafeAreaView>;

  if (restoring) return <SafeAreaView style={{ flex: 1, backgroundColor: colors.background }}><Screen rtl={rtl}>
    <PageHeader title={t.title} back={exit} />
    <AppText accessibilityLiveRegion="polite">{restoreError ? (rtl ? 'تعذّر استعادة تقدم الطهي على هذا الجهاز.' : 'Could not restore cooking progress on this device.') : (rtl ? 'جارٍ استعادة تقدمك…' : 'Restoring your cooking progress…')}</AppText>
    {restoreError && <><Button label={r.retry} onPress={retryProgress} /><Button label={rtl ? 'المتابعة دون حفظ' : 'Continue without saving'} variant="text" onPress={continueUnsaved} /></>}
  </Screen></SafeAreaView>;
  const progressNotice = saveWarning ? <View style={{ gap: 6 }}><AppText accessibilityRole="alert">{rtl ? 'لم يُحفظ التقدم على هذا الجهاز. يمكنك إعادة المحاولة.' : 'Progress was not saved on this device. You can retry.'}</AppText><Button label={r.retry} variant="text" onPress={retryProgress} /></View> : null;
  if (session.finished) return <SafeAreaView style={{ flex: 1, backgroundColor: colors.background }}><Screen rtl={rtl}>
    <View style={{ alignItems: 'center', gap: spacing.lg, paddingVertical: 24 }}>
      <CookingConfetti /><CookingMascot mood="cheer" size={170} reaction={1} />
      <AppText variant="title" accessibilityRole="header" style={{ textAlign: 'center' }}>{rtl ? 'أحسنت يا شيف!' : 'You did it, chef!'}</AppText>
      <AppText variant="heading" style={{ textAlign: 'center' }}>{recipeName(recipe.name, language)}</AppText>
      <AppText muted style={{ textAlign: 'center' }}>{rtl ? 'اكتملت كل الخطوات. استمتع بما حضرت!' : 'Every step complete. Enjoy what you made!'}</AppText>
    </View>
    <Card style={{ gap: spacing.md, backgroundColor: colors.hero, boxShadow: 'none' }}>
      <View style={uiStyles.row}><Icon name="award" size={28} color={colors.onHero} /><AppText variant="heading" style={[uiStyles.grow, { color: colors.onHero }]}>{rtl ? 'إنجاز هذه الوجبة' : 'This meal’s achievement'}</AppText></View>
      <AppText variant="figure" style={{ color: colors.onHero }}>{cookingPoints(session)} {rtl ? 'نقطة' : 'XP'}</AppText>
      <AppText style={{ color: colors.onHero }}>{rtl ? `الخطوات المكتملة: ${recipeNumber(session.completed.length, language)} · ٥٠ نقطة للإكمال` : `${session.completed.length} ${session.completed.length === 1 ? 'step' : 'steps'} completed · 50 completion bonus`}</AppText>
      {!!session.timerSteps?.length && <AppText style={{ color: colors.onHero }}>{rtl ? `رفيق المؤقت · خطوات استُخدم فيها المؤقت: ${recipeNumber(session.timerSteps.length, language)}` : `Timer teammate · ${session.timerSteps.length} timed ${session.timerSteps.length === 1 ? 'step' : 'steps'}`}</AppText>}
    </Card>
    {session.startedAt && session.finishedAt && <AppText muted>{rtl ? 'مدة الجلسة: ' : 'Session elapsed: '}{Math.floor((session.finishedAt - session.startedAt) / 60000)}:{String(Math.floor((session.finishedAt - session.startedAt) / 1000) % 60).padStart(2, '0')}</AppText>}
    {hasCareer ? <Card style={{ gap: 12 }}>
      <AppText variant="heading">{rtl ? 'رحلتك في الطهي' : 'Your cooking journey'}</AppText><AppText variant="caption" muted>{rtl ? 'محفوظة على هذا الجهاز' : 'Saved on this device'}</AppText>
      <AppText>{rtl ? `جلسات مكتملة: ${recipeNumber(career.cooks, language)} · إجمالي النقاط: ${recipeNumber(career.xp, language)}` : `${career.cooks} completed cooks · ${career.xp} total XP`}</AppText>
      {career.cooks < 10 && <AppText muted>{rtl ? `الإنجاز التالي: ${career.cooks < 5 ? 5 : 10} جلسات · ${career.cooks}/${career.cooks < 5 ? 5 : 10}` : `Next milestone: ${career.cooks < 5 ? 5 : 10} cooks · ${career.cooks}/${career.cooks < 5 ? 5 : 10}`}</AppText>}
      <View style={uiStyles.wrap}>{[
        [career.first, rtl ? 'أول طبق' : 'First dish'], [career.five, rtl ? 'خمسة أطباق' : 'Five cooks'], [career.ten, rtl ? 'عشرة أطباق' : 'Ten cooks'],
        [career.newRecipe, rtl ? 'وصفة جديدة' : 'New recipe'], [career.timer, rtl ? 'خبير المؤقت' : 'Timer master'],
      ].filter(([earned]) => earned).map(([, label]) => <View key={String(label)} style={[uiStyles.row, { gap: 6, padding: 10, borderRadius: 18, backgroundColor: colors.primarySoft }]}><Icon name="award" size={18} color={colors.primary} /><AppText variant="label">{String(label)}</AppText></View>)}</View>
    </Card> : !saveWarning && <AppText accessibilityLiveRegion="polite" muted>{rtl ? 'جارٍ حفظ إنجازك…' : 'Saving your achievement…'}</AppText>}
    {progressNotice}
    <AppText muted>{rtl ? 'راجع الحصص والمكونات قبل تأكيد التسجيل.' : 'Review servings and ingredients before confirming.'}</AppText>
    <Button label={rtl ? 'تسجيل هذه الوجبة' : 'Log this meal'} onPress={() => router.push({ pathname: '/cooked-it', params: { id, servings: count } })} />
    <Button label={rtl ? 'العودة للرئيسية' : 'Back to Home'} variant="secondary" onPress={() => router.dismissTo('/')} />
    <Button label={rtl ? 'اطبخها مرة أخرى' : 'Cook it again'} variant="text" onPress={() => { update(() => initialCookingSession); setReaction({ mood: 'happy', message: '', points: 0, serial: 0 }); }} />
  </Screen></SafeAreaView>;

  const last = current === steps.length - 1;
  return <SafeAreaView style={{ flex: 1, backgroundColor: colors.background }}><View style={{ flex: 1, direction: dir }}>
    <View style={{ paddingHorizontal: 20, paddingTop: spacing.sm, gap: spacing.md }}>
      <View style={uiStyles.row}>{exit}<AppText variant="caption" muted numberOfLines={1} dir={dir} style={[uiStyles.grow, { textAlign: rtl ? 'left' : 'right' }]}>{recipeName(recipe.name, language)} · {count} {rtl ? 'حصص' : 'servings'}</AppText></View>
      <View style={[uiStyles.row, { gap: 12, padding: 10, borderRadius: 24, backgroundColor: colors.hero }]}>
        <CookingMascot size={56} mood={reaction.mood} reaction={reaction.serial} />
        <View style={[uiStyles.grow, { gap: 3 }]}><AppText variant="label" accessibilityLiveRegion="polite" style={{ color: colors.onHero }}>{reaction.message || (rtl ? 'أنا معك، خطوة بخطوة.' : 'I’m with you, step by step.')}</AppText>
          <AppText variant="caption" style={{ color: colors.onHeroMuted }}>{reaction.points ? (rtl ? '+١٠ نقاط لهذه الخطوة · ' : '+10 step XP · ') : ''}{rtl ? `نقاط هذه الجلسة: ${recipeNumber(cookingPoints(session), language)}` : `${cookingPoints(session)} XP this cook`}</AppText></View>
      </View>
      <ScrollView horizontal showsHorizontalScrollIndicator={false}><View accessibilityRole="progressbar" accessibilityLabel={t.progress} accessibilityValue={{ min: 0, max: steps.length, now: session.completed.length }} style={{ flexDirection: 'row', gap: 4 }}>
        {steps.map((step, index) => <Pressable key={step.id} testID="cook-progress-segment" accessibilityRole="button" accessibilityLabel={`${t.step} ${recipeNumber(index + 1, language)} · ${session.completed.includes(index) ? t.completed : t.remaining}`}
          accessibilityState={{ selected: index === current }} hitSlop={{ top: 14, bottom: 14 }} onPress={() => { setView('method'); go(index); }}
          style={{ width: 44, height: 44, borderRadius: 22, alignItems: 'center', justifyContent: 'center', borderWidth: 2, borderColor: index === current ? colors.primary : colors.divider, backgroundColor: session.completed.includes(index) ? colors.primary : colors.surface }}>{session.completed.includes(index) ? <Icon name="check" size={18} color={colors.onPrimary} /> : <AppText variant="label">{recipeNumber(index + 1, language)}</AppText>}</Pressable>)}
      </View></ScrollView>
      <View accessibilityRole="tablist" style={{ flexDirection: 'row', padding: 4, borderRadius: radii.pill, backgroundColor: colors.surfaceMuted }}>
        {(['method', 'ingredients'] as const).map(key => <Pressable key={key} accessibilityRole="tab" accessibilityState={{ selected: view === key }} onPress={() => setView(key)}
          style={{ flex: 1, minHeight: 44, borderRadius: radii.pill, alignItems: 'center', justifyContent: 'center', backgroundColor: view === key ? colors.surface : 'transparent' }}>
          <AppText variant="label" style={{ color: view === key ? colors.text : colors.textSecondary }}>{key === 'method' ? (rtl ? 'الطريقة' : 'Method') : r.ingredients}</AppText></Pressable>)}
      </View>
    </View>
    {progressNotice && <View style={{ paddingHorizontal: 20 }}>{progressNotice}</View>}
    {view === 'method' ? <ScrollView ref={pager} horizontal pagingEnabled showsHorizontalScrollIndicator={false} style={{ flex: 1, direction: 'ltr' }}
      onMomentumScrollEnd={event => { const index = Math.round(event.nativeEvent.contentOffset.x / width); if (index !== current) go(index); }}>
      {steps.map((step, index) => <ScrollView key={step.id} style={{ width }} contentContainerStyle={{ padding: 24, paddingTop: spacing.xxl, gap: spacing.lg, direction: dir }}>
        <AppText variant="label" dir={dir} style={{ color: colors.accentText }}>{t.step} {recipeNumber(index + 1, language)} {t.of} {recipeNumber(steps.length, language)}</AppText>
        <AppText variant="heading" dir={recipeContentDirection(step.instruction, language)} accessibilityLiveRegion={index === current ? 'polite' : 'none'} style={{ fontSize: 27, lineHeight: 36 }}>{recipeContent(step.instruction, language)}</AppText>
        {step.durationMin !== null && <><AppText muted dir={dir}>{duration(step.durationMin)}</AppText><StepTimer minutes={step.durationMin} onEvent={event => {
          if (event === 'start') update(s => recordCookingTimer(s, index));
          if (event === 'finish') update(s => completeCookingTimer(s, index));
          const message = event === 'start' ? (rtl ? `مؤقت الخطوة ${index + 1} يعمل. خذ وقتك.` : `Step ${index + 1} timer on. Take your time.`)
            : event === 'finish' ? (rtl ? `انتهى مؤقت الخطوة ${index + 1}. راجع طعامك.` : `Ding! Check step ${index + 1}.`)
            : event === 'pause' ? (rtl ? 'المؤقت متوقف مؤقتًا. استأنفه عندما تكون جاهزًا.' : 'Timer paused. Resume when you’re ready.')
            : (rtl ? 'المؤقت جاهز من جديد.' : 'Timer reset. Ready when you are.');
          setReaction(old => ({ mood: event === 'start' ? 'focused' : event === 'finish' ? 'wow' : 'happy', message, points: 0, serial: old.serial + 1 }));
        }} /></>}
      </ScrollView>)}
    </ScrollView> : <ScrollView style={{ flex: 1 }} contentContainerStyle={{ padding: 20, gap: spacing.md }}>
      {detail?.matchingLoading && <AppText dir={dir}>{r.matchingLoading}</AppText>}
      {detail?.matchingError && <><AppText dir={dir}>{r.matchingError}</AppText><Button label={r.retry} onPress={retry} /></>}
      {!detail?.matchingLoading && !detail?.matchingError && !detail?.suggestion && <AppText muted dir={dir}>{r.matchingUnavailable}</AppText>}
      <Card style={{ gap: 0, paddingVertical: 4 }}>{recipe.ingredients.map((ingredient, index) => {
        const status = recipeIngredientStatus(ingredient, detail?.suggestion, language);
        return <View key={ingredient.id} style={{ paddingVertical: 12, gap: 2, borderTopWidth: index ? 1 : 0, borderColor: colors.divider }}>
          <View style={uiStyles.row}><AppText variant="label" dir={dir} style={uiStyles.grow}>{recipeFoodName(ingredient, language)}</AppText>
            <AppText dir={dir} style={{ color: colors.textSecondary }}>{recipeQuantity(scaleMealAmount(ingredient.quantity, String(recipe.servings), count), ingredient.unit, language)}</AppText></View>
          {status && <AppText variant="caption" muted dir={dir}>{status}</AppText>}
        </View>;
      })}</Card>
    </ScrollView>}
    <View style={[uiStyles.row, { paddingHorizontal: 20, paddingVertical: spacing.md, borderTopWidth: 1, borderColor: colors.divider }]}>
      <Button label={t.previous} variant="secondary" disabled={current === 0} onPress={() => { setView('method'); go(current - 1); }} />
      <View style={uiStyles.grow}><Button label={last ? t.finish : t.next} onPress={() => { setView('method'); update(session => advanceCooking(session, steps.length)); }} /></View>
    </View>
  </View></SafeAreaView>;
}
