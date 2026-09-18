/**
 * Reads the `module.json` manifests out of the source tree.
 *
 * The manifest is the single source of truth for what a module declares about
 * itself, so a test that checks how a declaration is *reported* should read it
 * from here rather than restate it. That matters most for the `experimental`
 * flag: it is expected to move — a command carries it until its options and
 * output settle, then loses it — and a test that hard-codes which commands
 * carry it today fails the day someone clears one, for no reason other than
 * having written the answer down twice.
 */
import * as fs from "fs";
import * as path from "path";

/** <repo>/modules, next to this test suite. */
const MODULES_DIR = path.resolve(__dirname, "..", "..", "modules");

export interface ModuleManifest {
  /** Directory (and module) name, e.g. "CheckDisk". */
  name: string;
  /** The module itself declares `"experimental": true`. */
  experimental: boolean;
  /** Command name -> whether that command declares itself experimental. */
  commands: Map<string, boolean>;
  /** The same, keyed by the lower-cased name the registry reports. */
  byLowerName: Map<string, boolean>;
}

interface RawCommand {
  experimental?: boolean;
}

function parse(name: string, text: string): ModuleManifest {
  const raw = JSON.parse(text) as {
    module?: { experimental?: boolean };
    commands?: Record<string, string | RawCommand>;
  };
  const commands = new Map<string, boolean>();
  for (const [command, value] of Object.entries(raw.commands ?? {})) {
    // "fallback" is a dispatch directive, not a command.
    if (command === "fallback") continue;
    commands.set(command, typeof value === "object" && value?.experimental === true);
  }
  // The registry keys commands case-insensitively and reports the lower-cased
  // key, which is how the legacy `checkDriveSize` spellings come back.
  const byLowerName = new Map<string, boolean>();
  for (const [command, experimental] of commands) byLowerName.set(command.toLowerCase(), experimental);
  return { name, experimental: raw.module?.experimental === true, commands, byLowerName };
}

/** Every module.json in the source tree, keyed by module name. */
export function moduleManifests(): Map<string, ModuleManifest> {
  const out = new Map<string, ModuleManifest>();
  for (const entry of fs.readdirSync(MODULES_DIR)) {
    const file = path.join(MODULES_DIR, entry, "module.json");
    if (!fs.existsSync(file)) continue;
    try {
      out.set(entry, parse(entry, fs.readFileSync(file, "utf8")));
    } catch (e) {
      throw new Error(`${file} is not readable as a module manifest: ${e}`);
    }
  }
  if (out.size === 0) throw new Error(`no module manifests found under ${MODULES_DIR}`);
  return out;
}

/** One module's manifest. Throws when the module has none. */
export function moduleManifest(name: string): ModuleManifest {
  const manifest = moduleManifests().get(name);
  if (!manifest) throw new Error(`no module.json for ${name}`);
  return manifest;
}

/** The commands `manifest` declares experimental (or, with `false`, the rest). */
export function commandsDeclaring(manifest: ModuleManifest, experimental: boolean): string[] {
  return [...manifest.commands.entries()]
    .filter(([, value]) => value === experimental)
    .map(([command]) => command)
    .sort();
}
