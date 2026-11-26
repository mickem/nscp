// What the web server page knows about WEBServer's settings
// (modules/WEBServer/WEBServer.cpp).
import { Settings } from "../../api/api.ts";
import { httpTlsKeys } from "../kit/httpTls.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const WEB_MODULE = "WEBServer";
export const WEB_PATH = "/settings/WEB/server";
export const WEB_LOG = `${WEB_PATH}/log`;
export const WEB_RESULTS = `${WEB_PATH}/results`;

export const WEB_TABLE: KeyTable = {
  [WEB_PATH]: {
    port: { builtIn: fixed("8443") },
    "allowed hosts": { builtIn: fixed("127.0.0.1"), inherits: true },
    "cache allowed hosts": { builtIn: fixed("true"), inherits: true },
    threads: { builtIn: fixed("10") },
    // The built-in admin user's password, shared through [/settings/default]
    // where `nscp web install` stores it hashed.
    password: { builtIn: fixed(""), inherits: true },
    "disable admin user": { builtIn: fixed("false") },
    "allow anonymous access": { builtIn: fixed("false") },
    "auth rate limit max failures": { builtIn: fixed("10") },
    "auth rate limit block seconds": { builtIn: fixed("60") },
    "persist sessions": { builtIn: fixed("true") },
    "legacy query auth user agents": { builtIn: fixed("Icinga/check_nscp_api") },
    ...httpTlsKeys(),
    "max request size": { builtIn: fixed("3MiB") },
    "openmetrics format": { builtIn: fixed("openmetrics") },
  },
  [WEB_RESULTS]: {
    enabled: { builtIn: fixed("false") },
    channel: { builtIn: fixed("WEB") },
    mode: { builtIn: fixed("last") },
    "primary index": { builtIn: fixed("${host}/${alias-or-command}") },
    "max entries": { builtIn: fixed("1000") },
    "max age": { builtIn: fixed("0") },
    "clear on poll": { builtIn: fixed("true") },
  },
  [WEB_LOG]: {
    error: { builtIn: fixed("true") },
    info: { builtIn: fixed("false") },
    debug: { builtIn: fixed("false") },
  },
};

const isTrue = (v: string) => v.toLowerCase() === "true" || v === "1";

export function anonymousRisk(allow: string): Risk | null {
  return isTrue(allow)
    ? {
        severity: "warning",
        title: "Requests without credentials are answered",
        reason:
          "A role named anonymous in the roles below grants what any caller on the allowed-hosts list may do without logging in.",
      }
    : null;
}

export function webIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, WEB_MODULE, WEB_TABLE, includeDefaults);
}
