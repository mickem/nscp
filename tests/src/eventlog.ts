/**
 * Shared Windows-event-log injection helpers for the CheckEventLog integration
 * suites.
 *
 * Events are written through .NET's `EventLog.WriteEntry` (via powershell)
 * rather than `eventcreate`, because WriteEntry works WITHOUT elevation as long
 * as the source is already registered — whereas `eventcreate` needs
 * Administrator to register a fresh source. The trade-offs of reusing a
 * pre-existing source:
 *   - Events are disambiguated by a per-run id range (well above real system
 *     event ids), not by a unique source.
 *   - `${message}` is not queryable for these events (the source's message DLL
 *     has no template for our ids), so filters must key off `id` / `source` /
 *     `level`, not message text.
 *
 * If no writable source can be found, `resolveEventLogSource()` throws so the
 * caller's `beforeAll` fails loudly instead of letting a suite silently skip
 * into a false pass.
 */
import { execFileSync } from "node:child_process";

/** WriteEntry severity, matching System.Diagnostics.EventLogEntryType. */
export type EventLevel = "Information" | "Warning" | "Error";

/** Escape for a single-quoted PowerShell literal. */
function psq(s: string): string {
  return "'" + s.replace(/'/g, "''") + "'";
}

/**
 * Write one event to the Application log via EventLog.WriteEntry. Only writes to
 * an ALREADY-registered source (checked first) so it never needs elevation and
 * never registers a stray source. Returns true on success.
 */
export function writeEventLogEntry(source: string, id: number, level: EventLevel, message: string): boolean {
  const script =
    `$ErrorActionPreference='Stop';` +
    `try{if(-not [System.Diagnostics.EventLog]::SourceExists(${psq(source)})){exit 2}}catch{exit 2};` +
    `try{[System.Diagnostics.EventLog]::WriteEntry(${psq(source)},${psq(message)},` +
    `[System.Diagnostics.EventLogEntryType]::${level},${id});exit 0}catch{exit 1}`;
  try {
    execFileSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", script], { stdio: "ignore" });
    return true;
  } catch {
    return false;
  }
}

/**
 * First pre-registered Application-log source we can write to without elevation.
 * Throws (so the caller's beforeAll fails, never silently skips) when injection
 * is impossible in this environment.
 */
export function resolveEventLogSource(): string {
  const candidates = [".NET Runtime", "Windows Error Reporting", "Application Error", "MsiInstaller"];
  for (const src of candidates) {
    if (writeEventLogEntry(src, 1, "Information", "nscp eventlog test probe")) return src;
  }
  throw new Error(
    "CheckEventLog tests: cannot inject Application-log events here. eventcreate needs " +
      "Administrator to register a source, and none of the pre-registered fallback sources " +
      `(${candidates.join(", ")}) are writable. Run elevated or on a host with .NET Framework installed.`,
  );
}

export interface EventIds {
  /** Lower bound of this run's id range — use `id >= base` to match our events. */
  readonly base: number;
  /** A fresh, unique event id within this run's range. */
  next(): number;
}

/**
 * A per-run event-id allocator based well above real system event ids, so
 * filters on this range never match genuine system events and separate runs
 * (different random base) don't collide. The base lands in
 * `[minBase, minBase + 8999]`, so callers that share one event log (the two
 * CheckEventLog suites) can pass distinct `minBase` values a decade apart to get
 * non-overlapping id bands and avoid matching each other's events.
 */
export function eventIdAllocator(minBase = 40000): EventIds {
  const base = minBase + Math.floor(Math.random() * 9000);
  let seq = 0;
  return { base, next: () => base + ++seq };
}
