// What the NCPA server page knows about NCPAServer's settings
// (modules/NCPAServer/NCPAServer.cpp).
import { Settings } from "../../api/api.ts";
import { httpTlsKeys } from "../kit/httpTls.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const NCPA_MODULE = "NCPAServer";
export const NCPA_PATH = "/settings/NCPA/server";
export const NCPA_LOG = `${NCPA_PATH}/log`;

export const NCPA_TABLE: KeyTable = {
  [NCPA_PATH]: {
    port: { builtIn: fixed("5693") },
    "allowed hosts": { builtIn: fixed("127.0.0.1"), inherits: true },
    "bind to": { builtIn: fixed(""), inherits: true },
    "cache allowed hosts": { builtIn: fixed("true"), inherits: true },
    threads: { builtIn: fixed("10") },
    token: { builtIn: fixed("") },
    "backup token": { builtIn: fixed("") },
    plugins: { builtIn: fixed("any") },
    "allow arguments": { builtIn: fixed("false") },
    "auth rate limit max failures": { builtIn: fixed("10") },
    "auth rate limit block seconds": { builtIn: fixed("60") },
    ...httpTlsKeys(),
  },
  [NCPA_LOG]: {
    error: { builtIn: fixed("true") },
    info: { builtIn: fixed("false") },
    debug: { builtIn: fixed("false") },
  },
};

export function tokenRisk(token: string): Risk | null {
  return token === ""
    ? {
        severity: "warning",
        title: "No token: every request is refused",
        reason:
          "Set the token check_ncpa.py sends with -t (nscp ncpa token --set stores it hashed).",
      }
    : null;
}

const isTrue = (v: string) => v.toLowerCase() === "true" || v === "1";

export function argumentsRisk(allow: string, plugins: string): Risk | null {
  if (!isTrue(allow)) return null;
  const scripts = plugins.trim() === "any" || plugins.trim() === "scripts";
  return {
    severity: scripts ? "error" : "warning",
    title: "Pollers can pass arguments",
    reason: scripts
      ? "Every caller with the token can run the exposed queries, external scripts included, with arguments of its choosing."
      : "Every caller with the token can run the exposed queries with arguments of its choosing.",
  };
}

export function ncpaIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, NCPA_MODULE, NCPA_TABLE, includeDefaults);
}
