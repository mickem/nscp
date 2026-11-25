import type { ModulePage } from "../types.ts";

const CREATE_OBJECT_KEYS = new Set(["check command", "host template", "service template"]);

const IcingaClient: ModulePage = {
  id: "IcingaClient",
  collections: {
    "/settings/icinga/client/targets": {
      kind: "instances",
      fieldGroups: [
        {
          name: "Icinga Server",
          keys: ["parent", "address", "host", "port", "username", "password", "retries", "timeout"],
        },
        { name: "TLS", keys: ["tls version", "verify mode", "ca"] },
        {
          name: "Create objects",
          keys: ["ensure objects", "check command", "host template", "service template"],
        },
        { name: "Check", keys: ["check source"] },
      ],
      // The "Create objects" fields only matter when auto-creation is enabled —
      // hide them when ensure objects is off so the tab isn't full of inert config.
      isFieldVisible: (key, { fields }) => {
        if (!CREATE_OBJECT_KEYS.has(key)) return undefined;
        const ensure = fields.find((f) => f.key === "ensure objects");
        const effective = (ensure?.value || ensure?.default_value || "").toLowerCase();
        return effective === "true" || effective === "1";
      },
    },
  },
};

export default IcingaClient;
