// What the NRPE page knows about NRPEServer's settings, as data and pure
// functions: how the stored keys read back into a mode, and the changes each
// mode writes. The page's controls call these recipes; nothing here saves.
//
// The semantics are the agent's (modules/NRPEServer/NRPEServer.cpp and the
// TLS option parser in include/net/socket/socket_helpers.cpp), including the
// defaults that depend on `insecure`: with it on, the agent loads no server
// certificate and uses SECLEVEL=0 ciphers unless they are set explicitly.
import { Settings } from "../../api/api.ts";
import { enableModuleChange, readStored, SettingChange } from "../kit/changes.ts";
import { ALLOWED_HOSTS_DEFAULT } from "../kit/allowedHosts.ts";
import {
  FieldValue,
  fixed,
  KeySpec,
  KeyTable,
  keyChanges,
  readKey,
  tableIni,
} from "../kit/keys.ts";
import { CUSTOM, Custom } from "../kit/modes.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import type { TlsFiles } from "../kit/TlsSection.tsx";
import {
  DEFAULT_CA,
  DEFAULT_CERTIFICATE,
  parseVerifyMode,
  SOCKET_TLS_VERSIONS,
  tlsVersionRisk,
} from "../kit/tls.ts";

export const NRPE_MODULE = "NRPEServer";
export const NRPE_PATH = "/settings/NRPE/server";
export const EXTERNAL_SCRIPTS_PATH = "/settings/external scripts";

export const INSECURE_CIPHERS = "ALL:!MD5:@STRENGTH:@SECLEVEL=0";
export const SECURE_CIPHERS = "ALL:!aNULL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
// The secure string `nscp nrpe install` and the earlier web wizard wrote.
const LEGACY_SECURE_CIPHERS = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";

export const DEFAULT_PORT = "5666";
export { DEFAULT_CA, DEFAULT_CERTIFICATE };

export type SecurityMode = "two-way-tls" | "one-way-tls" | "insecure";
export type ArgumentsMode = "none" | "built-ins" | "all";

const isTrue = (v: string | undefined) => {
  const s = (v ?? "").toLowerCase();
  return s === "true" || s === "1";
};

// The value in effect: stored, or the agent's default for that key.
function effective(stored: Settings[] | undefined, key: string, dflt: string): string {
  return readStored(stored, NRPE_PATH, key) ?? dflt;
}

export function isInsecureFlag(stored: Settings[] | undefined): boolean {
  return isTrue(effective(stored, "insecure", "false"));
}

function certificateDefault(insecure: boolean) {
  return insecure ? "" : DEFAULT_CERTIFICATE;
}
function ciphersDefault(insecure: boolean) {
  return insecure ? INSECURE_CIPHERS : SECURE_CIPHERS;
}

// The security mode the stored keys describe, or CUSTOM when they describe
// none of them. Never rounds to the nearest mode.
export function detectSecurity(stored: Settings[] | undefined): SecurityMode | Custom {
  if (!isTrue(effective(stored, "use ssl", "true"))) return CUSTOM;
  const insecure = isInsecureFlag(stored);
  const ciphers = effective(stored, "allowed ciphers", ciphersDefault(insecure));
  const certificate = effective(stored, "certificate", certificateDefault(insecure));
  const verify = parseVerifyMode(effective(stored, "verify mode", "none"));
  if (!verify) return CUSTOM;
  if (insecure) {
    return ciphers === INSECURE_CIPHERS && certificate === "" && !verify.peer && !verify.required
      ? "insecure"
      : CUSTOM;
  }
  if (ciphers !== SECURE_CIPHERS && ciphers !== LEGACY_SECURE_CIPHERS) return CUSTOM;
  if (certificate === "") return CUSTOM;
  if (!verify.peer && !verify.required) return "one-way-tls";
  if (verify.peer && verify.required && effective(stored, "ca", DEFAULT_CA) !== "")
    return "two-way-tls";
  return CUSTOM;
}

export function readTlsFiles(stored: Settings[] | undefined): TlsFiles {
  const insecure = isInsecureFlag(stored);
  return {
    certificate: effective(stored, "certificate", certificateDefault(insecure)),
    certificateKey: effective(stored, "certificate key", ""),
    ca: effective(stored, "ca", DEFAULT_CA),
  };
}

// Why a mode cannot be written with these files, or null when it can.
export function securityProblem(mode: SecurityMode, files: TlsFiles): string | null {
  if (mode === "insecure") return null;
  if (!files.certificate.trim()) return "TLS needs a server certificate.";
  if (mode === "two-way-tls" && !files.ca.trim())
    return "Two-way TLS needs the CA that issues the client certificates.";
  return null;
}

// The changes that put NRPE in `mode`. The keys a mode does not care about are
// left alone; `allowed ciphers` and `certificate` are reset to the agent's
// default for the mode rather than written, so they follow the agent if its
// defaults are tightened later.
export function securityChanges(mode: SecurityMode, files: TlsFiles): SettingChange[] {
  const c = (key: string, value: string | null, defaultValue?: string): SettingChange => ({
    path: NRPE_PATH,
    key,
    value,
    defaultValue,
  });
  if (mode === "insecure") {
    return [
      c("use ssl", "true", "true"),
      c("insecure", "true", "false"),
      c("allowed ciphers", null),
      c("certificate", null),
      c("certificate key", null),
      c("verify mode", null),
    ];
  }
  return [
    c("use ssl", "true", "true"),
    c("insecure", "false", "false"),
    c("allowed ciphers", null),
    c("certificate", files.certificate.trim(), DEFAULT_CERTIFICATE),
    c("certificate key", files.certificateKey.trim(), ""),
    c("verify mode", mode === "two-way-tls" ? "peer-cert" : "none", "none"),
    ...(mode === "two-way-tls" ? [c("ca", files.ca.trim(), DEFAULT_CA)] : []),
  ];
}

// How arguments from the monitoring server are handled. Two flags decide it:
// NRPEServer's `allow arguments` (may a request carry any) and
// CheckExternalScripts' `allow arguments` (may those reach your scripts).
export function detectArguments(stored: Settings[] | undefined): ArgumentsMode {
  if (!isTrue(effective(stored, "allow arguments", "false"))) return "none";
  const scripts = isTrue(readStored(stored, EXTERNAL_SCRIPTS_PATH, "allow arguments"));
  return scripts ? "all" : "built-ins";
}

export function argumentsChanges(mode: ArgumentsMode): SettingChange[] {
  if (mode === "none") {
    // The scripts flag is left alone: it also decides what other transports
    // (REST, NSCP) may pass to scripts.
    return [{ path: NRPE_PATH, key: "allow arguments", value: "false", defaultValue: "false" }];
  }
  return [
    { path: NRPE_PATH, key: "allow arguments", value: "true", defaultValue: "false" },
    {
      path: EXTERNAL_SCRIPTS_PATH,
      key: "allow arguments",
      value: mode === "all" ? "true" : "false",
      defaultValue: "false",
    },
  ];
}

export function readNastyCharacters(stored: Settings[] | undefined): boolean {
  return isTrue(effective(stored, "allow nasty characters", "false"));
}

export function nastyCharactersChange(allow: boolean): SettingChange {
  return {
    path: NRPE_PATH,
    key: "allow nasty characters",
    value: allow ? "true" : "false",
    defaultValue: "false",
  };
}

export function readPort(stored: Settings[] | undefined): string {
  return effective(stored, "port", DEFAULT_PORT);
}

export function portChange(port: string): SettingChange {
  return { path: NRPE_PATH, key: "port", value: port.trim(), defaultValue: DEFAULT_PORT };
}

export function enableNrpe(): SettingChange {
  return enableModuleChange(NRPE_MODULE);
}

// What the reader is told about a mode.
export function securityRisk(mode: SecurityMode | Custom): Risk | null {
  if (mode === "insecure") {
    return {
      severity: "error",
      title: "NRPE is insecure",
      reason:
        "The agent has no certificate and accepts ciphers below the current security level, so clients " +
        "cannot tell it from an impostor and anyone on the network path can read or change requests and " +
        "results. Use this only for legacy check_nrpe on a network you trust.",
    };
  }
  if (mode === "one-way-tls") {
    return {
      severity: "warning",
      title: "Clients are not authenticated",
      reason:
        "Traffic is encrypted and clients can verify the agent, but any host on the allowed-hosts list " +
        "can run commands. Use two-way TLS to require a client certificate.",
    };
  }
  return null;
}

export function argumentsRisk(mode: ArgumentsMode, nasty: boolean): Risk | null {
  if (mode === "all") {
    return {
      severity: "error",
      title: "Requests can pass arguments to your scripts",
      reason:
        "Any host that can reach the NRPE port can run your external scripts with arguments of its " +
        "choosing. Keep the allowed-hosts list tight and the port firewalled.",
    };
  }
  if (mode !== "none" && nasty) {
    return {
      severity: "warning",
      title: "Shell metacharacters are allowed in arguments",
      reason:
        "Arguments are passed to commands as single values, so this is defence in depth rather than the " +
        "last line, but it lets the most obvious abuse patterns through.",
    };
  }
  return null;
}

// Every key of [/settings/NRPE/server]: what the agent uses while it is not
// stored, and whether it falls back to [/settings/default] first
// (add_core_server_opts, and `encoding`). Some defaults follow the insecure
// flag, as in the agent.
export type NrpeKey = KeySpec;

// The keys the page edits one value at a time.
export const NRPE_FIELDS: Record<string, KeySpec> = {
  "bind to": { builtIn: fixed(""), inherits: true },
  "cache allowed hosts": { builtIn: fixed("true"), inherits: true },
  timeout: { builtIn: fixed("30"), inherits: true },
  "thread pool": { builtIn: fixed("10"), inherits: true },
  "socket queue size": { builtIn: fixed("0"), inherits: true },
  encoding: { builtIn: fixed(""), inherits: true },
  "client identity source": { builtIn: fixed("none") },
  "expose version": { builtIn: fixed("true") },
  "tls version": { builtIn: fixed("tlsv1.2+") },
  "payload length": { builtIn: fixed("1024") },
  // Legacy check_nrpe cannot read more than one packet, so insecure mode turns
  // it off unless it is set.
  "extended response": { builtIn: (stored) => (isInsecureFlag(stored) ? "false" : "true") },
  "performance data": { builtIn: fixed("true") },
  "certificate format": { builtIn: fixed("PEM") },
  dh: { builtIn: fixed("${nrpe-dh}/nrpe_dh_2048.pem") },
  "ssl options": { builtIn: fixed("") },
  "debug verify": { builtIn: fixed("false") },
};

// All of them, in the order the INI fragment lists them.
export const NRPE_KEYS: Record<string, KeySpec> = {
  port: { builtIn: fixed(DEFAULT_PORT) },
  "allowed hosts": { builtIn: fixed(ALLOWED_HOSTS_DEFAULT), inherits: true },
  "use ssl": { builtIn: fixed("true") },
  insecure: { builtIn: fixed("false") },
  "verify mode": { builtIn: fixed("none") },
  certificate: { builtIn: (stored) => certificateDefault(isInsecureFlag(stored)) },
  "certificate key": { builtIn: fixed("") },
  ca: { builtIn: fixed(DEFAULT_CA) },
  "allowed ciphers": { builtIn: (stored) => ciphersDefault(isInsecureFlag(stored)) },
  "allow arguments": { builtIn: fixed("false") },
  "allow nasty characters": { builtIn: fixed("false") },
  ...NRPE_FIELDS,
};

export const NRPE_TABLE: KeyTable = { [NRPE_PATH]: NRPE_KEYS };

export type { FieldValue };

export function readField(stored: Settings[] | undefined, key: string): FieldValue {
  return readKey(stored, NRPE_PATH, key, NRPE_KEYS[key]);
}

export function fieldChanges(
  stored: Settings[] | undefined,
  key: string,
  next: FieldValue,
): SettingChange[] {
  return keyChanges(stored, NRPE_PATH, key, NRPE_KEYS[key], next);
}

// The NRPE configuration as an INI fragment, to copy to another agent or into
// a fleet bundle, with CheckExternalScripts' `allow arguments` when it is set.
export function nrpeIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  const scripts = readStored(stored, EXTERNAL_SCRIPTS_PATH, "allow arguments");
  return tableIni(stored, NRPE_MODULE, NRPE_TABLE, includeDefaults, [
    [EXTERNAL_SCRIPTS_PATH, scripts !== undefined ? [["allow arguments", scripts]] : []],
  ]);
}

export { SOCKET_TLS_VERSIONS as TLS_VERSIONS, tlsVersionRisk };

// `client identity source = cn` takes the caller's identity from its client
// certificate, so the agent refuses to start NRPE unless one is required and
// verified (two-way TLS).
export function identityProblem(mode: SecurityMode | Custom, identity: string): string | null {
  if (identity === "none") return null;
  if (identity !== "cn")
    return `Client identity "${identity}" is not one the agent knows (none or cn).`;
  if (mode !== "two-way-tls" && mode !== CUSTOM)
    return "Client identity from the certificate (cn) needs two-way TLS; the agent refuses to start NRPE without it.";
  return null;
}

// Keys of [/settings/NRPE/server] only expert mode edits: the security mode
// owns them, and a value it does not write reads back as Custom.
export const NRPE_EXPERT_ONLY_KEYS = ["allowed ciphers"];

// Named recipes, for anything that sets NRPE up without rendering this page.
export const NRPE_RECIPES = {
  field: fieldChanges,
  security: securityChanges,
  arguments: argumentsChanges,
  nastyCharacters: nastyCharactersChange,
  port: portChange,
  enable: enableNrpe,
};
