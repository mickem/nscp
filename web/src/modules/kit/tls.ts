// TLS as the agent's servers configure it.
//
// Two families: the socket servers (NRPE, NSCA, NSClient, check_mk) read the
// option set of include/net/socket/socket_settings_helper.hpp (`use ssl`,
// `verify mode`, `certificate`, ...), and the HTTP listeners (WEB, NCPA) and
// the Icinga agent take a certificate, an `allow insecure` switch and a TLS
// version range in a vocabulary of their own.
import { Settings } from "../../api/api.ts";
import { readStored, SettingChange } from "./changes.ts";
import { fixed, KeySpec } from "./keys.ts";
import { CUSTOM, Custom } from "./modes.ts";
import type { Risk } from "./RiskBanner.tsx";
import type { TlsFiles } from "./TlsSection.tsx";

export const DEFAULT_CERTIFICATE = "${certificate-path}/certificate.pem";
export const DEFAULT_CA = "${certificate-path}/ca.pem";

const isTrue = (v: string | undefined) => {
  const s = (v ?? "").toLowerCase();
  return s === "true" || s === "1";
};

// The bits `verify mode` sets, exactly as the agent parses it
// (socket_helpers.cpp): comma separated, no trimming, and any token outside
// these makes it something the TLS modes do not describe.
export function parseVerifyMode(raw: string): { peer: boolean; required: boolean } | null {
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

// `tls version` of the socket servers: one version, or that version and newer
// with a `+`.
export const SOCKET_TLS_VERSIONS = [
  "tlsv1.3",
  "tlsv1.3+",
  "tlsv1.2",
  "tlsv1.2+",
  "tlsv1.1",
  "tlsv1.1+",
  "tlsv1.0",
  "tlsv1.0+",
];

// `tls version` of the HTTP listeners and the Icinga agent.
export const HTTP_TLS_VERSIONS = [
  "1.3",
  "1.3+",
  "1.2",
  "1.2+",
  "1.1",
  "1.1+",
  "1.0",
  "1.0+",
  "any",
];

export function tlsVersionRisk(version: string): Risk | null {
  if (/^(tlsv1\.[01]|sslv3|1\.[01]|any)/.test(version)) {
    return {
      severity: "warning",
      title: "An outdated TLS version is allowed",
      reason: `${version} lets clients negotiate a protocol older than TLS 1.2. Use a 1.2+ range unless a client cannot.`,
    };
  }
  return null;
}

// --- The socket servers ----------------------------------------------------

export type SocketTlsMode = "plain" | "one-way-tls" | "two-way-tls";

export interface SocketTlsDefaults {
  // `use ssl` when it is not stored: on for check_nt and NRPE, off for NSCA
  // and check_mk.
  ssl: boolean;
  ciphers: string;
  dh: string;
}

// The TLS keys of a socket server, with the defaults its module registers.
export function socketTlsKeys(d: SocketTlsDefaults): Record<string, KeySpec> {
  return {
    "use ssl": { builtIn: fixed(d.ssl ? "true" : "false") },
    "verify mode": { builtIn: fixed("none") },
    certificate: { builtIn: fixed(DEFAULT_CERTIFICATE) },
    "certificate key": { builtIn: fixed("") },
    ca: { builtIn: fixed(DEFAULT_CA) },
    "tls version": { builtIn: fixed("tlsv1.2+") },
    "allowed ciphers": { builtIn: fixed(d.ciphers) },
    "certificate format": { builtIn: fixed("PEM") },
    dh: { builtIn: fixed(d.dh) },
    "ssl options": { builtIn: fixed("") },
    "debug verify": { builtIn: fixed("false") },
  };
}

function effective(stored: Settings[] | undefined, path: string, key: string, dflt: string) {
  return readStored(stored, path, key) ?? dflt;
}

// The mode the stored keys describe, or CUSTOM when they describe none of
// them. Never rounds to the nearest mode.
export function detectSocketTls(
  stored: Settings[] | undefined,
  path: string,
  d: SocketTlsDefaults,
): SocketTlsMode | Custom {
  if (!isTrue(effective(stored, path, "use ssl", d.ssl ? "true" : "false"))) return "plain";
  const verify = parseVerifyMode(effective(stored, path, "verify mode", "none"));
  if (!verify) return CUSTOM;
  if (effective(stored, path, "certificate", DEFAULT_CERTIFICATE) === "") return CUSTOM;
  if (!verify.peer && !verify.required) return "one-way-tls";
  if (verify.peer && verify.required && effective(stored, path, "ca", DEFAULT_CA) !== "")
    return "two-way-tls";
  return CUSTOM;
}

export function readSocketTlsFiles(stored: Settings[] | undefined, path: string): TlsFiles {
  return {
    certificate: effective(stored, path, "certificate", DEFAULT_CERTIFICATE),
    certificateKey: effective(stored, path, "certificate key", ""),
    ca: effective(stored, path, "ca", DEFAULT_CA),
  };
}

// Why a mode cannot be written with these files, or null when it can.
export function tlsFilesProblem(mode: SocketTlsMode | "insecure", files: TlsFiles): string | null {
  if (mode === "plain" || mode === "insecure") return null;
  if (!files.certificate.trim()) return "TLS needs a server certificate.";
  if (mode === "two-way-tls" && !files.ca.trim())
    return "Two-way TLS needs the CA that issues the client certificates.";
  return null;
}

// The changes that put a socket server in `mode`; the keys a mode does not
// care about are left alone.
export function socketTlsChanges(
  path: string,
  mode: SocketTlsMode,
  files: TlsFiles,
  d: SocketTlsDefaults,
): SettingChange[] {
  const c = (key: string, value: string, defaultValue: string): SettingChange => ({
    path,
    key,
    value,
    defaultValue,
  });
  const ssl = d.ssl ? "true" : "false";
  if (mode === "plain") return [c("use ssl", "false", ssl)];
  return [
    c("use ssl", "true", ssl),
    c("certificate", files.certificate.trim(), DEFAULT_CERTIFICATE),
    c("certificate key", files.certificateKey.trim(), ""),
    c("verify mode", mode === "two-way-tls" ? "peer-cert" : "none", "none"),
    ...(mode === "two-way-tls" ? [c("ca", files.ca.trim(), DEFAULT_CA)] : []),
  ];
}
