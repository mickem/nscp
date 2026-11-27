// NRPE targets: the NRPE servers check_nrpe queries (modules/NRPEClient).
import { targetsCollection } from "../kit/targetsCollection.tsx";
import {
  CONNECTION_KEYS,
  connectionFields,
  isOn,
  SOCKET_TLS_KEYS,
  socketTlsFields,
  socketTlsMode,
  socketTlsRisks,
  socketTlsSummary,
} from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";

export const NRPE_CLIENT_TARGETS: TargetSpec = {
  moduleId: "NRPEClient",
  path: "/settings/NRPE/client/targets",
  noun: "NRPE server",
  fields: {
    ...connectionFields("5666"),
    // nrpe_client.hpp: TLS on, nothing verified, the agent's own certificate.
    ...socketTlsFields({
      ssl: true,
      certificate: "${certificate-path}/certificate.pem",
      ciphers: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
    }),
    insecure: {
      label: "Legacy mode",
      kind: "bool",
      builtIn: "false",
      help: "Anonymous TLS for an old NRPE server: no certificates, ciphers at security level 0.",
    },
    version: {
      label: "NRPE version",
      kind: { options: ["2", "4"] },
      builtIn: "2",
      help: "4 carries longer output; the server has to speak it.",
    },
    "payload length": {
      label: "Payload length",
      kind: "number",
      builtIn: "1024",
      help: "Version 2 packets only: must match the server's compiled-in value.",
    },
  },
  sections: [
    { title: "Connection", keys: CONNECTION_KEYS },
    {
      title: "Security",
      mode: socketTlsMode({ legacy: true }),
      keys: SOCKET_TLS_KEYS,
    },
    { title: "Protocol", keys: ["version"] },
  ],
  risks: (v) =>
    isOn(v("insecure"))
      ? [
          {
            severity: "error",
            title: "Legacy NRPE: the server is not authenticated",
            reason:
              "Anonymous TLS encrypts, but nothing proves who answers: anyone on the path can pose as the server. Upgrade the server to certificate TLS.",
          },
        ]
      : socketTlsRisks(v, "the commands and their output"),
  security: (v) => (isOn(v("insecure")) ? "Legacy (anonymous TLS)" : socketTlsSummary(v)),
  test: {
    query: "check_nrpe",
    // No command: the server answers with its version (_NRPE_CHECK).
    args: (t) => [`target=${t}`],
    what: "Asks the server for its version: proves it answers and the TLS settings match.",
  },
};

const NRPEClient: ModulePage = {
  id: "NRPEClient",
  purpose: {
    tools: ["NRPE servers"],
    transport: "NRPE",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [NRPE_CLIENT_TARGETS.path]: targetsCollection(NRPE_CLIENT_TARGETS) },
};

export default NRPEClient;
