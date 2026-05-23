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
