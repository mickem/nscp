// The stored sections under a page's lists (scripts, schedules, targets,
// ...), as an INI fragment for Show config.
import { Settings } from "../../api/api.ts";
import { toIni } from "./changes.ts";

export function listsIni(stored: Settings[], paths: string[]): string {
  const byPath = new Map<string, Array<[string, string]>>();
  for (const s of stored) {
    if (s.key === "" || !paths.some((p) => s.path === p || s.path.startsWith(p + "/"))) continue;
    byPath.set(s.path, [...(byPath.get(s.path) ?? []), [s.key, s.value]]);
  }
  return toIni([...byPath.entries()].sort(([a], [b]) => a.localeCompare(b)));
}
