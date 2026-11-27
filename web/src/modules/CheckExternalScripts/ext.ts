// What the External scripts page knows about CheckExternalScripts' settings
// (modules/CheckExternalScripts/CheckExternalScripts.cpp).
import { Settings } from "../../api/api.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const EXT_MODULE = "CheckExternalScripts";
export const EXT_PATH = "/settings/external scripts";
export const EXT_SCRIPTS = `${EXT_PATH}/scripts`;
export const EXT_ALIAS = `${EXT_PATH}/alias`;
export const EXT_WRAPPINGS = `${EXT_PATH}/wrappings`;
export const EXT_WRAPPED = `${EXT_PATH}/wrapped scripts`;

export const EXT_TABLE: KeyTable = {
  [EXT_PATH]: {
    "allow arguments": { builtIn: fixed("false") },
    "allow nasty characters": { builtIn: fixed("false") },
    timeout: { builtIn: fixed("60") },
    "kill tree": { builtIn: fixed("false") },
    "script path": { builtIn: fixed("") },
    // include/scripts/script_root_setting.hpp reads it with a parent.
    "script root": { builtIn: fixed(""), inherits: true },
  },
};

const on = (v: string) => v.toLowerCase() === "true" || v === "1";

export function argumentsRisk(allowArguments: string, allowNasty: string): Risk | null {
  if (!on(allowArguments)) return null;
  if (on(allowNasty))
    return {
      severity: "error",
      title: "Callers can pass shell characters to scripts",
      reason:
        "With arguments and | ` & > < ' \" \\ [ ] { } allowed, anyone who can run a check can make a script run commands of their choosing.",
    };
  return {
    severity: "warning",
    title: "Callers can pass arguments to scripts",
    reason:
      "Anyone who can run a check decides what a script gets as $ARG1$ and the rest. Prefer a fixed alias in Check helpers for each variant you need.",
  };
}

export function extIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, EXT_MODULE, EXT_TABLE, includeDefaults);
}

// --- Scripts ------------------------------------------------------------------

// A script's keys when it is a section (commands.hpp); none has a default, so
// a section takes what it leaves out from scripts/default.
export const SCRIPT_KEYS = [
  "command",
  "user",
  "domain",
  "password",
  "session",
  "display",
  "capture output",
  "ignore perfdata",
  "encoding",
] as const;

export interface Script {
  name: string;
  // `name = command` under the scripts path, or a section of its own.
  form: "line" | "section";
  values: Record<string, string>;
}

export function readScripts(stored: Settings[] | undefined): Script[] {
  const byName = new Map<string, Script>();
  const prefix = EXT_SCRIPTS + "/";
  for (const s of stored ?? []) {
    if (s.path === EXT_SCRIPTS && s.key !== "") {
      const cur = byName.get(s.key);
      if (cur?.form === "section") continue;
      byName.set(s.key, { name: s.key, form: "line", values: { command: s.value } });
    } else if (s.path.startsWith(prefix) && s.key !== "") {
      const name = s.path.slice(prefix.length);
      if (name.includes("/") || name === "default") continue;
      const cur = byName.get(name);
      const values = cur?.form === "section" ? cur.values : {};
      values[s.key] = s.value;
      byName.set(name, { name, form: "section", values });
    }
  }
  return [...byName.values()].sort((a, b) => a.name.localeCompare(b.name));
}

// Whether a script needs a section: anything but its command is set.
export function needsSection(values: Record<string, string>): boolean {
  return Object.entries(values).some(([k, v]) => k !== "command" && v !== "");
}

export interface Write {
  kind: "put" | "delete" | "delete-path";
  path: string;
  key?: string;
  value?: string;
}

// What saving a script writes. A script whose options are all empty is kept
// as (or turned into) a one-line entry; one with options becomes a section.
export function scriptWrites(
  original: Script | null,
  name: string,
  values: Record<string, string>,
): Write[] {
  const out: Write[] = [];
  const section = needsSection(values) || (original?.form === "section" && original.name === name);
  const renamed = original !== null && original.name !== name;
  if (original && (renamed || original.form !== (section ? "section" : "line"))) {
    out.push(
      original.form === "line"
        ? { kind: "delete", path: EXT_SCRIPTS, key: original.name }
        : { kind: "delete-path", path: `${EXT_SCRIPTS}/${original.name}` },
    );
  }
  if (!section) {
    out.push({ kind: "put", path: EXT_SCRIPTS, key: name, value: values.command ?? "" });
    return out;
  }
  const path = `${EXT_SCRIPTS}/${name}`;
  for (const key of SCRIPT_KEYS) {
    const v = values[key] ?? "";
    const before =
      original?.name === name && original.form === "section" ? original.values[key] : undefined;
    if (v !== "" && v !== before) out.push({ kind: "put", path, key, value: v });
    else if (v === "" && before !== undefined) out.push({ kind: "delete", path, key });
  }
  return out;
}
