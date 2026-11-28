// The state of a module page: the stored settings, a draft of the keys the
// reader changed, and saving. A page's modes keep their own draft and hand
// their changes to pending(); everything is written together.
import { useCallback, useState } from "react";
import { Settings, useGetSettingsQuery } from "../../api/api.ts";
import {
  enableModuleChange,
  pendingChanges,
  Scope,
  SettingChange,
  useApplyChanges,
} from "./changes.ts";
import { FieldValue, KeySpec, KeyTable, keyChanges, keyInScope, readKey } from "./keys.ts";

export interface Panel {
  moduleId: string;
  stored: Settings[] | undefined;
  busy: boolean;
  saved: boolean;
  error: string | null;
  // A key as the page shows it: the draft if edited, else what is in effect.
  get: (path: string, key: string) => FieldValue;
  value: (path: string, key: string) => string;
  // What is in effect now, ignoring the draft.
  current: (path: string, key: string) => FieldValue;
  set: (path: string, key: string, value: string) => void;
  setScope: (path: string, key: string, scope: Scope) => void;
  // Every change the draft and `extra` make, plus switching the module on,
  // leaving out what changes nothing.
  pending: (extra?: SettingChange[]) => SettingChange[];
  save: (changes: SettingChange[]) => Promise<void>;
  discard: () => void;
}

export function usePanel(moduleId: string, keys: KeyTable, onReset?: () => void): Panel {
  const { data: stored } = useGetSettingsQuery();
  const applyChanges = useApplyChanges();
  const [edits, setEdits] = useState<Record<string, FieldValue>>({});
  const [busy, setBusy] = useState(false);
  const [saved, setSaved] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const spec = (path: string, key: string): KeySpec => {
    const s = keys[path]?.[key];
    if (!s) throw new Error(`No key ${key} under ${path} in this page's table`);
    return s;
  };
  const id = (path: string, key: string) => `${path}\u0000${key}`;
  const current = (path: string, key: string) => readKey(stored, path, key, spec(path, key));
  const get = (path: string, key: string) => edits[id(path, key)] ?? current(path, key);
  const put = (path: string, key: string, next: FieldValue) => {
    setSaved(false);
    setEdits((e) => ({ ...e, [id(path, key)]: next }));
  };

  const pending = (extra: SettingChange[] = []) => {
    const out: SettingChange[] = [];
    for (const [k, next] of Object.entries(edits)) {
      const [path, key] = k.split("\u0000");
      out.push(...keyChanges(stored, path, key, spec(path, key), next));
    }
    const changes = pendingChanges(stored, [...out, ...extra]);
    // Configuring a module switches it on.
    return changes.length > 0
      ? pendingChanges(stored, [...changes, enableModuleChange(moduleId)])
      : changes;
  };

  const discard = useCallback(() => {
    setEdits({});
    onReset?.();
  }, [onReset]);

  return {
    moduleId,
    stored,
    busy,
    saved,
    error,
    get,
    value: (path, key) => get(path, key).value,
    current,
    set: (path, key, value) => put(path, key, { ...get(path, key), value }),
    setScope: (path, key, scope) =>
      put(path, key, {
        scope,
        value: keyInScope(stored, path, key, spec(path, key), scope, get(path, key).value),
      }),
    pending,
    save: async (changes) => {
      setError(null);
      setSaved(false);
      setBusy(true);
      try {
        await applyChanges(changes);
        setEdits({});
        onReset?.();
        setSaved(true);
      } catch (e) {
        setError(e instanceof Error ? e.message : String(e));
      } finally {
        setBusy(false);
      }
    },
    discard,
  };
}
