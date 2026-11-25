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
  // Write it even when it matches what is in effect: the reader chose to pin
  // the value in this section.
  pin?: boolean;
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
    if (current === undefined) return c.pin === true || c.value !== (c.defaultValue ?? "");
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

// Server modules read many keys from their own section first and fall back to
// /settings/default (add_parent in the agent): bind to, allowed hosts, timeout,
// thread pool, ...
export const DEFAULT_SECTION = "/settings/default";

export interface Inherited {
  value: string;
  // Where the value in effect comes from: the module's own path, the shared
  // default section, or the agent's built-in default.
  source: "own" | "default section" | "built-in";
}

export function readInherited(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  builtIn: string,
  inherits = true,
): Inherited {
  const own = readStored(stored, path, key);
  if (own !== undefined) return { value: own, source: "own" };
  if (inherits) {
    const shared = readStored(stored, DEFAULT_SECTION, key);
    if (shared !== undefined) return { value: shared, source: "default section" };
  }
  return { value: builtIn, source: "built-in" };
}

// A change to `key` on the module's own path. Writing the value already in
// effect, wherever it comes from, changes nothing and is left out; a module's
// page never writes the shared section.
export function inheritedChange(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  value: string,
  builtIn: string,
  inherits = true,
): SettingChange {
  const current = readInherited(stored, path, key, builtIn, inherits);
  return {
    path,
    key,
    value,
    defaultValue: current.source === "own" ? undefined : current.value,
  };
}

// Where an inheritable key is kept: the shared default section, used by every
// module that does not set its own, or the module's own section.
export type Scope = "default" | "local";

export interface ScopedValue {
  scope: Scope;
  value: string;
}

// The scope and value in effect: local when the module stores its own value,
// otherwise the shared one (or the agent's built-in default).
export function readScoped(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  builtIn: string,
): ScopedValue {
  const own = readStored(stored, path, key);
  if (own !== undefined) return { scope: "local", value: own };
  return { scope: "default", value: readStored(stored, DEFAULT_SECTION, key) ?? builtIn };
}

// The value a key shows when the reader switches it to `scope`.
export function valueInScope(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  builtIn: string,
  scope: Scope,
  shown: string,
): string {
  if (scope === "default") return readStored(stored, DEFAULT_SECTION, key) ?? builtIn;
  // Going local starts from the module's own value if it has one, else from
  // what was shown, so the switch alone pins the value in effect.
  return readStored(stored, path, key) ?? shown;
}

// The changes that keep `next` in its scope. Local writes the module's own
// section; default writes the shared section and drops the module's own value
// so the shared one applies.
export function scopedChanges(
  path: string,
  key: string,
  next: ScopedValue,
  builtIn: string,
): SettingChange[] {
  if (next.scope === "local") return [{ path, key, value: next.value, pin: true }];
  return [
    { path, key, value: null },
    { path: DEFAULT_SECTION, key, value: next.value, defaultValue: builtIn },
  ];
}

// What the stored settings would be after `changes`: a value replaces, null
// removes. For previews such as the INI fragment of a page.
export function applyToStored(
  stored: Settings[] | undefined,
  changes: SettingChange[],
): Settings[] {
  let out = [...(stored ?? [])];
  for (const c of changes) {
    out = out.filter((s) => !(s.path === c.path && s.key === c.key));
    if (c.value !== null) out.push({ path: c.path, key: c.key, value: c.value });
  }
  return out;
}

// An INI fragment: sections in the order given, keys in the order given.
export function toIni(sections: Array<[string, Array<[string, string]>]>): string {
  return sections
    .filter(([, keys]) => keys.length > 0)
    .map(([path, keys]) => [`[${path}]`, ...keys.map(([k, v]) => `${k} = ${v}`)].join("\n"))
    .join("\n\n");
}
