// Graphite targets: carbon servers metrics and check states are sent to.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { connectionFields, httpTlsFields, httpTlsRisks, isOn } from "../kit/targetFields.ts";
import type { TargetSpec, TargetValues } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";
import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";

const tls = (v: TargetValues) => isOn(v("ssl"));
const withTls = Object.fromEntries(
  Object.entries(httpTlsFields({ verify: "peer", ca: "${ca-path}", tls: "1.2+" })).map(([k, f]) => [
    k,
    { ...f, visible: tls },
  ]),
);

// The paths and switches have defaults on the default target only; every
// other target takes them from its parent (graphite_handler.hpp).
export const GRAPHITE_CLIENT_TARGETS: TargetSpec = {
  moduleId: "GraphiteClient",
  path: "/settings/graphite/client/targets",
  noun: "carbon server",
  fields: {
    ...connectionFields("2003"),
    "send perfdata": {
      label: "Send performance data",
      kind: "bool",
      builtIn: "true",
      inherits: true,
    },
    path: {
      label: "Performance data path",
      kind: "text",
      builtIn: "nsclient.${hostname}.${check_alias}.${perf_alias}",
      inherits: true,
    },
    "send status": { label: "Send check states", kind: "bool", builtIn: "true", inherits: true },
    "status path": {
      label: "Check state path",
      kind: "text",
      builtIn: "nsclient.${hostname}.${check_alias}.status",
      inherits: true,
    },
    "metric path": {
      label: "Metric path",
      kind: "text",
      builtIn: "nsclient.${hostname}.${metric}",
      inherits: true,
      help: "Read from the default target only; other targets take it from there.",
    },
    "metric tags": {
      label: "Send labels as carbon tags",
      kind: "bool",
      builtIn: "false",
      inherits: true,
      help: "Needs carbon 1.1 or later; an older one stores path;tag=value as a new series.",
    },
    ssl: {
      label: "Use TLS",
      kind: "bool",
      builtIn: "false",
      help: "Carbon has no TLS itself: only for a TLS-terminating proxy in front of it.",
    },
    ...withTls,
    "allowed ciphers": {
      label: "Allowed ciphers",
      kind: "text",
      builtIn: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
      visible: tls,
    },
    certificate: {
      label: "Client certificate",
      kind: "text",
      builtIn: "",
      inherits: true,
      visible: tls,
    },
    "certificate key": {
      label: "Client certificate key",
      kind: "text",
      builtIn: "",
      inherits: true,
      visible: tls,
    },
    "certificate format": {
      label: "Certificate format",
      kind: { options: ["PEM", "DER"] },
      builtIn: "PEM",
      visible: tls,
    },
  },
  sections: [
    { title: "Connection", keys: ["address", "timeout", "retries"] },
    {
      title: "What is sent",
      keys: ["send perfdata", "path", "send status", "status path", "metric tags"],
    },
    { title: "Security", keys: ["ssl", "verify mode", "ca", "tls version"] },
  ],
  risks: (v) => (tls(v) ? httpTlsRisks(v, true, "") : []),
  security: (v) => (tls(v) ? "TLS" : "Plain TCP"),
};

const GraphiteClient: ModulePage = {
  id: "GraphiteClient",
  purpose: {
    tools: ["Graphite"],
    transport: "carbon",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  sections: { "/settings/graphite/client": HOSTNAME_WIDGET },
  collections: { [GRAPHITE_CLIENT_TARGETS.path]: targetsCollection(GRAPHITE_CLIENT_TARGETS) },
};

export default GraphiteClient;
