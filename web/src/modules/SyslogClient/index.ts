// Syslog targets: syslog servers results are sent to, over UDP.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { connectionFields } from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";
import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";

const SEVERITIES = [
  "emergency",
  "alert",
  "critical",
  "error",
  "warning",
  "notice",
  "informational",
  "debug",
];
const severity = (label: string, builtIn: string) => ({
  label,
  kind: { options: SEVERITIES },
  builtIn,
});

export const SYSLOG_CLIENT_TARGETS: TargetSpec = {
  moduleId: "SyslogClient",
  path: "/settings/syslog/client/targets",
  noun: "syslog server",
  fields: {
    ...connectionFields("514"),
    facility: { label: "Facility", kind: "text", builtIn: "kernel" },
    severity: severity("Severity", "error"),
    "ok severity": severity("Severity of OK", "informational"),
    "warning severity": severity("Severity of WARNING", "warning"),
    "critical severity": severity("Severity of CRITICAL", "critical"),
    "unknown severity": severity("Severity of UNKNOWN", "emergency"),
    tag_syntax: { label: "Tag", kind: "text", builtIn: "NSCA" },
    message_syntax: { label: "Message", kind: "text", builtIn: "%message%" },
  },
  sections: [
    { title: "Connection", keys: ["address"] },
    {
      title: "Message",
      keys: [
        "facility",
        "ok severity",
        "warning severity",
        "critical severity",
        "unknown severity",
        "tag_syntax",
        "message_syntax",
      ],
    },
  ],
  // Plain UDP, as syslog is: nothing to protect it with.
  risks: () => [],
  security: () => "Plain UDP",
};

const SyslogClient: ModulePage = {
  id: "SyslogClient",
  purpose: {
    tools: ["Syslog"],
    transport: "syslog",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  sections: { "/settings/syslog/client": HOSTNAME_WIDGET },
  collections: { [SYSLOG_CLIENT_TARGETS.path]: targetsCollection(SYSLOG_CLIENT_TARGETS) },
};

export default SyslogClient;
