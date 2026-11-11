const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const ts = require('typescript');

test('cooking timer pauses, resumes from remaining time and emits one settings-controlled completion cue', () => {
  let now = 0, tick, cursor = 0, effects = [], cleanup;
  const state = [], events = [], cues = [], endAt = { current: 0 };
  const element = (type, props) => ({ type, props });
  const mocks = {
    react: { useState(initial) { const i = cursor++; if (!(i in state)) state[i] = initial; return [state[i], value => { state[i] = value; }]; }, useRef: () => endAt, useEffect: fn => effects.push(fn) },
    'react/jsx-runtime': { jsx: element, jsxs: element },
    'react-native': { Pressable: 'Pressable', View: 'View' },
    '@/components/ui/core': { AppText: 'AppText', uiStyles: { row: {} } },
    '@/components/ui/icon': { Icon: 'Icon' },
    '@/theme/provider': { useRemyTheme: () => ({ colors: {} }) },
    '@/providers/language-provider': { useLanguage: () => ({ language: 'en' }) },
    '@/theme/tokens': { radii: { pill: 999 } },
    '@/services/feedback': { feedback: { select: () => cues.push('select'), timerDone: () => cues.push('done') } },
  };
  const exported = {};
  const source = ts.transpileModule(fs.readFileSync('src/components/step-timer.tsx', 'utf8'), { compilerOptions: { module: ts.ModuleKind.CommonJS, jsx: ts.JsxEmit.ReactJSX } }).outputText;
  new Function('require', 'exports', 'Date', 'setInterval', 'clearInterval', source)(id => { assert(mocks[id], id); return mocks[id]; }, exported, { now: () => now }, callback => { tick = callback; return 1; }, () => { tick = undefined; });
  const render = () => {
    cleanup?.(); cursor = 0; effects = [];
    const tree = exported.StepTimer({ minutes: 1, onEvent: event => events.push(event) });
    for (const effect of effects) cleanup = effect();
    return tree.props.children[0];
  };
  let button = render(); assert.match(button.props.accessibilityLabel, /Start timer/);
  button.props.onPress(); button = render(); now = 10000; tick(); button = render(); assert.match(button.props.accessibilityLabel, /0:50/);
  button.props.onPress(); button = render(); assert.match(button.props.accessibilityLabel, /Resume/); assert.equal(tick, undefined);
  now = 60000; button.props.onPress(); button = render(); now = 109000; tick(); button = render(); assert.match(button.props.accessibilityLabel, /0:01/);
  now = 110000; tick(); button = render(); assert.match(button.props.accessibilityLabel, /Time's up/); assert.equal(tick, undefined);
  assert.deepEqual(events, ['start', 'pause', 'start', 'finish']); assert.equal(cues.filter(c => c === 'done').length, 1);
});
