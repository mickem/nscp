// NSCA-NG targets: TLS always, authenticated by a pre-shared key or by the
// server's certificate (modules/NSCANgClient).
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { CONNECTION_KEYS, connectionFields, isOn, socketTlsFields } from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import { parseVerifyMode } from "../kit/tls.ts";
import type { ModulePage } from "../types.ts";

const TLS = socketTlsFields({ ssl: true });
const certOnly = (v: (k: string) => string) => !isOn(v("use psk"));

export const NSCA_NG_CLIENT_TARGETS: TargetSpec = {
  moduleId: "NSCANgClient",
  path: "/settings/NSCA-NG/client/targets",
  noun: "NSCA-NG server",
  fields: {
    ...connectionFields("5668"),
    "use psk": { label: "Pre-shared key", kind: "bool", builtIn: "true" },
    identity: {
      label: "Identity",
      kind: "text",
      builtIn: "",
      help: "The PSK identity; the sender's host name when empty.",
      visible: (v) => isOn(v("use psk")),
    },
    password: {
      label: "Password",
      kind: "secret",
      builtIn: "",
      help: "The pre-shared key, as the server has it.",
      visible: (v) => isOn(v("use psk")),
    },
    insecure: {
      label: "Allow an unverified server",
      kind: "bool",
      builtIn: "false",
      help: "Without it, certificate TLS that does not verify the server is refused.",
      visible: certOnly,
    },
    // Only the certificate mode reads the TLS keys.
    ...Object.fromEntries(Object.entries(TLS).map(([k, f]) => [k, { ...f, visible: certOnly }])),
    "use ssl": {
      label: "Use TLS",
      kind: "bool",
      builtIn: "true",
      inherits: true,
      help: "Ignored: NSCA-NG always uses TLS.",
    },
    "max output length": { label: "Output kept (bytes)", kind: "number", builtIn: "65536" },
    "host check": {
      label: "Submit as host checks",
      kind: "bool",
      builtIn: "false",
      help: "Every result on this target as PROCESS_HOST_CHECK_RESULT.",
    },
  },
  sections: [
    { title: "Connection", keys: CONNECTION_KEYS },
    {
      title: "Authentication",
      mode: {
        label: "The server is authenticated by",
        options: [
          {
            value: "psk",
            label: "Pre-shared key",
            description: "TLS-PSK with the password both sides know (TLS 1.2).",
          },
          {
            value: "cert",
            label: "Its certificate",
            description: "Certificate TLS, the server checked against the CA.",
          },
        ],
        detect: (v) => (isOn(v("use psk")) ? "psk" : "cert"),
        apply: (m, v): Record<string, string> => {
          if (m === "psk") return { "use psk": "true" };
          const verify = parseVerifyMode(v("verify mode"));
          return verify?.peer
            ? { "use psk": "false" }
            : { "use psk": "false", "verify mode": "peer-cert" };
        },
      },
      keys: ["identity", "password", "verify mode", "ca", "insecure"],
    },
    { title: "Results", keys: ["host check"] },
  ],
  // The agent turns TLS on for NSCA-NG whatever `use ssl` says.
  expertOnly: ["use ssl"],
  risks: (v) => {
    if (isOn(v("use psk")))
      return v("password") === ""
        ? [
            {
              severity: "error",
              title: "No password",
              reason: "The pre-shared key is empty: the handshake fails, and every result is lost.",
            },
          ]
        : [];
    const peer = parseVerifyMode(v("verify mode"))?.peer ?? false;
    if (peer) return [];
    return isOn(v("insecure"))
      ? [
          {
            severity: "error",
            title: "The server is not authenticated",
            reason:
              "No pre-shared key and no certificate check: anyone on the path can pose as the server.",
          },
        ]
      : [
          {
            severity: "error",
            title: "Every submission is refused",
            reason:
              "Certificate TLS without verifying the server is refused by the agent. Verify the server (peer-cert), or use a pre-shared key.",
          },
        ];
  },
  security: (v) => (isOn(v("use psk")) ? "TLS, pre-shared key" : "TLS, certificate"),
};

const NSCANgClient: ModulePage = {
  id: "NSCANgClient",
  purpose: {
    tools: ["Nagios", "Icinga", "Naemon"],
    transport: "NSCA-NG",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [NSCA_NG_CLIENT_TARGETS.path]: targetsCollection(NSCA_NG_CLIENT_TARGETS) },
};

export default NSCANgClient;
