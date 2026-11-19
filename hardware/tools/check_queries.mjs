// Validates the device's GraphQL operations (firmware/Remi_Device/queries.h) against the Remi API schema.
// Usage: node tools/check_queries.mjs <path to Remi repo>/backend
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { join } from 'node:path';

const backend = process.argv[2];
if (!backend) throw new Error('usage: node tools/check_queries.mjs <Remi repo>/backend');
const { buildSchema, parse, validate } = createRequire(join(backend, 'package.json'))('graphql');
const schema = buildSchema(readFileSync(join(backend, 'schema.graphql'), 'utf8'));
const header = readFileSync(new URL('../firmware/Remi_Device/queries.h', import.meta.url), 'utf8');

let failed = 0, count = 0;
for (const [, name, body] of header.matchAll(/static const char (Q_\w+)\[\] =\s*((?:\s*"(?:[^"\\]|\\.)*")+);/g)) {
  const query = [...body.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((m) => m[1]).join('');
  const errors = validate(schema, parse(query));
  count++;
  if (errors.length) { failed++; console.log(`FAIL ${name}: ${errors.map((e) => e.message).join('; ')}`); }
  else console.log(`ok   ${name}`);
}
if (!count) throw new Error('no queries found');
process.exit(failed ? 1 : 0);
