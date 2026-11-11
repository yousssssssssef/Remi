const fs = require('node:fs'), test = require('node:test'), assert = require('node:assert/strict'), ts = require('typescript');
require.extensions['.ts'] = (m,f) => m._compile(ts.transpileModule(fs.readFileSync(f,'utf8'), {compilerOptions:{module:ts.ModuleKind.CommonJS}}).outputText,f);
const { createCookingJournal, parseCookingJournal, cookingCareer } = require('../src/services/cooking-journal.ts');
const { advanceCooking, initialCookingSession, cookingPoints, recordCookingTimer, completeCookingTimer } = require('../src/utils/cooking-session.ts');
function fixture() { const data = new Map(); let fail = false; return { data, fail: value => { fail = value; }, storage: { getItem: async key => data.get(key) ?? null, setItem: async (key,value) => { if (fail) throw Error('full disk'); data.set(key,value); } } }; }
function run(id='run-1') { return { ...initialCookingSession, runId:id, startedAt:1000, totalSteps:2 }; }
function finish(s=run()) { return { ...advanceCooking(advanceCooking(s,2),2), finishedAt:5000 }; }
test('a new journal instance restores progress and completed XP after application restart, isolated by account',async()=>{
 const f=fixture(), first=createCookingJournal(f.storage); const partial=advanceCooking(run(),2);
 await first.save('alice','recipe',partial); const restarted=createCookingJournal(f.storage);
 assert.deepEqual((await restarted.load('alice')).sessions.recipe,partial); assert.equal(Object.keys((await restarted.load('bob')).sessions).length,0);
 await restarted.save('alice','recipe',finish(partial));
 const journal=await createCookingJournal(f.storage).load('alice'); assert.equal(cookingCareer(journal.history).xp,70); assert.equal(cookingCareer(journal.history).cooks,1);
});
test('concurrent saves preserve other recipes and replaying the same completion cannot double-award XP',async()=>{
 const f=fixture(), store=createCookingJournal(f.storage), done=finish();
 await Promise.all([store.save('alice','one',advanceCooking(run(),2)),store.save('alice','one',done),store.save('alice','two',finish(run('run-2'))),store.save('alice','one',done)]);
 const saved=await store.load('alice'); assert.equal(Object.keys(saved.sessions).length,2); assert.equal(cookingCareer(saved.history).cooks,2); assert.equal(cookingCareer(saved.history).xp,140);
 await store.save('alice','one',advanceCooking(run(),2));assert.equal((await store.load('alice')).sessions.one.finished,true,'a late partial save cannot undo completion');
});
test('a failed write is retryable, leaves previous progress intact and does not award phantom achievements',async()=>{
 const f=fixture(), store=createCookingJournal(f.storage);await store.save('alice','one',advanceCooking(run(),2));f.fail(true);
 await assert.rejects(store.save('alice','one',finish()),/full disk/);assert.equal(cookingCareer((await store.load('alice')).history).cooks,0);
 f.fail(false);await store.save('alice','one',finish());assert.equal(cookingCareer((await store.load('alice')).history).cooks,1);
});
test('first, five, ten, new-recipe and timer milestones reflect real distinct completed cooks',async()=>{
 const f=fixture(), store=createCookingJournal(f.storage);let timed=recordCookingTimer(run('run-1'),0);timed=completeCookingTimer(timed,0);
 for(let i=1;i<=10;i++) await store.save('alice',i===10?'new-recipe':'repeat',finish(i===1?timed:run(`run-${i}`)));
 const career=cookingCareer((await store.load('alice')).history,'run-10');assert.equal(career.cooks,10);assert.equal(career.xp,700);assert.equal(career.recipes,2);
 for(const key of ['first','five','ten','newRecipe','timer'])assert.equal(career[key],true,key);
 assert.equal(completeCookingTimer(run(),0).completedTimers,undefined,'an unstarted timer earns no timer credit');
});
test('invalid persisted sessions are rejected rather than silently receiving completion bonuses',()=>{
 for(const session of [{...run(),finished:true},{...finish(),completed:[0,0]},{...run(),current:99},{...run(),timerSteps:[0],completedTimers:[1]}])
 assert.throws(()=>parseCookingJournal(JSON.stringify({version:1,sessions:{one:session},history:{}})));
 assert.equal(cookingPoints(initialCookingSession),0);
});
