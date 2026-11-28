// What the disk page knows about CheckDisk's settings (modules/CheckDisk,
// include/check/access_policy.hpp).
import { fixed, KeyTable } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const DISK_MODULE = "CheckDisk";
export const DISK_PATH = "/settings/disk";
export const DISK_FILES = `${DISK_PATH}/files`;
export const DISK_DIAGNOSTICS = `${DISK_PATH}/diagnostics`;
export const DISK_FACTS = `${DISK_PATH}/facts`;

export const DISK_TABLE: KeyTable = {
  [DISK_PATH]: {
    "file access": { builtIn: fixed("any") },
    "allowed files": { builtIn: fixed("") },
    "collection interval": { builtIn: fixed("10s") },
    "trend interval": { builtIn: fixed("5m") },
    "trend retention": { builtIn: fixed("7d") },
    "max collection errors": { builtIn: fixed("10") },
    disable: { builtIn: fixed("") },
  },
  [DISK_DIAGNOSTICS]: {
    "growth spots": { builtIn: fixed("false") },
    "growth spots interval": { builtIn: fixed("10m") },
  },
  [DISK_FACTS]: {
    "storage.volumes": { builtIn: fixed("false") },
  },
};

export type FileAccess = "any" | "allowed" | "predefined";

// The agent's spellings of each mode; anything else fails closed to
// predefined, logging the error.
export function fileAccessOf(v: string): FileAccess | null {
  const s = v.trim().toLowerCase();
  if (["", "any", "all", "unrestricted"].includes(s)) return "any";
  if (["allowed", "allow", "list"].includes(s)) return "allowed";
  if (["predefined", "defined", "named"].includes(s)) return "predefined";
  return null;
}

export function accessRisk(mode: FileAccess | null, allowed: string): Risk | null {
  if (mode === null)
    return {
      severity: "warning",
      title: "The file access setting is not one the agent reads",
      reason: "The agent falls back to the named paths only, and logs the error. Pick a mode.",
    };
  if (mode === "any")
    return {
      severity: "warning",
      title: "Callers can describe any path",
      reason:
        "check_files, check_single_file, check_largest_files and check_disk_write take any path a caller names: they list whole directory trees with sizes and timestamps and can report checksums. Where callers pass arguments (NRPE with allow arguments, the REST API), restrict them to the paths you check.",
    };
  if (mode === "allowed" && allowed.trim() === "")
    return {
      severity: "warning",
      title: "No path is allowed",
      reason: "Every path that is not one of the named paths below is refused.",
    };
  return null;
}

// The collectors a caller can switch off, in the agent's spelling.
export const COLLECTORS: Array<{ id: string; label: string; help: string }> = [
  {
    id: "disk_io",
    label: "Disk I/O",
    help: "check_disk_io and check_disk_health have nothing to report.",
  },
  {
    id: "disk_free",
    label: "Disk free space",
    help: "No free-space history; turns off the trend as well.",
  },
  {
    id: "trend",
    label: "Used-space trend",
    help: "check_drivesize's full_in and rate keywords have nothing to report.",
  },
];

// The agent looks for each name anywhere in the list.
export function disabledCollectors(v: string): Set<string> {
  return new Set(COLLECTORS.filter((c) => v.includes(c.id)).map((c) => c.id));
}

export function disableValue(ids: Set<string>): string {
  return COLLECTORS.filter((c) => ids.has(c.id))
    .map((c) => c.id)
    .join(",");
}
