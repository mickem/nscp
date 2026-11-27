// Gearman targets: the gearmand job servers mod_gearman reads results from.
// The worker section is a module setting, not a target.
import { targetsCollection } from "../kit/targetsCollection.tsx";
import { connectionFields, isOn } from "../kit/targetFields.ts";
import type { TargetSpec } from "../kit/targets.ts";
import type { ModulePage } from "../types.ts";

export const GEARMAN_CLIENT_TARGETS: TargetSpec = {
  moduleId: "GearmanClient",
  path: "/settings/gearman/client/targets",
  noun: "gearmand server",
  fields: {
    ...connectionFields("4730"),
    encryption: { label: "Encrypt results", kind: "bool", builtIn: "true" },
    key: {
      label: "Key",
      kind: "secret",
      builtIn: "",
      help: "The shared password from the core's module.conf (key=); at most 32 bytes are used.",
    },
    insecure: {
      label: "Allow unencrypted results",
      kind: "bool",
      builtIn: "false",
      help: "Without it, a submission with encryption off is refused.",
    },
    queue: {
      label: "Result queue",
      kind: "text",
      builtIn: "check_results",
      help: "Only when the core's module.conf names another result queue.",
    },
  },
  sections: [
    { title: "Connection", keys: ["address", "timeout", "retries", "queue"] },
    {
      title: "Security",
      mode: {
        label: "Results travel",
        options: [
          {
            value: "encrypted",
            label: "Encrypted (AES-256)",
            description: "mod_gearman's encryption=yes, with the shared key.",
          },
          {
            value: "plain",
            label: "Unencrypted",
            description: "Plain base64: anyone with access to gearmand reads and forges results.",
          },
        ],
        detect: (v) => {
          if (isOn(v("encryption"))) return "encrypted";
          return isOn(v("insecure")) ? "plain" : null;
        },
        apply: (m) =>
          m === "encrypted"
            ? { encryption: "true", insecure: "false" }
            : { encryption: "false", insecure: "true" },
      },
      keys: ["key"],
    },
  ],
  risks: (v) => {
    if (isOn(v("encryption")))
      return v("key") === ""
        ? [
            {
              severity: "error",
              title: "Every submission is refused",
              reason:
                "Encryption is on and no key is set. Set the key from the core's module.conf.",
            },
          ]
        : [];
    return isOn(v("insecure"))
      ? [
          {
            severity: "error",
            title: "Results are not encrypted",
            reason: "Anyone who can reach gearmand can read results and submit forged ones.",
          },
        ]
      : [
          {
            severity: "error",
            title: "Every submission is refused",
            reason: "Encryption is off without Allow unencrypted results.",
          },
        ];
  },
  security: (v) => (isOn(v("encryption")) ? "AES-256" : "None"),
};

const GearmanClient: ModulePage = {
  id: "GearmanClient",
  purpose: {
    tools: ["Naemon", "Nagios"],
    transport: "mod_gearman",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [GEARMAN_CLIENT_TARGETS.path]: targetsCollection(GEARMAN_CLIENT_TARGETS) },
};

export default GearmanClient;
