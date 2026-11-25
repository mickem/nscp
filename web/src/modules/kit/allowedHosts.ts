// The allowed-hosts list of a server module.
//
// Every server reads `allowed hosts` from its own section and falls back to
// /settings/default. Which of the two a page writes is the reader's choice
// (Default or Local); a fresh install keeps it in the shared section.
import { Settings } from "../../api/api.ts";
import { readScoped, ScopedValue, scopedChanges, SettingChange } from "./changes.ts";

export const ALLOWED_HOSTS_DEFAULT = "127.0.0.1";
export const ALLOWED_HOSTS_KEY = "allowed hosts";

export function readAllowedHosts(stored: Settings[] | undefined, path: string): ScopedValue {
  return readScoped(stored, path, ALLOWED_HOSTS_KEY, ALLOWED_HOSTS_DEFAULT);
}

export function allowedHostsChanges(path: string, next: ScopedValue): SettingChange[] {
  return scopedChanges(
    path,
    ALLOWED_HOSTS_KEY,
    { ...next, value: next.value.trim() },
    ALLOWED_HOSTS_DEFAULT,
  );
}

// Entries that let any host in.
export function isOpenToAll(value: string): boolean {
  return value
    .split(",")
    .map((h) => h.trim())
    .some((h) => h === "*" || h === "0.0.0.0/0" || h === "::/0");
}
