// The targets of a client module: the remote servers it submits to or queries,
// each a section `<targets path>/<name>`. What a client page knows about its
// targets is a TargetSpec: the keys with their built-in defaults, how they are
// grouped, what weakens a target, and how to probe one.
//
// A target reads a key from its own section. A key the agent registers
// without a default then comes from the target it names as `parent`
// (`default` when it names none); a key registered with one falls back to
// that built-in default, whatever the parent says (typed_key::notify in
// include/nscapi/settings/helper.cpp). `address` is never inherited either:
// reading a target starts by resetting it. So `default` is the target used
// when none is named, and the template for the inheriting keys only.
import type { ReactNode } from "react";
import { Settings } from "../../api/api.ts";
import { readStored, SettingChange, toIni } from "./changes.ts";
import type { ModeOption } from "./modes.ts";
import type { Risk } from "./RiskBanner.tsx";
import type { ValueKind } from "./ValueField.tsx";

export const DEFAULT_TARGET = "default";

export interface TargetField {
  label: string;
  // A function for a choice that depends on the target (a stored value
  // outside the offered list is shown, not silently replaced).
  kind: ValueKind | "secret" | ((v: TargetValues) => ValueKind);
  help?: ReactNode;
  // What the agent uses while the target does not set it (and, for an
  // inheriting key, nor does its parent).
  builtIn: string;
  // Registered without a default, so a target takes its parent's value.
  inherits?: boolean;
  // Only shown while this holds for the target's values.
  visible?: (v: TargetValues) => boolean;
}

// A target's effective values: own, else inherited, else built in.
export type TargetValues = (key: string) => string;

// One choice that writes several keys of a target (NRPE's TLS modes, ...).
export interface TargetMode<M extends string = string> {
  label: string;
  options: ModeOption<M>[];
  // The mode the values are in, or null when they match none (Custom).
  detect: (v: TargetValues) => M | null;
  // The keys to write for a mode. Keys left out keep their value.
  apply: (mode: M, v: TargetValues) => Record<string, string>;
}

export interface TargetSection {
  title: string;
  mode?: TargetMode;
  // Shown above the fields, e.g. what the protocol protects and what not.
  note?: ReactNode;
  keys: string[];
}

export interface TargetTest {
  // The query to run, and its arguments for a target.
  query: string;
  args: (target: string) => string[];
  // What a passing probe proves, said next to the button.
  what: string;
}

export interface TargetSpec {
  moduleId: string;
  // The targets collection, e.g. /settings/NRPE/client/targets.
  path: string;
  // "NRPE server", "NSCA server" — what a target is.
  noun: string;
  fields: Record<string, TargetField>;
  // Connection and security first; every other key of `fields` that no
  // section names lands under Advanced.
  sections: TargetSection[];
  // Keys the page leaves to expert mode on purpose (never shown).
  expertOnly?: string[];
  risks: (v: TargetValues) => Risk[];
  // Why the values cannot be saved (the agent would refuse the target), or null.
  problem?: (v: TargetValues) => string | null;
  // One line for the list: how the target is protected.
  security: (v: TargetValues) => string;
  test?: TargetTest;
}

// The address a target connects to, as the list shows it. `address` is what
// the agent reads; `host` and `port` are older spellings of its parts.
export function endpoint(v: TargetValues): string {
  const address = v("address");
  if (address) return address;
  const host = v("host");
  const port = v("port");
  return host ? (port ? `${host}:${port}` : host) : "";
}

export function targetPath(spec: TargetSpec, name: string): string {
  return `${spec.path}/${name}`;
}

// The targets stored, `default` first. A target exists when its section
// holds a key, or the collection names it.
export function targetNames(stored: Settings[] | undefined, spec: TargetSpec): string[] {
  const prefix = spec.path + "/";
  const names = new Set<string>();
  for (const s of stored ?? []) {
    if (s.path === spec.path && s.key !== "") names.add(s.key);
    if (s.path.startsWith(prefix) && s.key !== "") {
      const rest = s.path.slice(prefix.length);
      if (!rest.includes("/")) names.add(rest);
    }
  }
  names.delete(DEFAULT_TARGET);
  return [DEFAULT_TARGET, ...[...names].sort((a, b) => a.localeCompare(b))];
}

// The value a target inherits for a key: its parent's, else the built-in.
export function inherited(
  stored: Settings[] | undefined,
  spec: TargetSpec,
  name: string,
  key: string,
  seen: Set<string> = new Set(),
): string {
  const builtIn = spec.fields[key]?.builtIn ?? "";
  if (name === DEFAULT_TARGET || !spec.fields[key]?.inherits) return builtIn;
  seen.add(name);
  const parent = readStored(stored, targetPath(spec, name), "parent") || DEFAULT_TARGET;
  if (seen.has(parent)) return builtIn;
  return effective(stored, spec, parent, key, seen);
}

export function effective(
  stored: Settings[] | undefined,
  spec: TargetSpec,
  name: string,
  key: string,
  seen: Set<string> = new Set(),
): string {
  return (
    readStored(stored, targetPath(spec, name), key) ?? inherited(stored, spec, name, key, seen)
  );
}

export function targetValues(
  stored: Settings[] | undefined,
  spec: TargetSpec,
  name: string,
  draft: Record<string, string> = {},
): TargetValues {
  return (key) => draft[key] ?? effective(stored, spec, name, key);
}

// The changes a draft makes to a target. A key set to what it would inherit
// anyway is not written.
export function targetChanges(
  stored: Settings[] | undefined,
  spec: TargetSpec,
  name: string,
  draft: Record<string, string>,
): SettingChange[] {
  const path = targetPath(spec, name);
  return Object.entries(draft).map(([key, value]) => ({
    path,
    key,
    value,
    defaultValue: inherited(stored, spec, name, key),
  }));
}

// Keys of a spec under Advanced: every field no section names.
export function advancedKeys(spec: TargetSpec): string[] {
  const named = new Set(spec.sections.flatMap((s) => s.keys));
  const hidden = new Set(spec.expertOnly ?? []);
  return Object.keys(spec.fields).filter((k) => !named.has(k) && !hidden.has(k));
}

// Every key a spec handles, for the registry's coverage test.
export function specKeys(spec: TargetSpec): string[] {
  return Object.keys(spec.fields);
}

// A target as an INI fragment: the module switched on, then the `default`
// target and the target itself (only what each stores, unless
// `includeDefaults` pins the effective value of every key).
export function targetIni(
  stored: Settings[] | undefined,
  spec: TargetSpec,
  name: string,
  includeDefaults: boolean,
): string {
  const section = (n: string): [string, Array<[string, string]>] => {
    const path = targetPath(spec, n);
    const rows: Array<[string, string]> = [];
    for (const key of [...Object.keys(spec.fields), "parent"]) {
      const own = readStored(stored, path, key);
      if (own !== undefined) rows.push([key, own]);
      else if (includeDefaults && n === name && key !== "parent") {
        const v = effective(stored, spec, n, key);
        if (v !== "") rows.push([key, v]);
      }
    }
    return [path, rows];
  };
  const sections = [section(name)];
  if (name !== DEFAULT_TARGET && !includeDefaults) {
    // Without defaults the target leans on `default`: carry that along.
    const d = section(DEFAULT_TARGET);
    if (d[1].length > 0) sections.unshift(d);
  }
  return toIni([["/modules", [[spec.moduleId, "enabled"]]], ...sections]);
}
