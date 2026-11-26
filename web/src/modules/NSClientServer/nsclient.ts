// What the NSClient (check_nt) server page knows about NSClientServer's
// settings (modules/NSClientServer/NSClientServer.cpp).
import { Settings } from "../../api/api.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import { coreServerKeys } from "../kit/socketServer.ts";
import { Custom } from "../kit/modes.ts";
import { socketTlsKeys, SocketTlsDefaults, SocketTlsMode } from "../kit/tls.ts";

export const NSCLIENT_MODULE = "NSClientServer";
export const NSCLIENT_PATH = "/settings/NSClient/server";

// TLS on by default: check_nt carries the password in every request.
export const NSCLIENT_TLS: SocketTlsDefaults = {
  ssl: true,
  ciphers: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
  dh: "",
};

export const NSCLIENT_TABLE: KeyTable = {
  [NSCLIENT_PATH]: {
    port: { builtIn: fixed("12489") },
    ...coreServerKeys(),
    // Shared with the web UI and other inbound protocols through
    // [/settings/default], where `nscp web install` stores it hashed.
    password: { builtIn: fixed(""), inherits: true },
    allow: { builtIn: fixed("any") },
    "performance data": { builtIn: fixed("true") },
    ...socketTlsKeys(NSCLIENT_TLS),
  },
};

// The command groups `allow` takes, besides single command names.
export const COMMAND_GROUPS = ["metrics", "info", "service", "process", "counters", "files"];

export function legacyRisk(): Risk {
  return {
    severity: "warning",
    title: "check_nt is a legacy protocol",
    reason:
      "It carries the password in every request and has no way to authenticate the agent. Prefer REST or NRPE for new setups.",
  };
}

export function transportRisk(mode: SocketTlsMode | Custom): Risk | null {
  return mode === "plain"
    ? {
        severity: "error",
        title: "The password travels in clear",
        reason:
          "Without TLS, anyone on the network path can read the password from any request. Use TLS if your check_nt client supports it.",
      }
    : null;
}

export function passwordRisk(password: string): Risk | null {
  return password === ""
    ? {
        severity: "warning",
        title: "No password: every request is refused",
        reason:
          "Set a password here or in [/settings/default] (nscp web install does) for check_nt to answer.",
      }
    : null;
}

export function nsclientIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, NSCLIENT_MODULE, NSCLIENT_TABLE, includeDefaults);
}
