const fs = require('node:fs'), test = require('node:test'), assert = require('node:assert/strict'), ts = require('typescript');
function load(file) {
  const exports = {};
  const code = ts.transpileModule(fs.readFileSync(file, 'utf8'), { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 } }).outputText;
  new Function('require', 'exports', code)(require, exports);
  return exports;
}
const L = load('src/components/launch/launch-layout.ts');
const near = (a, b) => Math.abs(a - b) < 1e-9;

test('the handoff waits for Home and for the greeting to finish, never longer', () => {
  assert.equal(L.handoffStart(null), null);
  assert.equal(L.handoffStart(120), L.GREETING.end * 1000);
  assert.equal(L.handoffStart(3100), 3100);
  assert.ok(L.GREETING.end <= 2 && L.GIVE_UP_MS <= 8000);
});

test('the native splash is plain hero colour, so the welcome draws its own entrance with nothing to mismatch', () => {
  const app = JSON.parse(fs.readFileSync('app.json', 'utf8'));
  const splash = app.expo.plugins.find(p => Array.isArray(p) && p[0] === 'expo-splash-screen')[1];
  // A transparent 3 px image: Expo only applies the background colour set when an image is configured.
  for (const file of [splash.image, splash.dark.image]) assert.equal(file, './assets/images/splash-blank.png');
  const png = fs.readFileSync('assets/images/splash-blank.png');
  assert.equal(png.readUInt32BE(16), 3);
  assert.equal(png[25], 6, 'RGBA, so it can be transparent');
  assert.equal(splash.backgroundColor, '#F8E45C');
  assert.equal(splash.dark.backgroundColor, '#1734BE');
});

test('the wave starts and ends exactly at rest, with no snap back', () => {
  const { wave } = L.GREETING;
  assert.equal(L.keyframes(0, wave, L.WAVE_ANGLES), 0);
  assert.equal(L.keyframes(wave[wave.length - 1], wave, L.WAVE_ANGLES), 0);
  assert.equal(L.keyframes(L.GREETING.end, wave, L.WAVE_ANGLES), 0);
  assert.equal(L.keyframes(wave[1], wave, L.WAVE_ANGLES), -13);
  // Continuous across every key: no jump between adjacent samples.
  let previous = 0;
  for (let t = 0; t <= L.GREETING.end; t += 0.005) { const v = L.keyframes(t, wave, L.WAVE_ANGLES); assert.ok(Math.abs(v - previous) < 1.5); previous = v; }
});

test('every ingredient lands before the greeting ends and stays on a small screen, beside Remi', () => {
  const r = L.restingLayout(375, 667);
  L.FOODS.forEach((food, i) => {
    const lands = L.GREETING.foodStart + i * L.GREETING.foodStagger + L.GREETING.foodFlight;
    assert.ok(lands <= L.GREETING.end, `${food.key} lands by the end of the greeting`);
    const x = r.center.x + food.dx;
    assert.ok(x - L.FOOD_SIZE / 2 >= 8 && x + L.FOOD_SIZE / 2 <= 367, `${food.key} stays inside a 375 pt screen`);
    assert.ok(Math.abs(food.dx) >= L.COMPOSITION.mascotWidth / 2 - 20, `${food.key} sits beside Remi`);
  });
});

test('three particles trail each flight, fading in size and opacity, and are gone at the landing', () => {
  assert.equal(L.TRAIL.length, 3);
  for (let j = 1; j < 3; j++) { assert.ok(L.TRAIL[j].size < L.TRAIL[j - 1].size); assert.ok(L.TRAIL[j].opacity < L.TRAIL[j - 1].opacity); }
  // The overlay's particle opacity: min(1, q / 0.12) × clamp((1 − q) / 0.28) × peak, zero at q = 1.
  const opacity = (q, peak) => Math.min(1, q / 0.12) * Math.max(0, Math.min(1, (1 - q) / 0.28)) * peak;
  assert.equal(opacity(1, L.TRAIL[0].opacity), 0);
  assert.equal(opacity(0, L.TRAIL[0].opacity), 0);
});

test('arc and windowed helpers are bounded', () => {
  assert.equal(L.arc(0, 1, 5, 9), 1);
  assert.equal(L.arc(1, 1, 5, 9), 9);
  assert.equal(L.windowed(0.2, 0.4, 0.8), 0);
  assert.ok(near(L.windowed(0.6, 0.4, 0.8), 0.5));
  assert.equal(L.windowed(1, 0.4, 0.8), 1);
});
