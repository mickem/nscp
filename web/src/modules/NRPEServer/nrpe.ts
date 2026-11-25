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
import { CUSTOM, Custom } from "../kit/modes.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import type { TlsFiles } from "../kit/TlsSection.tsx";

export const NRPE_MODULE = "NRPEServer";
export const NRPE_PATH = "/settings/NRPE/server";
export const EXTERNAL_SCRIPTS_PATH = "/settings/external scripts";

export const INSECURE_CIPHERS = "ALL:!MD5:@STRENGTH:@SECLEVEL=0";
export const SECURE_CIPHERS = "ALL:!aNULL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
// The secure string `nscp nrpe install` and the earlier web wizard wrote.
const LEGACY_SECURE_CIPHERS = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";

export const DEFAULT_PORT = "5666";
export const DEFAULT_CERTIFICATE = "${certificate-path}/certificate.pem";
export const DEFAULT_CA = "${certificate-path}/ca.pem";

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

// The bits `verify mode` sets, exactly as the agent parses it: comma
// separated, no trimming, and any token outside these makes it something the
// three modes do not describe.
function parseVerifyMode(raw: string): { peer: boolean; required: boolean } | null {
  let peer = false;
  let required = false;
  for (const token of raw.split(",")) {
    if (token === "" || token === "none") continue;
    if (token === "peer-cert") {
      peer = true;
      required = true;
    } else if (token === "peer" || token === "certificate") peer = true;
    else if (
      token === "fail-if-no-cert" ||
      token === "fail-if-no-peer-cert" ||
      token === "client-certificate"
    )
      required = true;
    else return null;
  }
  return { peer, required };
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

// Named recipes, for anything that sets NRPE up without rendering this page.
export const NRPE_RECIPES = {
  security: securityChanges,
  arguments: argumentsChanges,
  nastyCharacters: nastyCharactersChange,
  port: portChange,
  enable: enableNrpe,
};
