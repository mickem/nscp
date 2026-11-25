// The allowed-hosts list of a server module.
//
// Every server reads `allowed hosts` from its own section and falls back to
// /settings/default, so the section shows the value in effect and where it
// comes from. Writing goes to the server's own path (allowedHostsChange): a
// change made on one server's page must not open every other server.
import { Settings } from "../../api/api.ts";
import { readStored, SettingChange } from "./changes.ts";

export const DEFAULT_SECTION = "/settings/default";
export const ALLOWED_HOSTS_DEFAULT = "127.0.0.1";
const KEY = "allowed hosts";

export interface AllowedHosts {
  value: string;
  // Where the value in effect comes from: the server's own path, the shared
  // default section, or the agent's built-in default.
  source: "own" | "default section" | "built-in";
}

export function readAllowedHosts(stored: Settings[] | undefined, path: string): AllowedHosts {
  const own = readStored(stored, path, KEY);
  if (own !== undefined) return { value: own, source: "own" };
  const shared = readStored(stored, DEFAULT_SECTION, KEY);
  if (shared !== undefined) return { value: shared, source: "default section" };
  return { value: ALLOWED_HOSTS_DEFAULT, source: "built-in" };
}

export function allowedHostsChange(
  stored: Settings[] | undefined,
  path: string,
  value: string,
): SettingChange {
  // Unchanged from what is in effect: nothing to write, wherever it came from.
  const current = readAllowedHosts(stored, path);
  return {
    path,
    key: KEY,
    value: value.trim(),
    defaultValue: current.source === "own" ? undefined : current.value,
  };
}

// Entries that let any host in.
export function isOpenToAll(value: string): boolean {
  return value
    .split(",")
    .map((h) => h.trim())
    .some((h) => h === "*" || h === "0.0.0.0/0" || h === "::/0");
}
