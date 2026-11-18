// Remi device: 3D build guide + live simulation.
// Units: millimetres. The TFT shows the real firmware UI (ui.cpp/gfx.cpp/synth.cpp compiled to remi.wasm).
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';

// ---------------------------------------------------------------- renderer / scene
const stage = document.getElementById('stage');
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
renderer.outputColorSpace = THREE.SRGBColorSpace;
renderer.toneMapping = THREE.ACESFilmicToneMapping;
renderer.toneMappingExposure = 1.05;
renderer.shadowMap.enabled = true;
renderer.shadowMap.type = THREE.PCFShadowMap;
stage.appendChild(renderer.domElement);

const scene = new THREE.Scene();
scene.background = new THREE.Color('#FFFDF4');
scene.environment = new THREE.PMREMGenerator(renderer).fromScene(new RoomEnvironment(), 0.04).texture;
const camera = new THREE.PerspectiveCamera(35, 1, 2, 4000);
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.maxPolarAngle = Math.PI * 0.49;
controls.minDistance = 40;
controls.maxDistance = 900;

scene.add(new THREE.HemisphereLight('#fffef5', '#d9cfa0', 0.9));
const sun = new THREE.DirectionalLight('#ffffff', 1.6);
sun.position.set(160, 320, 220);
sun.castShadow = true;
sun.shadow.mapSize.set(2048, 2048);
Object.assign(sun.shadow.camera, { left: -220, right: 220, top: 220, bottom: -220, near: 50, far: 900 });
sun.shadow.bias = -0.0004;
sun.shadow.radius = 4;
scene.add(sun);
const table = new THREE.Mesh(new THREE.CircleGeometry(900, 64), new THREE.MeshStandardMaterial({ color: '#F4EFDA', roughness: 0.95 }));
table.rotation.x = -Math.PI / 2;
table.receiveShadow = true;
scene.add(table);

// ---------------------------------------------------------------- materials
const M = (color, o = {}) => new THREE.MeshStandardMaterial({ color, roughness: 0.55, metalness: 0, ...o });
const mat = {
  pcbBlack: M('#1b1f26', { roughness: 0.7 }), pcbRed: M('#b0261f', { roughness: 0.6 }), pcbBlue: M('#1d4ea6'),
  pcbPurple: M('#4b2a8a'), pcbGreen: M('#1f5a3a'), pcbExp: M('#20262f', { roughness: 0.8 }),
  gold: M('#d4ad4f', { metalness: 1, roughness: 0.3 }), silver: M('#c9ced6', { metalness: 1, roughness: 0.25 }),
  steel: M('#9aa1aa', { metalness: 1, roughness: 0.35 }), plastic: M('#16181c', { roughness: 0.6 }),
  terminal: M('#2e78d6', { roughness: 0.5 }), green: M('#2f9a55', { roughness: 0.5 }), white: M('#fbfbf7', { roughness: 0.8 }),
  glass: M('#0b0d12', { roughness: 0.15, metalness: 0.2 }), lemon: M('#F8E45C', { roughness: 0.5 }),
  cobalt: M('#1734BE', { roughness: 0.45 }), cobaltSoft: M('#3b55d0', { roughness: 0.5 }), hat: M('#ffffff', { roughness: 0.9 }),
  breadboard: M('#f3f1ea', { roughness: 0.85 }), chip: M('#202226', { roughness: 0.4 }), led: M('#ff3b30', { emissive: '#ff3b30', emissiveIntensity: 0.6 }),
};

function box(w, h, d, material, r = 0) {
  const g = r > 0 ? new RoundedBoxGeometry(w, h, d, 3, r) : new THREE.BoxGeometry(w, h, d);
  const m = new THREE.Mesh(g, material);
  m.castShadow = m.receiveShadow = true;
  return m;
}
function cyl(rt, rb, h, material, seg = 32) {
  const m = new THREE.Mesh(new THREE.CylinderGeometry(rt, rb, h, seg), material);
  m.castShadow = m.receiveShadow = true;
  return m;
}
function at(obj, x, y, z) { obj.position.set(x, y, z); return obj; }

// A row of header pins. dir: unit vector the pins point to. Returns pin tip positions (local).
function headerRow(group, count, start, step, dir, length, withPlastic = true) {
  const tips = [];
  const d = new THREE.Vector3(...dir).normalize();
  const s = new THREE.Vector3(...step);
  for (let i = 0; i < count; i++) {
    const base = new THREE.Vector3(...start).addScaledVector(s, i);
    const pin = box(0.64, 0.64, length, mat.gold);
    pin.position.copy(base).addScaledVector(d, length / 2);
    pin.lookAt(pin.position.clone().add(d));
    group.add(pin);
    if (withPlastic) {
      const p = box(2.5, 2.5, 2.5, mat.plastic);
      p.position.copy(base).addScaledVector(d, 1.25);
      p.lookAt(p.position.clone().add(d));
      group.add(p);
    }
    tips.push(base.clone().addScaledVector(d, length));
  }
  return tips;
}

// ---------------------------------------------------------------- live screen (WebAssembly firmware)
const screenCanvas = document.createElement('canvas');
screenCanvas.width = 128; screenCanvas.height = 160;
const screenCtx = screenCanvas.getContext('2d');
const screenImage = screenCtx.createImageData(128, 160);
const screenTexture = new THREE.CanvasTexture(screenCanvas);
screenTexture.colorSpace = THREE.SRGBColorSpace;
screenTexture.magFilter = THREE.NearestFilter;
screenTexture.minFilter = THREE.LinearFilter;
const screenMat = new THREE.MeshBasicMaterial({ map: screenTexture, toneMapped: false });

let wasm = null;
const t0 = performance.now();
async function loadFirmware() {
  const stub = new Proxy({}, { get: () => () => 0 });
  const bytes = await (await fetch('remi.wasm')).arrayBuffer();
  const { instance } = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stub });
  wasm = instance.exports;
  wasm._initialize();
  wasm.remi_init(0);
  wasm.remi_sound(0);
}
function localSeconds() { return (Date.now() - new Date().getTimezoneOffset() * 60000) / 1000; }
function updateScreen() {
  if (!wasm) return;
  const ptr = wasm.remi_frame(Math.round(performance.now() - t0), localSeconds());
  const px = new Uint16Array(wasm.memory.buffer, ptr, 128 * 160);
  const d = screenImage.data;
  for (let i = 0, j = 0; i < px.length; i++, j += 4) {
    const c = px[i];
    d[j] = ((c >> 11) & 31) * 255 / 31; d[j + 1] = ((c >> 5) & 63) * 255 / 63; d[j + 2] = (c & 31) * 255 / 31; d[j + 3] = 255;
  }
  screenCtx.putImageData(screenImage, 0, 0);
  screenTexture.needsUpdate = true;
}
function sendInput(kind) {  // 0 left, 1 right, 2 press, 3 hold
  if (wasm) wasm.remi_input(kind);
  animateKnob(kind);
}

// Sound: pull samples from the firmware synth and schedule them in Web Audio.
let audio = null, nextAudioTime = 0;
function audioPump() {
  if (!audio || !wasm) return;
  while (nextAudioTime < audio.currentTime + 0.15) {
    const n = 1024, ptr = wasm.remi_audio(n);
    const pcm = new Int16Array(wasm.memory.buffer, ptr, n);
    const buf = audio.createBuffer(1, n, 22050);
    const ch = buf.getChannelData(0);
    for (let i = 0; i < n; i++) ch[i] = pcm[i] / 32768;
    const src = audio.createBufferSource();
    src.buffer = buf;
    src.connect(audio.destination);
    if (nextAudioTime < audio.currentTime) nextAudioTime = audio.currentTime + 0.03;
    src.start(nextAudioTime);
    nextAudioTime += n / 22050;
  }
}
setInterval(audioPump, 40);

// ---------------------------------------------------------------- parts
const pickables = [];  // meshes that select a part
const parts = {};      // id -> {group, info, pins, labelAt}
let PFX = '';           // each view builds its own copies of the parts: ids are 'view:part'
function registerPart(id, group, info, pins = {}) {
  const key = PFX + id;
  group.traverse((o) => { if (o.isMesh) { o.userData.part = key; pickables.push(o); } });
  parts[key] = { group, info, pins };
  return parts[key];
}

function makeDevKit() {
  const g = new THREE.Group();
  g.add(at(box(28.3, 1.6, 51.5, mat.pcbBlack, 0.6), 0, 0.8, 0));
  g.add(at(box(18, 0.8, 25.5, mat.pcbBlue), 0, 2, -13));                       // WROOM-32 module PCB
  g.add(at(box(15.8, 2.4, 17.6, mat.silver, 0.3), 0, 3.6, -9.6));              // RF shield
  g.add(at(box(14, 0.05, 5.5, M('#c9a24a', { metalness: 1, roughness: 0.4 })), 0, 2.45, -22.5));  // PCB antenna
  g.add(at(box(7.6, 2.8, 5.6, mat.silver, 0.4), 0, 3, 23.6));                  // micro-USB
  for (const x of [-8.5, 8.5]) { g.add(at(box(4.2, 1.6, 3, mat.silver), x, 2.4, 21)); g.add(at(cyl(0.9, 0.9, 1, mat.plastic, 12), x, 3.5, 21)); }
  g.add(at(box(6.5, 1.6, 3.5, mat.chip), -6, 2.4, 12));                         // AMS1117
  g.add(at(box(5, 1, 5, mat.chip), 6, 2.1, 9));                                 // USB-UART bridge
  g.add(at(box(1.2, 0.6, 0.8, mat.led), 9.5, 1.9, 4));
  const pins = {};
  const left = ['EN', 'VP', 'VN', 'D34', 'D35', 'D32', 'D33', 'D25', 'D26', 'D27', 'D14', 'D12', 'D13', 'GND', 'VIN'];
  const right = ['D23', 'D22', 'TX0', 'RX0', 'D21', 'D19', 'D18', 'D5', 'TX2', 'RX2', 'D4', 'D2', 'D15', 'GND2', '3V3'];
  const lt = headerRow(g, 15, [-12.7, 0, -17.78], [0, 0, 2.54], [0, -1, 0], 8.5);
  const rt = headerRow(g, 15, [12.7, 0, -17.78], [0, 0, 2.54], [0, -1, 0], 8.5);
  left.forEach((n, i) => (pins[n] = lt[i]));
  right.forEach((n, i) => (pins[n] = rt[i]));
  return registerPart('devkit', g, {
    name: 'ESP32 DevKit V1 (30-pin)', meta: 'ESP32-WROOM-32 · 51.5 × 28.3 mm · 3.3 V logic',
    rows: [['Role', 'Brain: screen, knob, Wi-Fi, sound'], ['Screen', 'SCK D18 · MOSI D23 · CS D5 · DC D4'], ['Encoder', 'CLK D32 · DT D33 · SW D25'],
      ['Amp (I2S)', 'BCLK D26 · LRC D27 · DIN D22'], ['Mic (planned)', 'SD D35, shares BCLK/LRC'], ['Power', 'USB 5 V → VIN; 3V3 regulator']],
  }, pins);
}

function makeExpansionBoard(devkit) {
  const g = new THREE.Group();
  const W = 62, D = 68;
  g.add(at(box(W, 1.6, D, mat.pcbExp, 1.2), 0, 0.8, 0));
  for (const x of [-12.7, 12.7]) g.add(at(box(2.6, 8.5, 38.5, mat.plastic), x, 1.6 + 4.25, 0));  // female headers
  const terminals = {};
  const names = Object.keys(devkit.pins);
  for (const [side, x] of [[-1, -W / 2 + 3.6], [1, W / 2 - 3.6]]) {
    for (let i = 0; i < 15; i++) {
      const z = -24.5 + i * 3.5;
      const b = box(7, 8.2, 3.4, mat.terminal, 0.3);
      g.add(at(b, x, 1.6 + 4.1, z));
      g.add(at(cyl(1.15, 1.15, 0.6, mat.steel, 12), x, 1.6 + 8.4, z));
      const pin = names[(side < 0 ? 0 : 15) + i];
      terminals[pin] = new THREE.Vector3(x + side * 3.4, 1.6 + 3.5, z);
    }
  }
  g.add(at(box(9, 10.5, 13.5, mat.plastic, 0.5), -16, 1.6 + 5.25, D / 2 - 7));   // DC barrel jack
  g.add(at(box(6.5, 1.6, 3.5, mat.chip), 4, 2.4, D / 2 - 5));                      // regulator
  g.add(at(box(1.2, 0.6, 0.8, mat.led), 10, 1.9, D / 2 - 4));
  devkit.group.position.set(0, 1.6 + 8.5 + 2.5, 0);  // pins sit inside the female headers
  g.add(devkit.group);
  // Screw terminals are where wires land on this board.
  const pins = {};
  for (const [k, v] of Object.entries(terminals)) pins[k] = v;
  return registerPart('expansion', g, {
    name: 'ESP32 expansion board (30-pin)', meta: 'Screw terminals for every DevKit pin · 5–12 V DC jack · generic, about 62 × 68 mm',
    rows: [['Why', 'Solid screw connections instead of loose jumpers'], ['Power in', 'USB on the DevKit, or the DC jack'], ['Check', 'Silkscreen order matches your board']],
  }, pins);
}

function makeTFT() {
  const g = new THREE.Group();  // stands upright, facing +z, origin at board centre
  g.add(at(box(34, 56.4, 1.6, mat.pcbRed, 0.8), 0, 0, 0));
  g.add(at(box(34, 45.8, 2.2, mat.glass, 0.4), 0, -3.6, 1.9));
  const active = new THREE.Mesh(new THREE.PlaneGeometry(28.03, 35.04), screenMat);
  active.position.set(0, -2.6, 3.02);
  g.add(active);
  g.add(at(box(26, 27, 2.2, mat.silver, 0.3), 0, -6, -1.9));  // SD slot on the back
  g.add(at(box(6.5, 1.6, 3.5, mat.chip), 11, 23, -1.6));         // AMS1117 regulator
  const names = ['VCC', 'GND', 'CS', 'RESET', 'A0', 'SDA', 'SCK', 'LED'];
  const tips = headerRow(g, 8, [-8.89, 25.6, -0.8], [2.54, 0, 0], [0, 0, -1], 8.5);
  const pins = {};
  names.forEach((n, i) => (pins[n] = tips[i]));
  return registerPart('tft', g, {
    name: '1.8" TFT, ST7735 128×160 (V1.1)', meta: '34 × 56.4 mm board · 28 × 35 mm active area · SPI',
    rows: [['VCC', '5 V (onboard AMS1117). 3.3 V only if J1 is bridged'], ['LED', 'Backlight → 3V3'], ['SCK / SDA', 'SPI clock / data (SDA = MOSI)'],
      ['A0', 'Data/command (DC)'], ['RESET', 'Tie to 3V3'], ['CS', 'Chip select'], ['Pins', 'Header order shown from the front: check your silkscreen']],
  }, pins);
}

let knobs = [];  // meshes that act as the knob (bench knob, finished-product dial)
function makeKY040(productKnob = false) {
  const g = new THREE.Group();  // lies flat, shaft +y
  g.add(at(box(19, 1.6, 26.5, mat.pcbBlack, 0.6), 0, 0.8, 0));
  g.add(at(box(12.4, 6.5, 13.4, mat.steel, 0.4), 0, 1.6 + 3.25, 3));
  g.add(at(cyl(3.5, 3.5, 7, mat.silver, 24), 0, 1.6 + 6.5 + 3.5, 3));
  const shaft = at(cyl(3, 3, 15, mat.silver, 24), 0, 1.6 + 6.5 + 7 + 7.5, 3);
  g.add(shaft);
  const knob = new THREE.Group();
  if (!productKnob) {
    knob.add(at(cyl(7.5, 7.8, 13, M('#26292f', { roughness: 0.5 }), 40), 0, 0, 0));
    knob.add(at(box(1, 1, 6, mat.white), 0, 6.6, 3));  // pointer line
    knob.position.set(0, 1.6 + 6.5 + 7 + 8, 3);
    g.add(knob);
  }
  for (const [x, z] of [[-5, -5.5], [5, -5.5]]) g.add(at(box(2, 1, 1.2, mat.chip), x, 2.1, z));  // pull-up resistors
  const names = ['GND', '+', 'SW', 'DT', 'CLK'];
  const tips = headerRow(g, 5, [-9.5, 1.6 + 1.3, -5.08 - 3], [0, 0, 2.54], [-1, 0, 0], 8.5);
  const pins = {};
  names.forEach((n, i) => (pins[n] = tips[i]));
  const part = registerPart(productKnob ? 'ky040p' : 'ky040', g, {
    name: 'KY-040 rotary encoder', meta: '20 clicks per turn · push switch · 26.5 × 19 mm board',
    rows: [['CLK', 'Channel A → D32'], ['DT', 'Channel B → D33'], ['SW', 'Push → D25 (internal pull-up)'], ['+', '3V3 only (its pull-ups set the logic level)'], ['GND', 'GND'],
      ['Feel', 'Turn = next/prev page · Press = select · Hold = back']],
  }, pins);
  part.knob = knob;
  if (!productKnob) knob.traverse((o) => { if (o.isMesh) knobs.push(o); });
  return part;
}

const loader = new GLTFLoader();
async function loadGLB(url) {
  // Hosts that won't serve .glb get the same bytes as .bin (GLTFLoader reads the binary header either way).
  const gltf = await loader.loadAsync(url).catch(async () => {
    // Hosts that won't serve .glb get the same bytes as a base64 JS module (models/<name>.glb.js).
    const b64 = (await import(`./${url}.js`)).default;
    const bytes = Uint8Array.from(atob(b64), (c) => c.charCodeAt(0));
    return loader.parseAsync(bytes.buffer, '');
  });
  gltf.scene.scale.setScalar(1000);  // metres -> mm
  gltf.scene.traverse((o) => { if (o.isMesh) { o.castShadow = o.receiveShadow = true; } });
  return gltf.scene;
}

async function makeAmp() {
  const g = new THREE.Group();  // flat, component side up; Adafruit CAD has the board in XY with Z up
  const model = await loadGLB('models/max98357a.glb');
  model.rotation.x = -Math.PI / 2;
  model.position.set(-8.9, 0, 9.55);
  g.add(model);
  model.traverse((o) => { if (o.isMesh && o.material?.color && o.material.color.r > 0.17 && o.material.color.r < 0.21) o.material = mat.pcbPurple; });
  const names = ['LRC', 'BCLK', 'DIN', 'GAIN', 'SD', 'GND', 'Vin'];
  const tips = headerRow(g, 7, [-7.62, 1.6, 8.0], [2.54, 0, 0], [0, 1, 0], 6.5);
  const pins = {};
  names.forEach((n, i) => (pins[n] = tips[i]));
  const term = at(box(7.2, 7.5, 7, mat.green, 0.4), 0, 1.6 + 3.75, -6.4);
  g.add(term);
  pins['SPK+'] = new THREE.Vector3(-1.75, 1.6 + 7.6, -6.4);
  pins['SPK-'] = new THREE.Vector3(1.75, 1.6 + 7.6, -6.4);
  return registerPart('amp', g, {
    name: 'MAX98357A I2S class-D amp', meta: '17.8 × 19.1 mm · 3.2 W into 4 Ω at 5 V · 3D model: Adafruit (MIT)',
    rows: [['BCLK', '→ D26'], ['LRC', '→ D27 (word select)'], ['DIN', '→ D22'], ['Vin', '5 V (DevKit VIN)'], ['GAIN', 'Open = 9 dB · GND = 12 dB'], ['SD', 'Open = mono mix of L+R'],
      ['Speaker', '+ and − only. Never wire a speaker lead to GND']],
  }, pins);
}

async function makeSpeaker() {
  const g = new THREE.Group();
  const model = await loadGLB('models/speaker-40mm.glb');
  model.rotation.x = Math.PI / 2;  // cone faces up on the bench
  model.position.y = 25;
  g.add(model);
  const pins = { '+': new THREE.Vector3(-4, 3, 17), '-': new THREE.Vector3(4, 3, 17) };
  g.add(at(box(3, 1, 4, mat.gold), -4, 2, 17)); g.add(at(box(3, 1, 4, mat.gold), 4, 2, 17));
  return registerPart('speaker', g, {
    name: 'Speaker, 40 mm 4–8 Ω', meta: '40 × 40 × 25 mm · 3 W · 3D model: Adafruit 3968 (MIT)',
    rows: [['Wiring', 'Amp + → speaker +, amp − → speaker −'], ['Mounting', 'Faces out through the back grille, sealed with foam for bass']],
  }, pins);
}

function makeINMP441() {
  const g = new THREE.Group();
  const pcb = new THREE.Mesh(new THREE.CylinderGeometry(7, 7, 1.2, 40), mat.pcbBlue);
  pcb.position.y = 0.6; pcb.castShadow = pcb.receiveShadow = true;
  g.add(pcb);
  g.add(at(cyl(0.6, 0.6, 0.05, mat.plastic, 16), 0, 1.23, 3.6));   // sound port
  g.add(at(box(4, 1, 3, mat.silver), 0, -0.5, 3.6));                   // MEMS mic underneath
  const names1 = ['L/R', 'WS', 'SCK'], names2 = ['SD', 'VDD', 'GND'];
  const t1 = headerRow(g, 3, [-2.54, 1.2, -1.2], [2.54, 0, 0], [0, 1, 0], 6, false);
  const t2 = headerRow(g, 3, [-2.54, 1.2, -3.74], [2.54, 0, 0], [0, 1, 0], 6, false);
  const pins = {};
  names1.forEach((n, i) => (pins[n] = t1[i]));
  names2.forEach((n, i) => (pins[n] = t2[i]));
  return registerPart('mic', g, {
    name: 'INMP441 I2S microphone (planned)', meta: '14 mm board · 24-bit I2S · not wired yet',
    rows: [['SCK', '→ D26 (shared with amp BCLK)'], ['WS', '→ D27 (shared with amp LRC)'], ['SD', '→ D35 (input-only pin, fine for data in)'], ['L/R', 'GND = left channel'], ['VDD', '3V3'],
      ['Plan', 'Push-to-talk on the Talk page; audio goes to the backend']],
  }, pins);
}

function makeESP32CAM() {
  const g = new THREE.Group();  // flat, shield up, antenna toward -z
  g.add(at(box(27, 1, 40.5, mat.pcbBlack, 0.5), 0, 0.5, 0));
  g.add(at(box(17.6, 2.2, 23, mat.silver, 0.3), 0, 2.1, -4));
  g.add(at(box(14, 0.05, 5, M('#c9a24a', { metalness: 1, roughness: 0.4 })), 0, 1.05, -17));
  g.add(at(box(8.5, 4.5, 8.5, mat.plastic, 0.5), 0, 3.3, 12));        // OV2640 camera
  g.add(at(cyl(2.8, 2.8, 2, mat.glass, 24), 0, 6.5, 12));
  g.add(at(box(3, 1, 2, M('#ffffff', { emissive: '#ffffff', emissiveIntensity: 0.3 })), -9, 1.5, 17));  // flash LED
  g.add(at(box(3.5, 1.4, 2.4, mat.silver), 9, 1.7, 17.5));             // RESET button
  const left = ['IO4', 'IO2', 'IO14', 'IO15', 'IO13', 'IO12', 'GND', '5V'];
  const right = ['GND3', 'U0T', 'U0R', 'VCC', 'GND2', 'IO0', 'IO16', '3V3'];
  const lt = headerRow(g, 8, [-11.43, 0, -8.89], [0, 0, 2.54], [0, -1, 0], 8.5);
  const rt = headerRow(g, 8, [11.43, 0, -8.89], [0, 0, 2.54], [0, -1, 0], 8.5);
  const pins = {};
  left.forEach((n, i) => (pins[n] = lt[i]));
  right.forEach((n, i) => (pins[n] = rt[i]));
  return registerPart('cam', g, {
    name: 'ESP32-CAM (AI Thinker)', meta: '27 × 40.5 mm · today\'s bench board',
    rows: [['Screen', 'SCK IO14 · MOSI IO13 · CS IO15 · DC IO2'], ['Speaker (amp)', 'BCLK U0T · LRC IO4 · DIN IO0 · 5V · GND'],
      ['Trade-offs', 'Serial logs move to the red LED (GPIO33); the flash LED glows while audio runs'], ['No knob', 'IO16 is the PSRAM select; IO12 is a boot pin; IO0 carries audio'],
      ['Upload', 'IO0 → GND jumper, TFT DC unplugged, then RESET'], ['Navigate', 'Serial Monitor keys: a / d / s / b']],
  }, pins);
}

function makeMB() {
  const g = new THREE.Group();
  g.add(at(box(27, 1.2, 40, mat.pcbBlack, 0.5), 0, 0.6, 0));
  for (const x of [-11.43, 11.43]) g.add(at(box(2.6, 8.5, 20.5, mat.plastic), x, 1.2 + 4.25, -2));
  g.add(at(box(7.6, 2.8, 5.6, mat.silver, 0.4), 0, 2.6, 18));
  g.add(at(box(5, 1, 5, mat.chip), 0, 1.7, 9));
  return registerPart('mb', g, {
    name: 'ESP32-CAM-MB programmer', meta: 'USB-serial (CH340) · micro-USB',
    rows: [['Today', 'Provides 5 V, GND and the serial upload lines to the CAM'], ['Upload', 'Its RTS/DTR auto-reset needs the CAM seated or U0T/U0R wired']],
  }, {});
}

function makeBreadboard() {
  const g = new THREE.Group();
  const c = document.createElement('canvas'); c.width = 660; c.height = 440;
  const x = c.getContext('2d');
  x.fillStyle = '#f3f1ea'; x.fillRect(0, 0, 660, 440);
  x.fillStyle = '#3a3a3a';
  for (let i = 0; i < 30; i++) for (let j = 0; j < 10; j++) x.fillRect(22 + i * 20.3, 70 + j * 20.3 + (j > 4 ? 30 : 0), 6, 6);
  x.fillStyle = '#d33'; x.fillRect(16, 20, 628, 3); x.fillStyle = '#33c'; x.fillRect(16, 418, 628, 3);
  const tex = new THREE.CanvasTexture(c); tex.colorSpace = THREE.SRGBColorSpace;
  const top = new THREE.MeshStandardMaterial({ map: tex, roughness: 0.85 });
  const b = new THREE.Mesh(new THREE.BoxGeometry(82.5, 9.5, 55), [mat.breadboard, mat.breadboard, top, mat.breadboard, mat.breadboard, mat.breadboard]);
  b.position.y = 4.75; b.castShadow = b.receiveShadow = true;
  g.add(b);
  return registerPart('breadboard', g, { name: 'Half breadboard', meta: '82.5 × 55 mm', rows: [['Today', 'Holds the CAM so its pins reach jumpers']] }, {});
}

// ---------------------------------------------------------------- wires
const WIRE_COLORS = {
  '5V': '#e53935', '3V3': '#fb8c00', GND: '#1d1d1f', SCK: '#fdd835', MOSI: '#43a047', CS: '#1e88e5', DC: '#8e24aa',
  CLK: '#eeeeee', DT: '#9e9e9e', SW: '#8d6e63', BCLK: '#ec407a', LRC: '#26c6da', DIN: '#9ccc65', MIC: '#00897b', SPK: '#c62828', UART: '#5c6bc0',
};
function wire(from, to, color, opts = {}) {
  const a = from.clone(), b = to.clone();
  const lift = opts.lift ?? Math.max(14, a.distanceTo(b) * 0.28);
  const mid = a.clone().lerp(b, 0.5); mid.y = Math.max(a.y, b.y) + lift;
  const curve = new THREE.CatmullRomCurve3([a, a.clone().add(new THREE.Vector3(0, 6, 0)), mid, b.clone().add(new THREE.Vector3(0, opts.endLift ?? 6, 0)), b], false, 'centripetal');
  const m = new THREE.Mesh(new THREE.TubeGeometry(curve, 48, opts.radius ?? 0.6, 8),
    new THREE.MeshStandardMaterial({ color, roughness: 0.45, transparent: !!opts.planned, opacity: opts.planned ? 0.45 : 1 }));
  m.castShadow = true;
  m.userData.wire = { group: opts.group, planned: !!opts.planned };
  return m;
}
function worldPin(part, pin) {
  const p = part.pins[pin];
  if (!p) throw new Error(`no pin ${pin} on ${part.info.name}`);
  part.group.updateWorldMatrix(true, true);
  return p.clone().applyMatrix4(part.group.matrixWorld);
}

// ---------------------------------------------------------------- views
const views = {};
const labelLayer = document.createElement('div');
labelLayer.style.cssText = 'position:absolute;inset:0;pointer-events:none;overflow:hidden';
stage.appendChild(labelLayer);

async function buildViews() {
  // ----- Today: ESP32-CAM on a breadboard + MB, TFT upright
  {
    PFX = 'today:';
    const root = new THREE.Group();
    const bb = makeBreadboard(); root.add(bb.group);
    const cam = makeESP32CAM(); cam.group.position.set(0, 9.5 + 8.5, 0); root.add(cam.group);
    const mb = makeMB(); mb.group.position.set(-62, 0, 6); root.add(mb.group);
    const tft = makeTFT(); tft.group.position.set(0, 28.2 + 6, -70); root.add(tft.group);
    const amp = await makeAmp(); amp.group.position.set(68, 0, 30); amp.group.rotation.y = -Math.PI / 2; root.add(amp.group);
    const spk = await makeSpeaker(); spk.group.position.set(92, 0, -28); root.add(spk.group);
    root.updateWorldMatrix(true, true);
    const wires = new THREE.Group();
    const conn = [
      [cam, '5V', tft, 'VCC', '5V', 'screen'], [cam, 'GND', tft, 'GND', 'GND', 'screen'], [cam, 'IO15', tft, 'CS', 'CS', 'screen'],
      [cam, '3V3', tft, 'RESET', '3V3', 'screen'], [cam, 'IO2', tft, 'A0', 'DC', 'screen'], [cam, 'IO13', tft, 'SDA', 'MOSI', 'screen'],
      [cam, 'IO14', tft, 'SCK', 'SCK', 'screen'], [cam, '3V3', tft, 'LED', '3V3', 'screen'],
      [cam, 'U0T', amp, 'BCLK', 'BCLK', 'audio'], [cam, 'IO4', amp, 'LRC', 'LRC', 'audio'], [cam, 'IO0', amp, 'DIN', 'DIN', 'audio'],
      [cam, '5V', amp, 'Vin', '5V', 'audio'], [cam, 'GND2', amp, 'GND', 'GND', 'audio'],
    ];
    for (const [pa, a, pb, b, c, grp] of conn) {
      const from = worldPin(pa, a); from.y = 9.6;  // jumper enters the breadboard row next to the pin
      wires.add(wire(from, worldPin(pb, b), WIRE_COLORS[c], { group: grp }));
    }
    const camPinsMB = [['5V', '5V'], ['GND', 'GND'], ['U0T', 'UART'], ['U0R', 'UART']];
    camPinsMB.forEach(([p, c], i) => {
      const from = worldPin(cam, p); from.y = 9.6;
      wires.add(wire(from, new THREE.Vector3(-62 + (i - 1.5) * 4, 10, -6), WIRE_COLORS[c], { group: 'power' }));
    });
    // upload jumper IO0 -> GND (shown during the upload step only)
    wires.add(wire(worldPin(amp, 'SPK+'), worldPin(spk, '+'), WIRE_COLORS.SPK, { group: 'audio', lift: 10 }));
    wires.add(wire(worldPin(amp, 'SPK-'), worldPin(spk, '-'), '#222', { group: 'audio', lift: 10 }));
    const jumper = wire(Object.assign(worldPin(cam, 'IO0'), { y: 9.6 }), Object.assign(worldPin(cam, 'GND2'), { y: 9.6 }), '#ffb300', { group: 'upload', lift: 6 });
    jumper.visible = false;
    wires.add(jumper);
    root.add(wires);
    views.today = {
      root, wires, prefix: 'today:', cam: { pos: [150, 175, 225], target: [8, 15, -15] }, labels: ['cam', 'mb', 'tft', 'amp', 'speaker'],
      steps: [
        { t: 'ESP32-CAM on the breadboard', d: 'Powered from the MB programmer: 5 V, GND and the U0T/U0R serial lines.', focus: 'cam', groups: ['power'] },
        { t: 'Screen wiring', d: 'SCK→IO14 · SDA→IO13 · CS→IO15 · A0→IO2 · VCC→5V · GND · RESET and LED→3V3.', focus: 'tft', groups: ['screen'] },
        { t: 'Amp and speaker', d: 'BCLK→U0T · LRC→IO4 · DIN→IO0 · Vin→5V · GND. Speaker on the amp + and −. Tape over the flash LED: it flickers with sound.', focus: 'amp', groups: ['audio'] },
        { t: 'Upload mode', d: 'Unplug USB, jumper IO0→GND (yellow; the amp's DIN wire can stay on IO0 too), unplug TFT A0, reconnect, press RESET, upload.', focus: 'cam', groups: ['upload'], show: [jumper] },
        { t: 'Run', d: 'Unplug, remove the jumper, plug A0 back in, press RESET. Serial Monitor keys: a / d / s / b, and c for the colour test.', focus: 'tft', groups: ['screen', 'power', 'audio'] },
      ],
    };
  }

  // ----- Full build on the bench: DevKit + expansion board + all modules
  {
    PFX = 'bench:';
    const root = new THREE.Group();
    const dev = makeDevKit();
    const exp = makeExpansionBoard(dev); root.add(exp.group);
    const tft = makeTFT(); tft.group.position.set(0, 28.2 + 8, -78); root.add(tft.group);
    const enc = makeKY040(); enc.group.position.set(78, 0, 18); root.add(enc.group);
    const amp = await makeAmp(); amp.group.position.set(-72, 0, -6); amp.group.rotation.y = Math.PI / 2; root.add(amp.group);
    const spk = await makeSpeaker(); spk.group.position.set(-82, 0, -58); root.add(spk.group);
    const mic = makeINMP441(); mic.group.position.set(-70, 0, 38); root.add(mic.group);
    root.updateWorldMatrix(true, true);
    const wires = new THREE.Group();
    const T = (pin) => worldPin(exp, pin);
    const links = [
      // screen
      ['VIN', tft, 'VCC', '5V', 'screen'], ['GND2', tft, 'GND', 'GND', 'screen'], ['D5', tft, 'CS', 'CS', 'screen'], ['3V3', tft, 'RESET', '3V3', 'screen'],
      ['D4', tft, 'A0', 'DC', 'screen'], ['D23', tft, 'SDA', 'MOSI', 'screen'], ['D18', tft, 'SCK', 'SCK', 'screen'], ['3V3', tft, 'LED', '3V3', 'screen'],
      // encoder
      ['GND2', enc, 'GND', 'GND', 'encoder'], ['3V3', enc, '+', '3V3', 'encoder'], ['D25', enc, 'SW', 'SW', 'encoder'], ['D33', enc, 'DT', 'DT', 'encoder'], ['D32', enc, 'CLK', 'CLK', 'encoder'],
      // amp
      ['VIN', amp, 'Vin', '5V', 'audio'], ['GND', amp, 'GND', 'GND', 'audio'], ['D26', amp, 'BCLK', 'BCLK', 'audio'], ['D27', amp, 'LRC', 'LRC', 'audio'], ['D22', amp, 'DIN', 'DIN', 'audio'],
      // microphone (planned)
      ['3V3', mic, 'VDD', '3V3', 'mic'], ['GND', mic, 'GND', 'GND', 'mic'], ['GND', mic, 'L/R', 'GND', 'mic'], ['D26', mic, 'SCK', 'BCLK', 'mic'], ['D27', mic, 'WS', 'LRC', 'mic'], ['D35', mic, 'SD', 'MIC', 'mic'],
    ];
    for (const [t, part, pin, c, grp] of links) wires.add(wire(T(t), worldPin(part, pin), WIRE_COLORS[c], { group: grp, planned: grp === 'mic' }));
    wires.add(wire(worldPin(amp, 'SPK+'), worldPin(spk, '+'), WIRE_COLORS.SPK, { group: 'audio', lift: 10 }));
    wires.add(wire(worldPin(amp, 'SPK-'), worldPin(spk, '-'), '#222', { group: 'audio', lift: 10 }));
    root.add(wires);
    views.bench = {
      root, wires, prefix: 'bench:', cam: { pos: [190, 250, 290], target: [0, 15, -10] }, labels: ['devkit', 'tft', 'ky040', 'amp', 'speaker', 'mic'],
      steps: [
        { t: 'DevKit on the expansion board', d: 'Seat the ESP32 DevKit in the female headers, USB toward you. All wires land on the screw terminals.', focus: 'expansion', groups: [] },
        { t: 'Screen (8 wires)', d: 'VCC→VIN 5 V · GND · CS→D5 · RESET→3V3 · A0→D4 · SDA→D23 · SCK→D18 · LED→3V3.', focus: 'tft', groups: ['screen'] },
        { t: 'Rotary encoder (5 wires)', d: '+→3V3 (not 5 V) · GND · SW→D25 · DT→D33 · CLK→D32.', focus: 'ky040', groups: ['encoder'] },
        { t: 'Amplifier and speaker', d: 'Vin→VIN 5 V · GND · BCLK→D26 · LRC→D27 · DIN→D22. Speaker on the amp + and − terminal.', focus: 'amp', groups: ['audio'] },
        { t: 'Microphone (planned)', d: 'VDD→3V3 · GND · L/R→GND · SCK→D26 · WS→D27 (shared with the amp) · SD→D35.', focus: 'mic', groups: ['mic'] },
        { t: 'Flash and power on', d: 'Board "ESP32 Dev Module", Partition "Huge APP". Turn the knob to move between pages.', focus: 'tft', groups: ['screen', 'encoder', 'audio'] },
      ],
    };
    views.bench.knob = enc;
  }

  // ----- Finished product (assembled + exploded share one model)
  {
    PFX = 'product:';
    const root = new THREE.Group();
    const product = makeProduct();
    root.add(product.group);
    const internals = await makeInternals();
    for (const it of internals) product.body.add(it.obj);
    views.product = {
      root, product, internals, prefix: 'product:', cam: { pos: [250, 230, 470], target: [0, 92, 0] }, labels: [],
      steps: [
        { t: 'Screen behind the window', d: 'TFT glass sits flush behind the front panel window, header up.', focus: 'tft' },
        { t: 'Knob = Remi\'s dial', d: 'Encoder shaft through the dial hole, nut on the front, cobalt knob on the shaft.', focus: 'ky040p' },
        { t: 'Microphone port', d: 'INMP441 sound port aligned with the small round hole, with a foam gasket.', focus: 'mic' },
        { t: 'Speaker on the back', d: '40 mm speaker fires through the back grille. The amp sits on the right wall.', focus: 'speaker' },
        { t: 'Brain on the floor', d: 'Expansion board + DevKit on standoffs; USB lines up with the port in the foot.', focus: 'expansion' },
        { t: 'Close up', d: 'Back panel on, chef-hat lid on top. Remi boots straight into his intro.', focus: null },
      ],
    };
    views.exploded = { ...views.product, exploded: true, cam: { pos: [380, 260, 420], target: [-20, 80, 0] } };
  }
}

// Mascot-shaped enclosure: lemon body, cobalt trim, dial knob, chef-hat lid.
function roundedRectShape(w, h, r, cx = 0, cy = 0) {
  const s = new THREE.Shape(), x = cx - w / 2, y = cy - h / 2;
  s.moveTo(x + r, y); s.lineTo(x + w - r, y); s.quadraticCurveTo(x + w, y, x + w, y + r); s.lineTo(x + w, y + h - r);
  s.quadraticCurveTo(x + w, y + h, x + w - r, y + h); s.lineTo(x + r, y + h); s.quadraticCurveTo(x, y + h, x, y + h - r);
  s.lineTo(x, y + r); s.quadraticCurveTo(x, y, x + r, y);
  return s;
}
function holePath(shape) { const p = new THREE.Path(); p.curves = shape.curves; return p; }
function circlePath(cx, cy, r) { const p = new THREE.Path(); p.absarc(cx, cy, r, 0, Math.PI * 2, true); return p; }
function extrude(shape, depth, material) {
  const m = new THREE.Mesh(new THREE.ExtrudeGeometry(shape, { depth, bevelEnabled: true, bevelSize: 0.6, bevelThickness: 0.6, bevelSegments: 2, curveSegments: 24 }), material);
  m.castShadow = m.receiveShadow = true;
  return m;
}

const BODY = { w: 90, h: 124, d: 80, r: 10, wall: 3, footH: 12 };
const SCREEN_Y = 24, DIAL_Y = -30, MIC = [30, -14];
function makeProduct() {
  const g = new THREE.Group();
  const body = new THREE.Group();
  body.position.y = BODY.footH + BODY.h / 2;
  g.add(body);
  const { w, h, d, r, wall } = BODY;
  // side walls: outer rounded rect minus inner, extruded through the depth
  const ring = roundedRectShape(w, h, r);
  ring.holes.push(holePath(roundedRectShape(w - 2 * wall, h - 2 * wall, r - wall)));
  const shell = extrude(ring, d - 2 * wall, mat.lemon);
  shell.position.z = -(d - 2 * wall) / 2;
  body.add(shell);
  // front panel with window, dial hole, mic hole and slot (the mascot's face layout)
  const front = roundedRectShape(w, h, r);
  front.holes.push(holePath(roundedRectShape(31, 38, 3, 0, SCREEN_Y)));
  front.holes.push(circlePath(0, DIAL_Y, 3.8));
  front.holes.push(circlePath(MIC[0], MIC[1], 1.6));
  front.holes.push(holePath(roundedRectShape(14, 6, 3, -28, -14)));
  const frontMesh = extrude(front, wall, mat.lemon);
  const frontGroup = new THREE.Group();
  frontGroup.add(frontMesh);
  // cobalt trim: screen bezel, outer outline, dial ring
  const bezel = roundedRectShape(44, 52, 6, 0, SCREEN_Y);
  bezel.holes.push(holePath(roundedRectShape(31, 38, 3, 0, SCREEN_Y)));
  frontGroup.add(at(extrude(bezel, 1.2, mat.cobalt), 0, 0, wall));
  const outline = roundedRectShape(w, h, r);
  outline.holes.push(holePath(roundedRectShape(w - 4, h - 4, r - 2)));
  frontGroup.add(at(extrude(outline, 1.2, mat.cobalt), 0, 0, wall));
  const dialRing = new THREE.Shape(); dialRing.absarc(0, DIAL_Y, 21, 0, Math.PI * 2, false); dialRing.holes.push(circlePath(0, DIAL_Y, 18.5));
  frontGroup.add(at(extrude(dialRing, 1.2, mat.cobalt), 0, 0, wall));
  const micRing = new THREE.Shape(); micRing.absarc(MIC[0], MIC[1], 5, 0, Math.PI * 2, false); micRing.holes.push(circlePath(MIC[0], MIC[1], 3.4));
  frontGroup.add(at(extrude(micRing, 1.2, mat.cobalt), 0, 0, wall));
  const slotRing = roundedRectShape(18, 9, 4.5, -28, -14); slotRing.holes.push(holePath(roundedRectShape(14, 6, 3, -28, -14)));
  frontGroup.add(at(extrude(slotRing, 1.2, mat.cobalt), 0, 0, wall));
  frontGroup.position.z = d / 2 - wall;
  body.add(frontGroup);
  // the dial knob (rotary encoder knob)
  const dial = new THREE.Group();
  dial.add(at(cyl(16.5, 17, 9, mat.cobalt, 64), 0, 0, 0));
  dial.add(at(cyl(11, 11, 9.4, mat.lemon, 64), 0, 0, 0));
  dial.add(at(cyl(8, 8, 9.8, mat.cobaltSoft, 48), 0, 0, 0));
  dial.add(at(box(1.6, 10, 5, mat.white), 0, 0.2, 13));
  dial.children.forEach((c) => (c.rotation.x = Math.PI / 2));
  dial.children[3].rotation.x = 0; dial.children[3].position.set(0, 13.2, 4.95);
  dial.position.set(0, DIAL_Y, d / 2 + 1.2 + 4.5);
  body.add(dial);
  dial.traverse((o) => { if (o.isMesh) knobs.push(o); });
  // back panel with speaker grille
  const back = roundedRectShape(w, h, r);
  for (let i = -3; i <= 3; i++) for (let j = -3; j <= 3; j++) if (i * i + j * j <= 10) back.holes.push(circlePath(i * 4.6, 18 + j * 4.6, 1.3));
  back.holes.push(holePath(roundedRectShape(14, 6, 2, 0, -50)));
  const backMesh = extrude(back, wall, mat.lemon);
  backMesh.position.z = -d / 2;
  body.add(backMesh);
  // chef-hat lid
  const hat = new THREE.Group();
  hat.add(at(cyl(30, 31, 20, mat.hat, 64), 0, 10, 0));
  for (const [x, y, z, s] of [[-17, 30, 0, 17], [0, 36, -4, 20], [17, 30, 0, 17], [0, 30, 12, 15], [-8, 27, -14, 14], [10, 27, -14, 14]]) {
    const puff = new THREE.Mesh(new THREE.SphereGeometry(s, 32, 20), mat.hat);
    puff.position.set(x, y, z); puff.castShadow = true; hat.add(puff);
  }
  for (let i = 0; i < 3; i++) hat.add(at(box(1.2, 14, 1, M('#e6e6e6')), -10 + i * 10, 11, 30.6));
  hat.position.y = h / 2;
  body.add(hat);
  // foot with the USB port opening
  const foot = box(48, BODY.footH, 40, mat.lemon, 4);
  foot.position.y = BODY.footH / 2;
  g.add(foot);
  const usb = box(10, 4, 1, mat.plastic, 1); usb.position.set(0, BODY.footH / 2, 20.2); g.add(usb);
  const footTrim = box(50, 1.6, 42, mat.cobalt, 0.8); footTrim.position.y = BODY.footH; g.add(footTrim);
  registerPart('enclosure', g, {
    name: 'Remi enclosure (concept)', meta: `${w} × ${h + BODY.footH} × ${d} mm body · PLA, lemon with cobalt trim`,
    rows: [['Front', 'Screen window, dial (rotary knob), mic port, slot'], ['Back', 'Speaker grille, service slot'], ['Top', 'Chef-hat lid, lifts off'], ['Foot', 'USB port opening'],
      ['Print', 'Front, shell, back, hat and foot as separate parts; trim in cobalt PLA']],
  });
  const product = { group: g, body, front: frontGroup, back: backMesh, hat, dial, shell };
  return product;
}

async function makeInternals() {
  const { d, wall, h } = BODY;
  const list = [];
  const tft = makeTFT();
  tft.group.position.set(0, SCREEN_Y + 2.6, d / 2 - wall - 3.1);
  list.push({ obj: tft.group, explode: [0, 0, 55] });
  const enc = makeKY040(true);
  enc.group.rotation.x = Math.PI / 2;                       // shaft toward the front
  enc.group.position.set(0, DIAL_Y + 3, d / 2 - wall - 8.1 - 6.5);
  list.push({ obj: enc.group, explode: [0, 0, 38] });
  const mic = makeINMP441();
  mic.group.rotation.x = Math.PI / 2;
  mic.group.position.set(MIC[0], MIC[1] - 3.6, d / 2 - wall - 1.3);
  list.push({ obj: mic.group, explode: [26, 0, 30] });
  const spk = await makeSpeaker();
  spk.group.rotation.x = -Math.PI / 2;                      // cone faces the back grille
  spk.group.position.set(0, 18 - 17, -d / 2 + wall + 0.1);
  list.push({ obj: spk.group, explode: [0, 0, -40] });
  const amp = await makeAmp();
  amp.group.rotation.z = Math.PI / 2;
  amp.group.position.set(BODY.w / 2 - wall - 1, -20, -12);
  list.push({ obj: amp.group, explode: [42, 0, 0] });
  const dev = makeDevKit();
  const exp = makeExpansionBoard(dev);
  exp.group.position.set(0, -h / 2 + wall + 4, 0);
  list.push({ obj: exp.group, explode: [-115, 20, 0] });
  for (const it of list) it.home = it.obj.position.clone();
  return list;
}

// ---------------------------------------------------------------- UI wiring
let current = null, explodeT = 0, explodeTarget = 0, stepIndex = -1, showWires = true, showLabels = true;
const camTween = { from: null, to: null, start: 0, dur: 700 };

function setView(name) {
  for (const v of Object.values(views)) scene.remove(v.root);
  current = views[name];
  scene.add(current.root);
  explodeTarget = current.exploded ? 1 : 0;
  document.querySelectorAll('#views button').forEach((b) => b.setAttribute('aria-pressed', String(b.dataset.view === name)));
  flyTo(current.cam.pos, current.cam.target);
  renderSteps();
  stepIndex = -1;
  applyStep();
  try { localStorage.setItem('remi-sim-view', name); } catch {}
  if (location.hash !== '#' + name) history.replaceState(null, '', '#' + name);
}
function flyTo(pos, target) {
  camTween.from = { p: camera.position.clone(), t: controls.target.clone() };
  camTween.to = { p: new THREE.Vector3(...pos), t: new THREE.Vector3(...target) };
  camTween.start = performance.now();
}
function renderSteps() {
  const ol = document.getElementById('steps');
  ol.innerHTML = '';
  current.steps.forEach((s, i) => {
    const li = document.createElement('li');
    li.innerHTML = `<b></b><span></span>`;
    li.querySelector('b').textContent = s.t;
    li.querySelector('span').textContent = s.d;
    li.onclick = () => { stepIndex = stepIndex === i ? -1 : i; applyStep(); };
    ol.appendChild(li);
  });
}
function applyStep() {
  document.querySelectorAll('#steps li').forEach((li, i) => (i === stepIndex ? li.setAttribute('aria-current', 'step') : li.removeAttribute('aria-current')));
  const step = current.steps[stepIndex];
  if (current.wires) {
    for (const w of current.wires.children) {
      const info = w.userData.wire;
      const always = !info.group || info.group === 'power';
      const inStep = step && step.groups && step.groups.includes(info.group);
      if (info.group === 'upload') w.visible = !!(step && step.show && step.show.includes(w));
      else w.visible = showWires;
      w.material.opacity = !step || inStep || (always && !step.groups?.length) ? (info.planned ? 0.45 : 1) : 0.12;
      w.material.transparent = true;
    }
  }
  const focus = step && step.focus && parts[current.prefix + step.focus] ? current.prefix + step.focus : null;
  if (focus) {
    selectPart(focus);
    const p = new THREE.Vector3();
    parts[focus].group.getWorldPosition(p);
    const dir = camera.position.clone().sub(controls.target).normalize();
    flyTo(p.clone().addScaledVector(dir, current === views.product || current === views.exploded ? 220 : 170).toArray(), p.toArray());
  } else {
    flyTo(current.cam.pos, current.cam.target);
  }
}
let selected = null;
function selectPart(id) {
  selected = id;
  const p = parts[id];
  const card = document.getElementById('part');
  card.innerHTML = '<h3></h3><div class="meta"></div><table></table>';
  card.querySelector('h3').textContent = p.info.name;
  card.querySelector('.meta').textContent = p.info.meta;
  const t = card.querySelector('table');
  for (const [k, v] of p.info.rows) {
    const tr = t.insertRow();
    tr.insertCell().textContent = k;
    tr.insertCell().textContent = v;
  }
}

// knob animation (visual only)
let knobAngle = 0, knobPress = 0;
function animateKnob(kind) {
  if (kind === 0) knobAngle += Math.PI / 10;
  if (kind === 1) knobAngle -= Math.PI / 10;
  if (kind === 2 || kind === 3) knobPress = 1;
}
function applyKnobVisual() {
  knobPress *= 0.82;
  const benchKnob = views.bench?.knob?.knob;
  if (benchKnob) { benchKnob.rotation.y = knobAngle; benchKnob.position.y = 1.6 + 6.5 + 7 + 8 - knobPress * 1.2; }
  const dial = views.product?.product.dial;
  if (dial) { dial.rotation.z = knobAngle; dial.position.z = BODY.d / 2 + 1.2 + 4.5 - knobPress * 1.2; }
}

// pointer: select parts, drag the knob to turn it, click to press, hold to go back
const ray = new THREE.Raycaster(), ndc = new THREE.Vector2();
let drag = null;
function hit(e, list) {
  const r = renderer.domElement.getBoundingClientRect();
  ndc.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
  ray.setFromCamera(ndc, camera);
  return ray.intersectObjects(list.filter((o) => isInScene(o)), false)[0];
}
function isInScene(o) { let p = o; while (p) { if (p === scene) return true; p = p.parent; } return false; }
renderer.domElement.addEventListener('pointerdown', (e) => {
  const k = hit(e, knobs);
  if (k) {
    controls.enabled = false;
    const c = new THREE.Vector3(); k.object.getWorldPosition(c); c.project(camera);
    const r = renderer.domElement.getBoundingClientRect();
    drag = { cx: (c.x + 1) / 2 * r.width + r.left, cy: (1 - c.y) / 2 * r.height + r.top, last: null, acc: 0, moved: 0, t: performance.now(), held: false };
    drag.last = Math.atan2(e.clientY - drag.cy, e.clientX - drag.cx);
    drag.timer = setTimeout(() => { if (drag && drag.moved < 0.25) { drag.held = true; sendInput(3); } }, 600);
    try { renderer.domElement.setPointerCapture(e.pointerId); } catch {}
    return;
  }
  const p = hit(e, pickables);
  if (p) selectPart(p.object.userData.part);
});
renderer.domElement.addEventListener('pointermove', (e) => {
  if (!drag) { renderer.domElement.style.cursor = hit(e, knobs) ? 'grab' : ''; return; }
  const a = Math.atan2(e.clientY - drag.cy, e.clientX - drag.cx);
  let da = a - drag.last; if (da > Math.PI) da -= 2 * Math.PI; if (da < -Math.PI) da += 2 * Math.PI;
  drag.last = a; drag.acc += da; drag.moved += Math.abs(da);
  const detent = Math.PI / 10;  // 20 clicks per turn
  while (drag.acc > detent) { drag.acc -= detent; sendInput(1); }
  while (drag.acc < -detent) { drag.acc += detent; sendInput(0); }
});
renderer.domElement.addEventListener('pointerup', () => {
  if (!drag) return;
  clearTimeout(drag.timer);
  if (!drag.held && drag.moved < 0.25) sendInput(2);
  drag = null;
  controls.enabled = true;
});
renderer.domElement.addEventListener('wheel', (e) => {
  if (!hit(e, knobs)) return;
  e.preventDefault(); e.stopImmediatePropagation();
  sendInput(e.deltaY > 0 ? 1 : 0);
}, { capture: true, passive: false });

document.getElementById('left').onclick = () => sendInput(0);
document.getElementById('right').onclick = () => sendInput(1);
document.getElementById('press').onclick = () => sendInput(2);
document.getElementById('hold').onclick = () => sendInput(3);
document.getElementById('replay').onclick = () => { if (wasm) { wasm.remi_init(Math.round(performance.now() - t0)); wasm.remi_sound(audio ? 1 : 0); } };
document.getElementById('sound').onclick = async (e) => {
  const b = e.currentTarget;
  if (!audio) {
    try { audio = new AudioContext(); await audio.resume(); nextAudioTime = audio.currentTime + 0.05; } catch { audio = null; }
    if (audio && wasm) wasm.remi_sound(1);
  } else {
    await audio.close(); audio = null; if (wasm) wasm.remi_sound(0);
  }
  b.setAttribute('aria-pressed', String(!!audio));
  b.textContent = audio ? 'Sound on' : 'Sound off';
};
document.getElementById('wires').onclick = (e) => { showWires = !showWires; e.currentTarget.setAttribute('aria-pressed', String(showWires)); applyStep(); };
document.getElementById('labels').onclick = (e) => { showLabels = !showLabels; e.currentTarget.setAttribute('aria-pressed', String(showLabels)); };
document.querySelectorAll('#views button').forEach((b) => (b.onclick = () => setView(b.dataset.view)));
addEventListener('keydown', (e) => {
  if (e.target.closest('button') && (e.key === ' ' || e.key === 'Enter')) return;
  const k = { ArrowLeft: 0, ArrowRight: 1, ' ': 2, Enter: 2, Escape: 3 }[e.key];
  if (k !== undefined) { e.preventDefault(); sendInput(k); }
});
addEventListener('hashchange', () => { const v = location.hash.slice(1); if (views[v]) setView(v); });

// legend
const legend = document.getElementById('legend');
for (const [k, c] of Object.entries({ '5 V': WIRE_COLORS['5V'], '3.3 V': WIRE_COLORS['3V3'], GND: WIRE_COLORS.GND, 'SPI SCK': WIRE_COLORS.SCK, 'SPI MOSI': WIRE_COLORS.MOSI,
  'TFT CS': WIRE_COLORS.CS, 'TFT DC': WIRE_COLORS.DC, 'Enc CLK': WIRE_COLORS.CLK, 'Enc DT': WIRE_COLORS.DT, 'Enc SW': WIRE_COLORS.SW, 'I2S BCLK': WIRE_COLORS.BCLK,
  'I2S LRC': WIRE_COLORS.LRC, 'I2S DIN': WIRE_COLORS.DIN, 'Mic SD': WIRE_COLORS.MIC, Speaker: WIRE_COLORS.SPK, Serial: WIRE_COLORS.UART })) {
  const d = document.createElement('div');
  d.innerHTML = `<span class="sw" style="background:${c};outline:1px solid rgba(0,0,0,.15)"></span>${k}`;
  legend.appendChild(d);
}

// labels
const labelEls = new Map();
function updateLabels() {
  const ids = showLabels && current ? current.labels.map((id) => current.prefix + id) : [];
  const r = renderer.domElement.getBoundingClientRect();
  for (const [id, el] of labelEls) if (!ids.includes(id)) el.style.display = 'none';
  for (const id of ids) {
    let el = labelEls.get(id);
    if (!el) { el = document.createElement('div'); el.className = 'tag'; el.textContent = parts[id].info.name.split(/[(,]/)[0].trim(); labelLayer.appendChild(el); labelEls.set(id, el); }
    const box3 = new THREE.Box3().setFromObject(parts[id].group);
    const p = new THREE.Vector3((box3.min.x + box3.max.x) / 2, box3.max.y + 2, (box3.min.z + box3.max.z) / 2).project(camera);
    el.style.display = p.z < 1 ? 'block' : 'none';
    el.style.left = `${(p.x + 1) / 2 * r.width}px`;
    el.style.top = `${(1 - p.y) / 2 * r.height}px`;
  }
}

// ---------------------------------------------------------------- loop
function resize() {
  const w = stage.clientWidth, h = stage.clientHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
}
new ResizeObserver(resize).observe(stage);
const reduceMotion = matchMedia('(prefers-reduced-motion: reduce)').matches;
function tick() {
  requestAnimationFrame(tick);
  const now = performance.now();
  if (camTween.to) {
    const t = reduceMotion ? 1 : Math.min(1, (now - camTween.start) / camTween.dur), e = 1 - Math.pow(1 - t, 3);
    camera.position.lerpVectors(camTween.from.p, camTween.to.p, e);
    controls.target.lerpVectors(camTween.from.t, camTween.to.t, e);
    if (t >= 1) camTween.to = null;
  }
  explodeT += (explodeTarget - explodeT) * (reduceMotion ? 1 : 0.12);
  if (current?.product) {
    const pr = current.product, e = explodeT;
    pr.front.position.z = BODY.d / 2 - BODY.wall + e * 95;
    pr.dial.position.z = BODY.d / 2 + 1.2 + 4.5 + e * 135 - knobPress * 1.2;
    pr.back.position.z = -BODY.d / 2 - e * 95;
    pr.hat.position.y = BODY.h / 2 + e * 75;
    pr.shell.material.opacity = 1 - e * 0.55; pr.shell.material.transparent = e > 0.01;
    for (const it of current.internals) it.obj.position.copy(it.home).add(new THREE.Vector3(...it.explode).multiplyScalar(e));
  }
  applyKnobVisual();
  updateScreen();
  controls.update();
  renderer.render(scene, camera);
  updateLabels();
}

(async () => {
  try {
    await Promise.all([loadFirmware().catch((err) => { console.error(err); document.getElementById('loading').textContent = 'Firmware simulation could not start in this browser.'; }), buildViews()]);
  } catch (err) {
    console.error(err);
    document.getElementById('loading').textContent = 'Could not load the 3D models: ' + err.message;
    return;
  }
  document.getElementById('loading').remove();
  resize();
  let start = location.hash.slice(1);
  if (!views[start]) { try { start = localStorage.getItem('remi-sim-view'); } catch {} }
  setView(views[start] ? start : 'bench');
  camera.position.copy(camTween.to.p).multiplyScalar(1.4);
  camTween.from = { p: camera.position.clone(), t: controls.target.clone() };
  window.remiSim = { camera, controls, views, parts, get wasm() { return wasm; }, renderOnce() { applyKnobVisual(); updateScreen(); renderer.render(scene, camera); } };  // console debugging
  tick();
})();
