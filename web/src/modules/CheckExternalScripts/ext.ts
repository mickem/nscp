// What the External scripts page knows about CheckExternalScripts' settings
// (modules/CheckExternalScripts/CheckExternalScripts.cpp).
import { Settings } from "../../api/api.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import { Entry, entryWrites, readEntries, Write } from "../kit/entries.ts";

export type { Write };

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

export type Script = Entry;

export function readScripts(stored: Settings[] | undefined): Script[] {
  return readEntries(stored, EXT_SCRIPTS, "command");
}

// A script with nothing but its command is one line; with options, a section.
export function scriptWrites(
  original: Script | null,
  name: string,
  values: Record<string, string>,
): Write[] {
  return entryWrites(EXT_SCRIPTS, SCRIPT_KEYS, "command", original, name, values);
}
