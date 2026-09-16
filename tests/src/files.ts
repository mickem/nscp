import * as fs from "fs";
import * as path from "path";

/**
 * Read every regular file under `dir` (recursive) and return their
 * contents concatenated. Replaces `findstr /s` over a spool directory.
 */
export function readAllUnder(dir: string): string {
  if (!fs.existsSync(dir)) return "";
  const out: string[] = [];
  walk(dir, (file) => {
    try {
      out.push(fs.readFileSync(file, "utf8"));
    } catch {
      /* ignore binary / unreadable files */
    }
  });
  return out.join("\n");
}

/** True if any file under `dir` (recursive) contains `needle` literally. */
export function anyFileContains(dir: string, needle: string): boolean {
  let found = false;
  walk(dir, (file) => {
    if (found) return;
    try {
      if (fs.readFileSync(file, "utf8").includes(needle)) found = true;
    } catch {
      /* ignore */
    }
  });
  return found;
}

/** True if `file` contains `needle` literally. */
export function fileContains(file: string, needle: string): boolean {
  if (!fs.existsSync(file)) return false;
  return fs.readFileSync(file, "utf8").includes(needle);
}

function walk(dir: string, visit: (file: string) => void): void {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, entry.name);
    if (entry.isDirectory()) walk(p, visit);
    else if (entry.isFile()) visit(p);
  }
}

const onWindows = process.platform === "win32";

/**
 * Write a helper script for the *agent under test* and return the command
 * line to register it under `/settings/external scripts/scripts`.
 *
 * The suites that need one (the Mod-Gearman core and proxy suites) run their
 * containers on docker but the agent itself on the host, so the script has to
 * be in the host's own flavour: a `.bat` driven by `cmd /c` on Windows, a
 * `#!/bin/sh` file driven by `/bin/sh` elsewhere. Writing a `/bin/sh` script
 * unconditionally is the bug this replaces - it fails on Windows with
 * "The system cannot find the file specified", which arrives at the core as a
 * plain UNKNOWN and reads like an agent fault.
 */
function writeScript(dir: string, name: string, windows: string, posix: string): string {
  if (onWindows) {
    const file = path.join(dir, `${name}.bat`);
    fs.writeFileSync(file, `@echo off\r\n${windows.replace(/\n/g, "\r\n")}\r\n`);
    return `cmd /c ${file}`;
  }
  const file = path.join(dir, `${name}.sh`);
  fs.writeFileSync(file, `#!/bin/sh\n${posix}\n`, { mode: 0o755 });
  return `/bin/sh ${file}`;
}

/** A script that prints `text` and exits 0. */
export function writeEchoScript(dir: string, name: string, text: string): string {
  return writeScript(dir, name, `echo ${text}`, `echo '${text}'`);
}

/**
 * A script that blocks for `seconds` before printing `text`. `ping` is the
 * batch-file sleep that needs no console; `-n` counts pings, not waits, so it
 * is one more than the seconds wanted.
 */
export function writeSleepScript(dir: string, name: string, seconds: number, text: string): string {
  return writeScript(
    dir,
    name,
    `ping -n ${seconds + 1} 127.0.0.1 >nul\necho ${text}`,
    `sleep ${seconds}\necho '${text}'`,
  );
}
