import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import NscaPage from "./NscaPage.tsx";
import { NSCA_TABLE } from "./nsca.ts";

const NSCAServer: ModulePage = {
  id: "NSCAServer",
  Panel: NscaPage,
  handledKeys: tableKeys(NSCA_TABLE),
  purpose: {
    tools: ["Nagios", "Icinga", "Naemon", "op5"],
    transport: "NSCA",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
};

export default NSCAServer;
