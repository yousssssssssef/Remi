export interface CookingSession { current: number; completed: readonly number[]; finished: boolean; timerSteps?: readonly number[]; completedTimers?: readonly number[]; runId?: string; startedAt?: number; finishedAt?: number; totalSteps?: number }
export const initialCookingSession: CookingSession = { current: 0, completed: [], finished: false };
export function cookingInstructions(catalog: Record<string, unknown>, id: string): readonly string[] {
  const steps = catalog[id];
  return Array.isArray(steps) && steps.length > 0 && steps.every(step => typeof step === 'string' && step.trim()) ? steps : [];
}
export function advanceCooking(session: CookingSession, total: number): CookingSession {
  if (session.finished || total < 1) return session;
  const completed = [...new Set([...session.completed, session.current])];
  const finished = completed.length === total;
  const next = session.current < total - 1 ? session.current + 1 : Array.from({ length: total }, (_, i) => i).find(i => !completed.includes(i)) ?? total - 1;
  return { ...session, current: next, completed, finished, totalSteps: total };
}
export function selectCookingStep(session: CookingSession, index: number, total: number): CookingSession {
  if (session.finished || !Number.isInteger(index) || index < 0 || index >= total) return session;
  return { ...session, current: index };
}

/** Points are derived from unique completed steps, so reviewing a step cannot farm rewards. */
export function cookingPoints(session: CookingSession): number {
  return new Set(session.completed).size * 10 + (session.finished ? 50 : 0);
}
export function recordCookingTimer(session: CookingSession, step: number): CookingSession {
  if (session.finished || session.timerSteps?.includes(step)) return session;
  return { ...session, timerSteps: [...(session.timerSteps ?? []), step] };
}

export function completeCookingTimer(session: CookingSession, step: number): CookingSession {
  if (session.finished || !session.timerSteps?.includes(step) || session.completedTimers?.includes(step)) return session;
  return { ...session, completedTimers: [...(session.completedTimers ?? []), step] };
}
