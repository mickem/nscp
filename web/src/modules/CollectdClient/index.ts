// collectd targets: collectd servers metrics are sent to, over UDP.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { connectionFields } from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";
import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";

export const COLLECTD_CLIENT_TARGETS: TargetSpec = {
  moduleId: "CollectdClient",
  path: "/settings/collectd/client/targets",
  noun: "collectd server",
  fields: {
    ...connectionFields("25826"),
    interval: {
      label: "Interval (seconds)",
      kind: "number",
      builtIn: "",
      inherits: true,
      help: "Reported to collectd with each metric; the client's interval when empty.",
    },
    "multicast interface": {
      label: "Multicast interface",
      kind: "text",
      builtIn: "auto",
      help: "With no address, metrics go to collectd's multicast group through this interface.",
    },
  },
  sections: [{ title: "Connection", keys: ["address", "interval", "multicast interface"] }],
  risks: () => [],
  security: () => "Plain UDP",
};

const CollectdClient: ModulePage = {
  id: "CollectdClient",
  purpose: {
    tools: ["collectd"],
    transport: "collectd",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  sections: { "/settings/collectd/client": HOSTNAME_WIDGET },
  collections: { [COLLECTD_CLIENT_TARGETS.path]: targetsCollection(COLLECTD_CLIENT_TARGETS) },
};

export default CollectdClient;
