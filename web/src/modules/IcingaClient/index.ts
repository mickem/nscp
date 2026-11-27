// Icinga targets: Icinga 2 REST APIs results are submitted to.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import {
  connectionFields,
  httpTlsFields,
  httpTlsRisks,
  isOn,
  scheme,
} from "../kit/targetFields.ts";
import type { TargetSpec, TargetValues } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";

// Anything but http:// is sent as https (icinga_client.hpp).
const https = (v: TargetValues) => scheme(v("address")) !== "http";
const ensure = (v: TargetValues) => isOn(v("ensure objects"));

export const ICINGA_CLIENT_TARGETS: TargetSpec = {
  moduleId: "IcingaClient",
  path: "/settings/icinga/client/targets",
  noun: "Icinga 2 API",
  fields: {
    ...connectionFields("5665"),
    username: { label: "API user", kind: "text", builtIn: "", inherits: true },
    password: { label: "API password", kind: "secret", builtIn: "", inherits: true },
    ...httpTlsFields({ verify: "peer", ca: "${ca-path}", tls: "1.3" }),
    "ensure objects": {
      label: "Create missing hosts and services",
      kind: "bool",
      builtIn: "false",
      inherits: true,
      help: "Probed with GET first; created with PUT only when missing.",
    },
    "host template": {
      label: "Host templates",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "Comma separated; generic-host when empty.",
      visible: ensure,
    },
    "service template": {
      label: "Service templates",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "Comma separated; generic-service when empty.",
      visible: ensure,
    },
    "check command": {
      label: "Check command",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "Set on created services; dummy when empty.",
      visible: ensure,
    },
    "check source": {
      label: "Check source",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "Reported to Icinga; the local host name when empty.",
    },
  },
  sections: [
    { title: "Connection", keys: ["address", "timeout", "retries"] },
    { title: "Security", keys: ["username", "password", "verify mode", "ca", "tls version"] },
    {
      title: "Objects",
      keys: [
        "ensure objects",
        "host template",
        "service template",
        "check command",
        "check source",
      ],
    },
  ],
  risks: (v) => httpTlsRisks(v, https(v), "The API password and every result"),
  security: (v) => (https(v) ? `HTTPS, verify ${v("verify mode") || "none"}` : "HTTP"),
};

const IcingaClient: ModulePage = {
  id: "IcingaClient",
  purpose: {
    tools: ["Icinga"],
    transport: "Icinga 2 API",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [ICINGA_CLIENT_TARGETS.path]: targetsCollection(ICINGA_CLIENT_TARGETS) },
};

export default IcingaClient;
