// check_mk targets: check_mk agents whose output is read.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import {
  CONNECTION_KEYS,
  connectionFields,
  SOCKET_TLS_KEYS,
  socketTlsFields,
  socketTlsMode,
  socketTlsRisks,
  socketTlsSummary,
} from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";

export const CHECK_MK_CLIENT_TARGETS: TargetSpec = {
  moduleId: "CheckMKClient",
  path: "/settings/check_mk/client/targets",
  noun: "check_mk agent",
  fields: {
    ...connectionFields("6556"),
    ...socketTlsFields({ ssl: false, verify: "peer", ca: "${ca-path}" }),
  },
  sections: [
    { title: "Connection", keys: CONNECTION_KEYS },
    { title: "Security", mode: socketTlsMode(), keys: SOCKET_TLS_KEYS },
  ],
  risks: (v) => socketTlsRisks(v, "the agent's output"),
  security: socketTlsSummary,
};

const CheckMKClient: ModulePage = {
  id: "CheckMKClient",
  purpose: {
    tools: ["Checkmk"],
    transport: "check_mk",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [CHECK_MK_CLIENT_TARGETS.path]: targetsCollection(CHECK_MK_CLIENT_TARGETS) },
};

export default CheckMKClient;
