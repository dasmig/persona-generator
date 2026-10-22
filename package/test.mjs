// Tests the assembled npm package's Node entry: node package/test.mjs build/package
// (package/browser-test.mjs tests the browser entry).
//
// The known answers are persona-generator's (tests/tests.cpp), so a
// pass means the WebAssembly build gives the same personas as a native one.
import assert from 'node:assert/strict';
import path from 'node:path';
import { pathToFileURL } from 'node:url';


const dir = path.resolve(process.argv[2] || 'build/package');
const { createPersonaGenerator, createPersonaPool } = await import(pathToFileURL(path.join(dir, 'node.mjs')).href);
const personas = await createPersonaGenerator();

// persona::to_string(), from the JSON.
const summary = p => [p.name, p.age, p.sex, p.city.name, p.country.name_common,
  p.occupation.job ? p.occupation.job.label : p.occupation.status].join(', ');
const signature = p => p.signature.seed + '-' + p.signature.data;

assert.equal(personas.countries.length, 79);
assert.ok(!personas.countries.includes('KR'));
assert.equal(personas.dataHash, 'ad4adc1a06ddb63b');
assert.equal(personas.maxTraits, 52);
assert.equal(personas.versions['persona-generator'].startsWith('v'), true, JSON.stringify(personas.versions));
assert.equal(Object.keys(personas.versions).length, 11);
assert.ok(Object.isFrozen(personas) && Object.isFrozen(personas.countries));

// Known answers.
const one = personas.generate({ seed: 1 });
assert.equal(summary(one), 'Nigel Phillips, 13, male, Gosport, United Kingdom, student');
assert.equal(signature(one), '0000000000000001-a8919ddcdd84eea5');
const two = personas.generate({ seed: '2' });
assert.equal(summary(two), 'Sadaf Esmaeili, 16, female, Mashhad, Iran, student');
assert.equal(signature(two), '0000000000000002-a8919ddcdd84eea5');
const three = personas.generate({ country: 'BR', sex: 'female', ages: { min: 25, max: 35 }, traits: 5, seed: 3n });
assert.equal(summary(three), 'Jéssica Tomaz Cardozo, 34, female, São Paulo, Brazil, Farm labourer');
assert.equal(signature(three), '0000000000000003-7612de93fd4e41c3');
assert.equal(three.personality.traits.length, 5);
const four = personas.generate({ country: 'jp', referenceDate: '2030-03-09', seed: 4 });
assert.equal(summary(four), 'Iwasaki Yasushi, 70, male, Edogawe, Japan, Shop assistant');
assert.equal(signature(four), '0000000000000004-3a346bd12f7d1be9');

// Relations add the family and change nothing else.
const plain = personas.generate({ country: 'BR', seed: 7 });
assert.equal(plain.relatives, undefined);
const withFamily = personas.generate({ country: 'BR', seed: 7, relations: true });
const { relatives, ...rest } = withFamily;
assert.deepEqual(rest, plain);
assert.equal(relatives.members[0].role, 'anchor');
assert.equal(relatives.members[0].birth, plain.birth.date);
assert.equal(relatives.country, 'BR');
assert.ok(relatives.members.some(m => m.role === 'mother') && relatives.members.some(m => m.role === 'father'));
assert.deepEqual(personas.generate({ country: 'BR', seed: 7, relations: {} }), withFamily);
const grand = personas.generate({ country: 'BR', seed: 7, relations: { up: 2, down: 0, partners: false, names: false } });
assert.ok(grand.relatives.members.some(m => m.role === 'paternal_grandfather'));
assert.ok(grand.relatives.members.every(m => m.name === undefined && m.role !== 'partner' && m.role !== 'child'));
const homed = personas.generate({ country: 'BR', seed: 7, relations: { stepparents: true, home: true } });
assert.equal(homed.relatives.household.members[0], 0);
assert.ok(['one_person', 'couple', 'nuclear', 'single_parent', 'with_parents', 'multi_generation', 'other'].includes(homed.relatives.household.type));
assert.equal(withFamily.relatives.household, undefined);
assert.throws(() => personas.generate({ relations: { stepparents: 1 } }), TypeError);
assert.throws(() => personas.generate({ relations: { up: 3 } }), RangeError);
assert.throws(() => personas.generate({ relations: { depth: 1 } }), TypeError);
assert.equal(personas.generateBatch({ request: { country: 'BR', relations: true }, count: 3, seed: 5 })[2].relatives.members[0].role, 'anchor');

// The hash of 2,000 JSON documents. The native test runs without admin1
// names, which this module embeds, so they are blanked before hashing.
let hash = 0xcbf29ce484222325n;
const mask = (1n << 64n) - 1n;
const encoder = new TextEncoder();
for (let s = 0; s < 2000; ++s) {
  const json = personas.generateJson({ seed: s }).replace(/"admin1":"(?:[^"\\]|\\.)*"/, '"admin1":""');
  for (const byte of encoder.encode(json)) hash = ((hash ^ BigInt(byte)) * 0x100000001b3n) & mask;
}
assert.equal(hash.toString(16), 'dcba62c57573e127');

// Admin1 names are embedded.
assert.equal(personas.generate({ country: 'BR', seed: 1 }).admin1.length > 0, true);

// A random persona replays from its seed.
const random = personas.generate({ country: 'DE', referenceDate: new Date(2026, 9, 15) });
assert.equal(random.birth.date <= '2026-10-15', true);
assert.deepEqual(personas.generate({ country: 'DE', referenceDate: '2026-10-15', seed: random.signature.seed }), random);
assert.notEqual(personas.generate().signature.seed, personas.generate().signature.seed);

// Requests.
for (let s = 0; s < 40; ++s) {
  const p = personas.generate({ ages: { min: 30, max: 35 }, seed: s });
  assert.ok(p.age >= 30 && p.age <= 35);
  assert.equal(p.biodata.age, p.age);
}
const female = personas.generate({ country: 'FR', sex: 'female', seed: 9 });
const male = personas.generate({ country: 'FR', sex: 'male', seed: 9 });
assert.equal(female.sex, 'female');
assert.equal(male.sex, 'male');
assert.deepEqual([male.city, male.personality], [female.city, female.personality]);
assert.equal(personas.generate({ seed: 'ffffffffffffffff' }).signature.seed, 'ffffffffffffffff');
assert.equal(personas.generate({ seed: (1n << 64n) - 1n }).signature.seed, 'ffffffffffffffff');
assert.equal(personas.generate({ seed: '0x2A' }).signature.seed, '000000000000002a');
assert.deepEqual(personas.generate({ seed: '2a' }), personas.generate({ seed: 42 }));
assert.equal(personas.generate({ traits: 20, seed: 1 }).personality.traits.length, 20);
const most = personas.generate({ traits: personas.maxTraits, seed: 1 }).personality.traits.length;
assert.ok(most > 20 && most <= personas.maxTraits, 'fewer traits only when no other is compatible');
assert.equal(personas.generate({ country: 'jp', seed: 1 }).country.cca2, 'JP');

// Errors: the wrapper checks the request, so a malformed field is a
// TypeError and a value out of range is a RangeError, before the module runs.
for (const [request, kind, message] of [
  [{ country: 'XX' }, RangeError, /"XX" is not supported/],
  [{ country: 'NZ' }, RangeError, /not supported/],
  [{ country: 'KR' }, RangeError, /not supported/],
  [{ country: ' br ' }, RangeError, /not supported/],
  [{ country: 7 }, TypeError, /country/],
  [{ contry: 'BR' }, TypeError, /unknown field "contry"/],
  [{ country: 'US', referenceDate: '2023-02-30' }, RangeError, /not a calendar date/],
  [{ referenceDate: '2023-2-3' }, RangeError, /YYYY-MM-DD/],
  [{ referenceDate: '1899-12-31' }, RangeError, /1900 and 2100/],
  [{ referenceDate: new Date(NaN) }, RangeError, /invalid Date/],
  [{ referenceDate: 20230701 }, TypeError, /referenceDate/],
  [{ ages: { min: 40, max: 20 } }, RangeError, /ages.min \(40\)/],
  [{ ages: { min: 20 } }, TypeError, /ages.max/],
  [{ ages: { min: -1, max: 20 } }, RangeError, /ages.min/],
  [{ ages: { min: 20, max: 121 } }, RangeError, /ages.max/],
  [{ traits: 1.5 }, TypeError, /traits/],
  [{ traits: 53 }, RangeError, /traits must be from 0 to 52/],
  [{ sex: 'other' }, RangeError, /sex/],
  [{ sex: 1 }, TypeError, /sex/],
  [{ seed: '10000000000000000' }, RangeError, /hexadecimal/],
  [{ seed: 'xyz' }, RangeError, /hexadecimal/],
  [{ seed: -1 }, RangeError, /seed/],
  [{ seed: 2 ** 53 }, RangeError, /bigint/],
  [{ seed: 1n << 64n }, RangeError, /seed/],
  [{ seed: {} }, TypeError, /seed/],
  [null, TypeError, /object/],
  [[], TypeError, /object/]
]) {
  assert.throws(() => personas.generate(request), error => error instanceof kind && message.test(error.message),
    JSON.stringify(request, (_, v) => typeof v === 'bigint' ? v.toString() : v));
}
// A failed call leaves the generator usable.
assert.equal(signature(personas.generate({ seed: 1 })), '0000000000000001-a8919ddcdd84eea5');

// Batches. Persona i of a batch is the single call with the seed
// core::item_seed(batchSeed, i): splitmix64_mix(seed ^ splitmix64_mix(i ^ 0x6a09e667f3bcc909)).
const mix = z => {
  z = ((z ^ (z >> 30n)) * 0xbf58476d1ce4e5b9n) & mask;
  z = ((z ^ (z >> 27n)) * 0x94d049bb133111ebn) & mask;
  return z ^ (z >> 31n);
};
const itemSeed = (batchSeed, index) => mix(batchSeed ^ mix(BigInt(index) ^ 0x6a09e667f3bcc909n));

const request = { country: 'BR', sex: 'female', ages: { min: 20, max: 40 }, traits: 4 };
const batch = personas.generateBatch({ request, count: 60, seed: 7 });
assert.equal(batch.length, 60);
for (let i = 0; i < batch.length; ++i) {
  assert.deepEqual(batch[i], personas.generate({ ...request, seed: itemSeed(7n, i) }), `persona ${i}`);
}
// A range is generated directly, and two batches that follow each other equal one.
assert.deepEqual(personas.generateBatch({ request, count: 20, first: 40, seed: 7 }), batch.slice(40));
assert.deepEqual([...personas.generateBatch({ request, count: 25, seed: '7' }),
  ...personas.generateBatch({ request, count: 35, first: 25, seed: 7n })], batch);
// JSON Lines: one persona per line, as the C++ library writes it.
const lines = personas.generateBatchJsonl({ request, count: 60, seed: 7 });
assert.ok(lines.endsWith('\n'));
assert.equal(lines.split('\n').length, 61);
assert.deepEqual(lines.slice(0, -1).split('\n').map(line => JSON.parse(line)), batch);
assert.deepEqual(personas.generateBatch({ count: 0, seed: 1 }), []);
assert.equal(personas.generateBatchJsonl({ count: 0 }), '');
// Without a seed the batch is random but each persona replays from its signature.
const unseeded = personas.generateBatch({ count: 3 });
assert.notEqual(unseeded[0].signature.seed, unseeded[1].signature.seed);
assert.deepEqual(personas.generate({ seed: unseeded[2].signature.seed }), unseeded[2]);
for (const [options, kind, message] of [
  [{}, TypeError, /count is required/],
  [{ count: 1.5 }, TypeError, /count/],
  [{ count: -1 }, RangeError, /count/],
  [{ count: 2 ** 31 }, RangeError, /count/],
  [{ count: 1, first: -1 }, RangeError, /first/],
  [{ count: 1, first: 2 ** 53 }, RangeError, /first/],
  [{ count: 1, seed: 'xyz' }, RangeError, /hexadecimal/],
  [{ count: 1, request: { seed: 1 } }, TypeError, /own seed/],
  [{ count: 1, request: { country: 'XX' } }, RangeError, /not supported/],
  [{ count: 1, request: { contry: 'BR' } }, TypeError, /unknown field/],
  [{ count: 1, extra: 1 }, TypeError, /unknown batch field/],
  [null, TypeError, /object/]
]) {
  assert.throws(() => personas.generateBatch(options), error => error instanceof kind && message.test(error.message),
    JSON.stringify(options));
}

// Pools of 1, 2 and 4 workers give the same personas as one generator.
const poolOptions = { request: { sex: 'male' }, count: 203, first: 11, seed: 'abc123' };
const expected = personas.generateBatchJsonl(poolOptions);
for (const workers of [1, 2, 4]) {
  const pool = await createPersonaPool({ workers });
  try {
    assert.equal(pool.size, workers);
    assert.equal(await pool.generateBatchJsonl(poolOptions), expected, `${workers} workers`);
    assert.deepEqual(await pool.generateBatch({ ...poolOptions, count: 3 }),
      personas.generateBatch({ ...poolOptions, count: 3 }));
    assert.deepEqual(await pool.generateBatch({ count: 0 }), []);
    // Fewer personas than workers, and two calls at once.
    const [few, other] = await Promise.all([pool.generateBatchJsonl({ ...poolOptions, count: 1 }),
      pool.generateBatchJsonl({ ...poolOptions, count: 5 })]);
    assert.equal(few, expected.slice(0, expected.indexOf('\n') + 1));
    assert.equal(other, expected.split('\n').slice(0, 5).join('\n') + '\n');
    await assert.rejects(pool.generateBatch({ count: 1, request: { country: 'XX' } }), RangeError);
    await assert.rejects(pool.generateBatch({ count: 1, seed: 'xyz' }), RangeError);
    assert.equal(await pool.generateBatchJsonl(poolOptions), expected, 'still usable after errors');
  } finally {
    pool.close();
  }
  await assert.rejects(pool.generateBatch({ count: 1 }), /closed/);
}
await assert.rejects(createPersonaPool({ workers: 0 }), RangeError);
await assert.rejects(createPersonaPool({ workers: 1.5 }), TypeError);
await assert.rejects(createPersonaPool({ threads: 2 }), TypeError);

console.log('PASS: @dasmig/persona matches the persona-generator known answers (5 personas, 2,000-document hash), replay, requests, errors, batches and worker pools.');
