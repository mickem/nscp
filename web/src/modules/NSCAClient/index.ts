// NSCA targets: the NSCA daemons passive results are submitted to.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { CONNECTION_KEYS, connectionFields, socketTlsFields } from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import { encryptionOptions, encryptionRisk } from "../NSCAServer/nsca.ts";
import type { ModulePage } from "../types.ts";
import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";

const TLS = socketTlsFields({ ssl: false });

export const NSCA_CLIENT_TARGETS: TargetSpec = {
  moduleId: "NSCAClient",
  path: "/settings/NSCA/client/targets",
  noun: "NSCA server",
  fields: {
    ...connectionFields("5667"),
    encryption: {
      label: "Encryption",
      kind: (v) => ({ options: encryptionOptions(v("encryption")) }),
      builtIn: "aes",
      help: "The cipher every result is encrypted with; the daemon has to use the same one. aes is aes256.",
    },
    password: {
      label: "Password",
      kind: "secret",
      builtIn: "",
      help: "The shared key, the same as the daemon's.",
    },
    "payload length": {
      label: "Payload length",
      kind: "number",
      builtIn: "512",
      help: "Must match the daemon's compiled-in value; 512 for standard NSCA.",
    },
    timezone: {
      label: "Timezone",
      kind: "text",
      builtIn: "utc",
      help: "Of the timestamp sent: utc as the protocol says, or local for a legacy daemon.",
    },
    "time offset": { label: "Time offset", kind: "text", builtIn: "0" },
    encoding: { label: "Encoding", kind: "text", builtIn: "" },
    ...TLS,
  },
  sections: [
    { title: "Connection", keys: CONNECTION_KEYS },
    { title: "Encryption", keys: ["encryption", "password"] },
  ],
  // NSCA has its own encryption; the TLS keys stay in expert mode, as on the
  // NSCA server page.
  expertOnly: Object.keys(TLS),
  risks: (v) => {
    const out = [];
    const r = encryptionRisk(v("encryption"), false);
    if (r) out.push(r);
    const enc = v("encryption");
    if (enc !== "none" && enc !== "" && v("password") === "")
      out.push({
        severity: "error" as const,
        title: "No password",
        reason: "Results are encrypted with an empty, well-known key. Set the daemon's password.",
      });
    return out;
  },
  security: (v) =>
    v("encryption") === "none" || v("encryption") === "" ? "None" : v("encryption"),
};

const NSCAClient: ModulePage = {
  id: "NSCAClient",
  purpose: {
    tools: ["Nagios", "Icinga", "Naemon", "op5"],
    transport: "NSCA",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  sections: { "/settings/NSCA/client": HOSTNAME_WIDGET },
  collections: { [NSCA_CLIENT_TARGETS.path]: targetsCollection(NSCA_CLIENT_TARGETS) },
};

export default NSCAClient;
