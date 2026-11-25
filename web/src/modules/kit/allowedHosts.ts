// The allowed-hosts list of a server module.
//
// Every server reads `allowed hosts` from its own section and falls back to
// /settings/default, so the section shows the value in effect and where it
// comes from. Writing goes to the server's own path (allowedHostsChange): a
// change made on one server's page must not open every other server.
import { Settings } from "../../api/api.ts";
import {
  DEFAULT_SECTION,
  Inherited,
  inheritedChange,
  readInherited,
  SettingChange,
} from "./changes.ts";

export { DEFAULT_SECTION };
export const ALLOWED_HOSTS_DEFAULT = "127.0.0.1";
const KEY = "allowed hosts";

export type AllowedHosts = Inherited;

export function readAllowedHosts(stored: Settings[] | undefined, path: string): AllowedHosts {
  return readInherited(stored, path, KEY, ALLOWED_HOSTS_DEFAULT);
}

export function allowedHostsChange(
  stored: Settings[] | undefined,
  path: string,
  value: string,
): SettingChange {
  return inheritedChange(stored, path, KEY, value.trim(), ALLOWED_HOSTS_DEFAULT);
}

// Entries that let any host in.
export function isOpenToAll(value: string): boolean {
  return value
    .split(",")
    .map((h) => h.trim())
    .some((h) => h === "*" || h === "0.0.0.0/0" || h === "::/0");
}
