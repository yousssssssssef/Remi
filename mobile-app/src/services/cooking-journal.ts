import { cookingPoints, type CookingSession } from '../utils/cooking-session';

export interface SavedCookingSession extends CookingSession { runId: string; startedAt: number; totalSteps: number; finishedAt?: number }
export interface CompletedCook { recipeId: string; session: SavedCookingSession }
export interface CookingJournal { version: 1; sessions: Record<string, SavedCookingSession>; history: Record<string, CompletedCook> }
interface Storage { getItem(key: string): Promise<string | null>; setItem(key: string, value: string): Promise<void> }
const empty = (): CookingJournal => ({ version: 1, sessions: {}, history: {} });
const record = (v: unknown): v is Record<string, unknown> => typeof v === 'object' && v !== null && !Array.isArray(v);
function validSession(v: unknown): v is SavedCookingSession {
  if (!record(v) || typeof v.runId !== 'string' || !v.runId || typeof v.startedAt !== 'number' || !Number.isFinite(v.startedAt) || v.startedAt <= 0 || typeof v.totalSteps !== 'number' || !Number.isInteger(v.totalSteps) || v.totalSteps < 1 || v.totalSteps > 1000) return false;
  const total = v.totalSteps;
  const steps = (x: unknown): x is number[] => Array.isArray(x) && x.every(i => Number.isInteger(i) && i >= 0 && i < total) && new Set(x).size === x.length;
  return Number.isInteger(v.current) && Number(v.current) >= 0 && Number(v.current) < total && steps(v.completed)
    && typeof v.finished === 'boolean' && v.finished === (v.completed.length === total)
    && (v.timerSteps === undefined || steps(v.timerSteps)) && (v.completedTimers === undefined || steps(v.completedTimers) && v.completedTimers.every(i => Array.isArray(v.timerSteps) && v.timerSteps.includes(i)))
    && (!v.finished || typeof v.finishedAt === 'number' && Number.isFinite(v.finishedAt) && v.finishedAt >= v.startedAt);
}
export function parseCookingJournal(raw: string | null): CookingJournal {
  if (raw === null) return empty();
  const value: unknown = JSON.parse(raw);
  if (!record(value) || value.version !== 1 || !record(value.sessions) || !record(value.history)
    || !Object.values(value.sessions).every(validSession)
    || !Object.entries(value.history).every(([id, run]) => record(run) && typeof run.recipeId === 'string' && validSession(run.session) && run.session.finished && run.session.runId === id)) throw new Error('Saved cooking progress is invalid.');
  return { version: 1, sessions: value.sessions as Record<string, SavedCookingSession>, history: value.history as Record<string, CompletedCook> };
}

/** One account, one atomic journal. Serialize reads and writes so a late step save cannot erase a finished cook. */
export function createCookingJournal(storage: Storage) {
  const pending = new Map<string, Promise<void>>();
  function enqueue<T>(userId: string, task: (key: string) => Promise<T>): Promise<T> {
    const key = `remi.cooking.journal.v1:${userId}`;
    const result = (pending.get(key) ?? Promise.resolve()).then(() => task(key));
    const tail = result.then(() => undefined, () => undefined); pending.set(key, tail);
    void tail.then(() => { if (pending.get(key) === tail) pending.delete(key); });
    return result;
  }
  return {
    load: (userId: string) => enqueue(userId, async key => parseCookingJournal(await storage.getItem(key))),
    save: (userId: string, recipeId: string, session: SavedCookingSession) => enqueue(userId, async key => {
      if (!validSession(session)) throw new Error('Invalid cooking progress.');
      const journal = parseCookingJournal(await storage.getItem(key));
      const previous = journal.sessions[recipeId];
      const progress = previous?.runId === session.runId && (previous.finished || previous.completed.length > session.completed.length) ? previous : session;
      const next: CookingJournal = { version: 1, sessions: { ...journal.sessions, [recipeId]: progress }, history: session.finished && !Object.hasOwn(journal.history, session.runId)
        ? { ...journal.history, [session.runId]: { recipeId, session } } : journal.history };
      await storage.setItem(key, JSON.stringify(next)); return next;
    }),
  };
}
export function cookingCareer(history: Record<string, CompletedCook>, currentRun?: string) {
  const runs = Object.values(history), current = currentRun ? history[currentRun] : undefined;
  return { cooks: runs.length, xp: runs.reduce((sum, run) => sum + cookingPoints(run.session), 0), recipes: new Set(runs.map(r => r.recipeId)).size,
    first: runs.length >= 1, five: runs.length >= 5, ten: runs.length >= 10,
    newRecipe: !!current && runs.filter(r => r.recipeId === current.recipeId).length === 1,
    timer: runs.some(r => !!r.session.completedTimers?.length) };
}
