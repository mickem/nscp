// Settings changes as data.
//
// A module page's modes and presets are recipes: plain functions that return
// the changes to write, and never save anything themselves. The page's own
// controls call them, and so can anything that composes several modules'
// changes into one summary (the first-run wizard this work is heading to).
import { useCallback } from "react";
import { Settings, useDeleteSettingsMutation, useUpdateSettingsMutation } from "../../api/api.ts";

export interface SettingChange {
  path: string;
  key: string;
  // The value to store, or null to remove the key so the agent's default
  // applies again.
  value: string | null;
  // What the agent uses while the key is not stored. Writing exactly this to
  // a key that is not stored changes nothing, so it is left out.
  defaultValue?: string;
}

// The stored value of a key, or undefined when it is not stored. /v2/settings
// can list a key more than once; the last one wins, as in the agent.
export function readStored(
  stored: Settings[] | undefined,
  path: string,
  key: string,
): string | undefined {
  let value: string | undefined;
  for (const s of stored ?? []) {
    if (s.path === path && s.key === key) value = s.value;
  }
  return value;
}

// The changes that would actually change something, last one per key wins.
export function pendingChanges(
  stored: Settings[] | undefined,
  changes: SettingChange[],
): SettingChange[] {
  const byKey = new Map<string, SettingChange>();
  for (const c of changes) byKey.set(`${c.path}\u0000${c.key}`, c);
  return [...byKey.values()].filter((c) => {
    const current = readStored(stored, c.path, c.key);
    if (c.value === null) return current !== undefined;
    if (current === undefined) return c.value !== (c.defaultValue ?? "");
    return current !== c.value;
  });
}

export function describeChange(c: SettingChange): string {
  return c.value === null
    ? `[${c.path}] ${c.key}: reset to default`
    : `[${c.path}] ${c.key} = ${c.value}`;
}

// Writes changes one at a time, in order. Rejects on the first failure; the
// changes before it stay written.
export function useApplyChanges(): (changes: SettingChange[]) => Promise<void> {
  const [updateSettings] = useUpdateSettingsMutation();
  const [deleteSettings] = useDeleteSettingsMutation();
  return useCallback(
    async (changes: SettingChange[]) => {
      for (const c of changes) {
        if (c.value === null) await deleteSettings({ path: c.path, key: c.key }).unwrap();
        else await updateSettings({ path: c.path, key: c.key, value: c.value }).unwrap();
      }
    },
    [updateSettings, deleteSettings],
  );
}

// Loading a module is part of most recipes: a page that configures a module
// also switches it on.
export function enableModuleChange(moduleId: string): SettingChange {
  return { path: "/modules", key: moduleId, value: "enabled" };
}

export function isEnabledValue(value: string | undefined): boolean {
  const v = (value ?? "").toLowerCase();
  return v === "enabled" || v === "true" || v === "1";
}
