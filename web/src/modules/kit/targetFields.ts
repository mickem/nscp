// Target keys the clients share, with the agent's defaults
// (include/nscapi/nscapi_targets.cpp), and the TLS shapes they come in.
import type { Risk } from "./RiskBanner.tsx";
import type { TargetField, TargetMode, TargetValues } from "./targets.ts";
import { HTTP_TLS_VERSIONS, parseVerifyMode } from "./tls.ts";

const on = (v: string) => v.toLowerCase() === "true" || v === "1";
export const isOn = on;

// Where the target is, how long to wait, and whether a request may send its
// credentials elsewhere. Every one of these has a default of its own, so none
// is inherited from the default target.
export function connectionFields(port: string): Record<string, TargetField> {
  return {
    address: {
      label: "Address",
      kind: "text",
      builtIn: "",
      help: `Host name or IP, optionally with :port (port ${port} if left out).`,
    },
    host: {
      label: "Host",
      kind: "text",
      builtIn: "",
      help: "Replaces the host part of the address.",
    },
    port: {
      label: "Port",
      kind: "text",
      builtIn: "",
      help: `Replaces the port of the address (${port} when neither names one).`,
    },
    timeout: { label: "Timeout (seconds)", kind: "number", builtIn: "30" },
    retries: { label: "Retries", kind: "number", builtIn: "3" },
    "allow host override": {
      label: "Let a request send this target's credentials elsewhere",
      kind: "bool",
      builtIn: "false",
      help: "Off refuses a request that names another host, proxy or TLS setting while this target's password or token goes with it. Add a separate target instead.",
    },
  };
}

export const CONNECTION_KEYS = ["address", "timeout", "retries"];

// --- Socket clients (add_ssl_keys) -------------------------------------------

export interface SocketTlsDefaults {
  ssl: boolean;
  // A module that passes ssl_defaults gives these two a default of their
  // own, so they are not inherited.
  verify?: string;
  ca?: string;
  certificate?: string;
  ciphers?: string;
}

const PEM = "PEM";

export function socketTlsFields(d: SocketTlsDefaults): Record<string, TargetField> {
  const tls = (v: TargetValues) => on(v("use ssl"));
  return {
    "use ssl": {
      label: "Use TLS",
      kind: "bool",
      builtIn: d.ssl ? "true" : "false",
      inherits: true,
    },
    "verify mode": {
      label: "Verify mode",
      kind: "text",
      builtIn: d.verify ?? "",
      inherits: d.verify === undefined,
      help: "none, peer or peer-cert, comma separated with fail-if-no-cert, client-once, workarounds, single.",
      visible: tls,
    },
    ca: {
      label: "CA",
      kind: "text",
      builtIn: d.ca ?? "",
      inherits: d.ca === undefined,
      help: "The certificate authority the server's certificate is checked against.",
      visible: tls,
    },
    certificate: {
      label: "Client certificate",
      kind: "text",
      builtIn: d.certificate ?? "",
      inherits: true,
      help: "Sent to a server that asks for one (two-way TLS).",
      visible: tls,
    },
    "certificate key": {
      label: "Client certificate key",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "Empty when the key is in the certificate file.",
      visible: tls,
    },
    "certificate format": {
      label: "Certificate format",
      kind: { options: ["PEM", "DER"] },
      builtIn: PEM,
      inherits: true,
      visible: tls,
    },
    "allowed ciphers": {
      label: "Allowed ciphers",
      kind: "text",
      builtIn: d.ciphers ?? "",
      inherits: true,
      help: "OpenSSL cipher list.",
      visible: tls,
    },
    dh: {
      label: "DH parameters",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "Only for an anonymous-cipher setup.",
      visible: tls,
    },
  };
}

export type SocketTlsMode = "verified" | "unverified" | "plain";

// TLS that checks the server, TLS that does not, or none. A verify mode the
// agent would reject, or one that asks only for a client certificate, is
// Custom.
export function socketTlsMode(extra: { legacy?: boolean } = {}): TargetMode {
  return {
    label: "Connection",
    options: [
      {
        value: "verified",
        label: "TLS, server verified",
        description: "The server's certificate must chain to the CA and name the host.",
      },
      {
        value: "unverified",
        label: "TLS, server not verified",
        description:
          "Encrypted, but anyone who can intercept the connection can pose as the server.",
      },
      ...(extra.legacy
        ? [
            {
              value: "legacy",
              label: "Legacy NRPE (anonymous TLS)",
              description:
                "For a server that only speaks the old anonymous-cipher NRPE: no certificate either way.",
            },
          ]
        : []),
      {
        value: "plain",
        label: "No TLS",
        description: "Plain TCP. Only where the network path is trusted.",
      },
    ],
    detect: (v) => {
      if (extra.legacy && on(v("insecure"))) return "legacy";
      if (!on(v("use ssl"))) return "plain";
      const mode = parseVerifyMode(v("verify mode"));
      if (!mode) return null;
      if (mode.peer) return "verified";
      return mode.required ? null : "unverified";
    },
    apply: (mode, v) => {
      const out: Record<string, string> = {};
      if (extra.legacy && on(v("insecure")) && mode !== "legacy") out.insecure = "false";
      if (mode === "legacy") return { ...out, insecure: "true", "use ssl": "true" };
      if (mode === "plain") return { ...out, "use ssl": "false" };
      out["use ssl"] = "true";
      const current = parseVerifyMode(v("verify mode"));
      if (mode === "verified" && !current?.peer) out["verify mode"] = "peer-cert";
      if (mode === "unverified" && (current === null || current.peer)) out["verify mode"] = "none";
      return out;
    },
  };
}

export function socketTlsRisks(v: TargetValues, what: string): Risk[] {
  if (!on(v("use ssl")))
    return [
      {
        severity: "error",
        title: "Sent in clear",
        reason: `Without TLS, ${what} can be read and changed by anyone on the network path.`,
      },
    ];
  const mode = parseVerifyMode(v("verify mode"));
  if (mode && !mode.peer)
    return [
      {
        severity: "warning",
        title: "The server is not verified",
        reason:
          "The connection is encrypted, but its certificate is not checked: anyone who can intercept it can pose as the server. Choose TLS, server verified, with the server's CA.",
      },
    ];
  return [];
}

export function socketTlsSummary(v: TargetValues): string {
  if (!on(v("use ssl"))) return "No TLS";
  const mode = parseVerifyMode(v("verify mode"));
  if (!mode) return `TLS (verify: ${v("verify mode")})`;
  return mode.peer ? "TLS, verified" : "TLS, not verified";
}

export const SOCKET_TLS_KEYS = ["certificate", "certificate key", "ca"];

// --- HTTP clients (NRDP, Icinga, Graphite) -----------------------------------

export function httpTlsFields(d: {
  verify: string;
  ca: string;
  tls: string;
}): Record<string, TargetField> {
  return {
    "verify mode": {
      label: "Verify the server",
      kind: { options: ["peer", "peer-cert", "none"] },
      builtIn: d.verify,
      help: "peer checks the certificate chains to the CA and names the host; none checks nothing.",
    },
    ca: {
      label: "CA",
      kind: "text",
      builtIn: d.ca,
      help: "${ca-path} is the system's trusted roots.",
    },
    "tls version": {
      label: "TLS version",
      kind: { options: HTTP_TLS_VERSIONS },
      builtIn: d.tls,
      help: "An exact version, or with + that version and newer.",
    },
  };
}

export function httpTlsRisks(v: TargetValues, secure: boolean, what: string): Risk[] {
  if (!secure)
    return [
      {
        severity: "error",
        title: "Sent over plain HTTP",
        reason: `${what} can be read and changed by anyone on the network path. Use an https:// address.`,
      },
    ];
  const out: Risk[] = [];
  if (/(^|,)none($|,)/.test(v("verify mode")) || v("verify mode") === "")
    out.push({
      severity: "warning",
      title: "The server is not verified",
      reason:
        "Anyone who can intercept the connection can pose as the server and collect what is sent.",
    });
  if (/^(1\.[01]|tlsv1\.[01]|sslv3|ssl3|any)/.test(v("tls version")))
    out.push({
      severity: "warning",
      title: "An outdated TLS version is allowed",
      reason: `${v("tls version")} lets the connection fall back below TLS 1.2.`,
    });
  return out;
}

export function scheme(address: string): string {
  const m = /^([a-z][a-z0-9+.-]*):\/\//i.exec(address);
  return m ? m[1].toLowerCase() : "";
}
