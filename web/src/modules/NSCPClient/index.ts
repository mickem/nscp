// NSCP targets: other NSClient++ agents queried over their REST API.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import {
  CONNECTION_KEYS,
  connectionFields,
  socketTlsFields,
  socketTlsMode,
  socketTlsRisks,
  socketTlsSummary,
} from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";

export const NSCP_CLIENT_TARGETS: TargetSpec = {
  moduleId: "NSCPClient",
  path: "/settings/NSCP/client/targets",
  noun: "NSClient++ agent",
  fields: {
    ...connectionFields("8443"),
    ...socketTlsFields({
      ssl: true,
      verify: "peer",
      ca: "${ca-path}",
      ciphers: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
    }),
    password: {
      label: "Password",
      kind: "secret",
      builtIn: "",
      help: "The remote agent's web password.",
    },
  },
  sections: [
    { title: "Connection", keys: CONNECTION_KEYS },
    { title: "Security", mode: socketTlsMode(), keys: ["ca", "password"] },
  ],
  // nscp_client.hpp never sends a client certificate, whatever these say.
  expertOnly: ["certificate", "certificate key", "certificate format"],
  risks: (v) => socketTlsRisks(v, "the password and every result"),
  security: socketTlsSummary,
  test: {
    query: "check_remote_nscp",
    // The remote agent needs CheckHelpers for check_ok.
    args: (t) => [`target=${t}`, "command=check_ok"],
    what: "Runs check_ok on the remote agent (it needs CheckHelpers): proves the address, TLS and password.",
  },
};

const NSCPClient: ModulePage = {
  id: "NSCPClient",
  purpose: {
    tools: ["NSClient++"],
    transport: "REST",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [NSCP_CLIENT_TARGETS.path]: targetsCollection(NSCP_CLIENT_TARGETS) },
};

export default NSCPClient;
