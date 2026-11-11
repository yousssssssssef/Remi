import { useCallback, useEffect, useRef, useState } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { useAuth } from '@/providers/auth-provider';
import { useBackendCookingSession } from '@/providers/backend-recipes-provider';
import { getAuth } from '@/services/auth';
import { operationKey } from '@/services/kitchen-api';
import { createCookingJournal, type CompletedCook, type SavedCookingSession } from '@/services/cooking-journal';
import { initialCookingSession, type CookingSession } from '@/utils/cooking-session';

const journal = createCookingJournal(AsyncStorage);
interface State { scope: string; status: 'ready' | 'error' | 'unsaved'; history: Record<string, CompletedCook>; warning: boolean }
export function useCookingJourney(id: string, total?: number) {
  const { session, update: memoryUpdate } = useBackendCookingSession(id);
  const auth = useAuth(), userId = auth.user?.id;
  const revision = getAuth().getIdentityRevision(), scope = `${userId}:${revision}:${id}:${total}`;
  const [state, setState] = useState<State | null>(null), [attempt, setAttempt] = useState(0);
  const activeScope = useRef<string | null>(scope);
  useEffect(() => { activeScope.current = scope; return () => { activeScope.current = null; }; }, [scope]);
  const ready = state?.scope === scope && (state.status === 'ready' || state.status === 'unsaved');
  const fresh = useCallback((): SavedCookingSession => ({ ...initialCookingSession, runId: operationKey(), startedAt: Date.now(), totalSteps: total! }), [total]);
  const save = useCallback((session: SavedCookingSession) => {
    if (!userId) return;
    void journal.save(userId, id, session).then(saved => {
      if (activeScope.current === scope) setState(s => s?.scope === scope ? { ...s, status: 'ready', history: saved.history, warning: false } : s);
    }).catch(() => { if (activeScope.current === scope) setState(s => s?.scope === scope ? { ...s, warning: true } : s); });
  }, [userId, id, scope]);
  useEffect(() => {
    if (!userId || auth.busy || !total) return;
    let live = true;
    void journal.load(userId).then(saved => {
      if (!live || getAuth().getIdentityRevision() !== revision) return;
      const stored = Object.hasOwn(saved.sessions, id) ? saved.sessions[id] : undefined;
      const existing = session.runId ? session : stored;
      const restored = existing?.totalSteps === total ? existing : fresh();
      memoryUpdate(() => restored);
      setState({ scope, status: 'ready', history: saved.history, warning: false });
      save(restored as SavedCookingSession);
    }).catch(() => { if (live) setState({ scope, status: 'error', history: {}, warning: true }); });
    return () => { live = false; };
    // The scope is the hydration boundary. Local progress changes must not reload older storage.
  }, [scope, attempt, userId, auth.busy, total, revision, id, fresh, memoryUpdate, save]); // eslint-disable-line react-hooks/exhaustive-deps
  const update = useCallback((change: (session: CookingSession) => CookingSession) => {
    if (!ready || auth.busy || !total) return;
    memoryUpdate(current => {
      const next = change(current); if (next === current) return current;
      const base = next.runId ? next : { ...next, ...fresh() };
      const stamped: SavedCookingSession = { ...base, runId: base.runId!, startedAt: base.startedAt!, totalSteps: total,
        finishedAt: next.finished ? current.finishedAt ?? Math.max(Date.now(), base.startedAt!) : undefined };
      if (state.status !== 'unsaved') save(stamped);
      return stamped;
    });
  }, [ready, auth.busy, total, memoryUpdate, fresh, state, save]);
  const continueUnsaved = () => { memoryUpdate(s => s.runId ? s : fresh()); setState({ scope, status: 'unsaved', history: {}, warning: true }); };
  return { session: session, update, restoring: !!userId && !!total && !ready, restoreError: state?.scope === scope && state.status === 'error',
    history: ready ? state.history : {}, saveWarning: ready && state.warning,
    retry: () => { if (ready) save(session as SavedCookingSession); else setAttempt(n => n + 1); }, continueUnsaved };
}
