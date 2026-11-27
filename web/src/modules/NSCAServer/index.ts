import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import NscaPage from "./NscaPage.tsx";
import { NSCA_PATH, NSCA_TABLE, NSCA_TLS_KEYS } from "./nsca.ts";

const NSCAServer: ModulePage = {
  id: "NSCAServer",
  Panel: NscaPage,
  // NSCA has its own encryption: the agent's TLS keys are in expert mode only.
  handledKeys: {
    [NSCA_PATH]: tableKeys(NSCA_TABLE)[NSCA_PATH].filter((k) => !NSCA_TLS_KEYS.includes(k)),
  },
  expertOnlyKeys: { [NSCA_PATH]: NSCA_TLS_KEYS },
  purpose: {
    tools: ["Nagios", "Icinga", "Naemon", "op5"],
    transport: "NSCA",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
};

export default NSCAServer;
