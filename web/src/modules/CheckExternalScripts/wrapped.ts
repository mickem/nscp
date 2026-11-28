// Wrapped scripts (modules/CheckExternalScripts): `name = script.ext args`,
// run through the wrapping registered for the script's extension.
// script_provider::generate_wrapped_command splits the value at its first
// space, takes the text after the script's last dot as the extension (case
// sensitive), and puts the script in for %SCRIPT% and the rest for %ARGS%.
// $ARGS"$ and the like stay, for the caller's arguments.
import { Settings } from "../../api/api.ts";
import { EXT_WRAPPED, EXT_WRAPPINGS } from "./ext.ts";

// Registered when the settings leave them out (CheckExternalScripts.cpp).
export const BUILT_IN_WRAPPINGS: Record<string, string> = {
  ps1: 'powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -File "scripts\\%SCRIPT%" $ARGS"$',
  vbs: 'cscript.exe //T:30 //NoLogo "scripts\\lib\\wrapper.vbs" %SCRIPT% $ARGS"$',
  bat: '"scripts\\%SCRIPT%" $ARGS"$',
};

const keyValues = (stored: Settings[] | undefined, path: string) => {
  const out = new Map<string, string>();
  for (const s of stored ?? []) if (s.path === path && s.key !== "") out.set(s.key, s.value);
  return out;
};

// Every wrapping in effect: the stored ones, and the built-in ones they leave.
export function readWrappings(
  stored: Settings[] | undefined,
): Array<{ ext: string; line: string; builtIn: boolean; overridden: boolean }> {
  const own = keyValues(stored, EXT_WRAPPINGS);
  const exts = new Set([...Object.keys(BUILT_IN_WRAPPINGS), ...own.keys()]);
  return [...exts].sort().map((ext) => ({
    ext,
    line: own.get(ext) ?? BUILT_IN_WRAPPINGS[ext],
    builtIn: ext in BUILT_IN_WRAPPINGS,
    // The agent writes the built-in ones back as static keys, so a stored
    // copy of the built-in line is not an override.
    overridden:
      own.has(ext) && ext in BUILT_IN_WRAPPINGS && own.get(ext) !== BUILT_IN_WRAPPINGS[ext],
  }));
}

export function readWrapped(stored: Settings[] | undefined): Array<[string, string]> {
  return [...keyValues(stored, EXT_WRAPPED).entries()].sort(([a], [b]) => a.localeCompare(b));
}

export function splitWrapped(value: string): { script: string; args: string } {
  const at = value.indexOf(" ");
  return at < 0
    ? { script: value, args: "" }
    : { script: value.slice(0, at), args: value.slice(at + 1) };
}

export function joinWrapped(script: string, args: string): string {
  return args !== "" ? `${script.trim()} ${args}` : script.trim();
}

export function extensionOf(script: string): string {
  const at = script.lastIndexOf(".");
  return at < 0 ? "none" : script.slice(at + 1);
}

// The command line the agent builds, or null when no wrapping matches.
export function wrappedCommand(
  value: string,
  wrappings: Array<{ ext: string; line: string }>,
): { ext: string; command: string | null } {
  const { script, args } = splitWrapped(value);
  const ext = extensionOf(script);
  const tpl = wrappings.find((w) => w.ext === ext)?.line;
  return {
    ext,
    command: tpl ? tpl.split("%SCRIPT%").join(script).split("%ARGS%").join(args) : null,
  };
}
