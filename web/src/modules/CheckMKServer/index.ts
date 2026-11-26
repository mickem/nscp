import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import CheckmkPage from "./CheckmkPage.tsx";
import { CHECKMK_LOCAL, CHECKMK_MRPE, CHECKMK_SCRIPTS, CHECKMK_TABLE } from "./checkmk.ts";

const CheckMKServer: ModulePage = {
  id: "CheckMKServer",
  Panel: CheckmkPage,
  handledKeys: tableKeys(CHECKMK_TABLE),
  purpose: {
    tools: ["Checkmk"],
    transport: "Checkmk agent",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
  collections: {
    // `service name = command=<nscp command> [args...]` per entry.
    [CHECKMK_LOCAL]: {
      kind: "keyValue",
      presets: [
        {
          label: "CPU load",
          description: "A local check on the CPU load.",
          defaultKey: "CPU Load",
          defaultValue: "command=check_cpu warn=load>80",
        },
      ],
    },
    [CHECKMK_MRPE]: {
      kind: "keyValue",
      presets: [
        {
          label: "Uptime",
          description: "An MRPE check on the uptime.",
          defaultKey: "Uptime",
          defaultValue: "command=check_uptime warn=uptime<2d",
        },
      ],
    },
    // `name = <lua script>` per entry.
    [CHECKMK_SCRIPTS]: { kind: "keyValue" },
  },
};

export default CheckMKServer;
