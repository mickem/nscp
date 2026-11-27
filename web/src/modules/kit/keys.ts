// A module page's keys as a table: for every section path, every key with the
// value the agent uses while it is not stored, and whether the agent reads it
// from [/settings/default] first (add_parent in the module's C++). The table
// drives reading a key, the changes an edit makes, and the INI fragment.
import { Settings } from "../../api/api.ts";
import {
  DEFAULT_SECTION,
  readScoped,
  readStored,
  Scope,
  scopedChanges,
  SettingChange,
  toIni,
  valueInScope,
} from "./changes.ts";

export interface KeySpec {
  // The agent's default; some depend on other stored keys.
  builtIn: (stored: Settings[] | undefined) => string;
  // Falls back to [/settings/default] when the module's own section leaves
  // it out.
  inherits?: boolean;
  // The agent works the default out at run time (a host name, ...): the INI
  // fragment leaves the key out rather than pin a value the page cannot know.
  computedDefault?: boolean;
}

export const fixed = (v: string) => (): string => v;

// path -> key -> spec, in the order the INI fragment lists them.
export type KeyTable = Record<string, Record<string, KeySpec>>;

// A key's value and, for a key that can inherit, where it is kept.
export interface FieldValue {
  value: string;
  scope?: Scope;
}

export function readKey(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  spec: KeySpec,
): FieldValue {
  if (spec.inherits) return readScoped(stored, path, key, spec.builtIn(stored));
  return { value: readStored(stored, path, key) ?? spec.builtIn(stored) };
}

export function keyChanges(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  spec: KeySpec,
  next: FieldValue,
): SettingChange[] {
  if (spec.inherits && next.scope) {
    return scopedChanges(path, key, { scope: next.scope, value: next.value }, spec.builtIn(stored));
  }
  return [{ path, key, value: next.value, defaultValue: spec.builtIn(stored) }];
}

// The value a key shows when the reader moves it to `scope`.
export function keyInScope(
  stored: Settings[] | undefined,
  path: string,
  key: string,
  spec: KeySpec,
  scope: Scope,
  shown: string,
): string {
  return valueInScope(stored, path, key, spec.builtIn(stored), scope, shown);
}

// The module's configuration as an INI fragment: the module switched on, the
// shared keys it reads from [/settings/default], then each of its sections,
// then `extra` sections (another module's key a page sets, ...). Every key is
// listed in the section the agent reads it from. `includeDefaults` adds the
// keys left at the agent's default, so the fragment also pins them on an agent
// whose defaults differ.
export function tableIni(
  stored: Settings[] | undefined,
  moduleId: string,
  table: KeyTable,
  includeDefaults: boolean,
  extra: Array<[string, Array<[string, string]>]> = [],
): string {
  const shared: Array<[string, string]> = [];
  const sections: Array<[string, Array<[string, string]>]> = [];
  for (const [path, keys] of Object.entries(table)) {
    const own: Array<[string, string]> = [];
    for (const [key, spec] of Object.entries(keys)) {
      const local = readStored(stored, path, key);
      const common = spec.inherits ? readStored(stored, DEFAULT_SECTION, key) : undefined;
      if (local !== undefined) own.push([key, local]);
      else if (common !== undefined) shared.push([key, common]);
      else if (includeDefaults && !spec.computedDefault)
        (spec.inherits ? shared : own).push([key, spec.builtIn(stored)]);
    }
    sections.push([path, own]);
  }
  return toIni([
    ["/modules", [[moduleId, "enabled"]]],
    [DEFAULT_SECTION, shared],
    ...sections,
    ...extra,
  ]);
}

// Every key of a table, as handledKeys wants them.
export function tableKeys(table: KeyTable): Record<string, string[]> {
  return Object.fromEntries(Object.entries(table).map(([path, keys]) => [path, Object.keys(keys)]));
}
