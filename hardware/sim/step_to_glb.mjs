// Converts a STEP CAD file to a compact GLB for the 3D simulator, using occt-import-js (Open CASCADE in WebAssembly).
//   npm install occt-import-js
//   node sim/step_to_glb.mjs input.step output.glb [scale=0.001]
// STEP units are millimetres; the simulator works in metres, so the default scale is 0.001.
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';

const [input, output, scaleArg] = process.argv.slice(2);
if (!input || !output) throw new Error('usage: node sim/step_to_glb.mjs input.step output.glb [scale]');
const scale = Number(scaleArg ?? 0.001);
const occt = await createRequire(import.meta.url)('occt-import-js')();
const result = occt.ReadStepFile(new Uint8Array(readFileSync(input)), null);
if (!result.success) throw new Error('STEP import failed');

// One glTF mesh per STEP mesh, flat colour from the CAD file.
const chunks = [], accessors = [], views = [], meshes = [], nodes = [], materials = [], matIndex = new Map();
let offset = 0;
function addView(typed, target) {
  const bytes = new Uint8Array(typed.buffer, typed.byteOffset, typed.byteLength);
  const pad = (4 - (bytes.length % 4)) % 4;
  chunks.push(bytes, new Uint8Array(pad));
  views.push({ buffer: 0, byteOffset: offset, byteLength: bytes.length, target });
  offset += bytes.length + pad;
  return views.length - 1;
}
for (const m of result.meshes) {
  const pos = new Float32Array(m.attributes.position.array.map((v) => v * scale));
  const nrm = new Float32Array(m.attributes.normal?.array ?? []);
  const idx = new Uint32Array(m.index.array);
  const min = [Infinity, Infinity, Infinity], max = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i < pos.length; i += 3)
    for (let k = 0; k < 3; k++) { min[k] = Math.min(min[k], pos[i + k]); max[k] = Math.max(max[k], pos[i + k]); }
  const attributes = { POSITION: accessors.push({ bufferView: addView(pos, 34962), componentType: 5126, count: pos.length / 3, type: 'VEC3', min, max }) - 1 };
  if (nrm.length) attributes.NORMAL = accessors.push({ bufferView: addView(nrm, 34962), componentType: 5126, count: nrm.length / 3, type: 'VEC3' }) - 1;
  const indices = accessors.push({ bufferView: addView(idx, 34963), componentType: 5125, count: idx.length, type: 'SCALAR' }) - 1;
  const c = m.color ?? [0.6, 0.6, 0.6];
  const key = c.map((v) => v.toFixed(3)).join(',');
  if (!matIndex.has(key)) {
    matIndex.set(key, materials.length);
    materials.push({ pbrMetallicRoughness: { baseColorFactor: [...c, 1], metallicFactor: 0.1, roughnessFactor: 0.6 } });
  }
  meshes.push({ name: m.name || `part${meshes.length}`, primitives: [{ attributes, indices, material: matIndex.get(key) }] });
  nodes.push({ mesh: meshes.length - 1 });
}
const bin = new Uint8Array(offset);
let o = 0;
for (const c of chunks) { bin.set(c, o); o += c.length; }
const json = JSON.stringify({
  asset: { version: '2.0', generator: 'Remi step_to_glb (occt-import-js)' },
  scene: 0, scenes: [{ nodes: nodes.map((_, i) => i) }], nodes, meshes, materials, accessors, bufferViews: views,
  buffers: [{ byteLength: bin.length }],
});
const jsonBytes = new TextEncoder().encode(json + ' '.repeat((4 - (json.length % 4)) % 4));
const total = 12 + 8 + jsonBytes.length + 8 + bin.length;
const out = Buffer.alloc(total);
out.writeUInt32LE(0x46546c67, 0); out.writeUInt32LE(2, 4); out.writeUInt32LE(total, 8);
out.writeUInt32LE(jsonBytes.length, 12); out.writeUInt32LE(0x4e4f534a, 16); Buffer.from(jsonBytes).copy(out, 20);
const b = 20 + jsonBytes.length;
out.writeUInt32LE(bin.length, b); out.writeUInt32LE(0x004e4942, b + 4); Buffer.from(bin).copy(out, b + 8);
writeFileSync(output, out);
console.log(`${output}: ${result.meshes.length} meshes, ${(total / 1024).toFixed(0)} KB`);
