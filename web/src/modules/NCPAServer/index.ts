import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import NcpaPage from "./NcpaPage.tsx";
import { NCPA_TABLE } from "./ncpa.ts";

const NCPAServer: ModulePage = {
  id: "NCPAServer",
  Panel: NcpaPage,
  handledKeys: tableKeys(NCPA_TABLE),
  purpose: {
    tools: ["Nagios XI", "Nagios"],
    transport: "NCPA",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
};

export default NCPAServer;
