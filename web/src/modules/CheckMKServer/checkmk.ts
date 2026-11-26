// What the check_mk server page knows about CheckMKServer's settings
// (modules/CheckMKServer/CheckMKServer.cpp).
import { Settings } from "../../api/api.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import { Custom } from "../kit/modes.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import { coreServerKeys } from "../kit/socketServer.ts";
import { socketTlsKeys, SocketTlsDefaults, SocketTlsMode } from "../kit/tls.ts";

export const CHECKMK_MODULE = "CheckMKServer";
export const CHECKMK_PATH = "/settings/check_mk/server";
export const CHECKMK_LOCAL = `${CHECKMK_PATH}/local`;
export const CHECKMK_MRPE = `${CHECKMK_PATH}/mrpe`;
export const CHECKMK_SCRIPTS = `${CHECKMK_PATH}/scripts`;

export const CHECKMK_TLS: SocketTlsDefaults = {
  ssl: false,
  ciphers: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
  dh: "",
};

export const CHECKMK_TABLE: KeyTable = {
  [CHECKMK_PATH]: {
    port: { builtIn: fixed("6556") },
    ...coreServerKeys(),
    "local channel": { builtIn: fixed("check_mk-local") },
    "mrpe channel": { builtIn: fixed("check_mk-mrpe") },
    "submission ttl": { builtIn: fixed("60") },
    // include/scripts/script_root_setting.hpp reads it with a parent.
    "script root": { builtIn: fixed(""), inherits: true },
    ...socketTlsKeys(CHECKMK_TLS),
  },
};

export function transportRisk(mode: SocketTlsMode | Custom): Risk | null {
  return mode === "plain"
    ? {
        severity: "warning",
        title: "The agent output travels in clear",
        reason:
          "Without TLS, anyone on the network path can read what the agent reports. Keep the allowed-hosts list to the Checkmk server.",
      }
    : null;
}

export function checkmkIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, CHECKMK_MODULE, CHECKMK_TABLE, includeDefaults);
}
