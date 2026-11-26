import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import IcingaPage from "./IcingaPage.tsx";
import { ICINGA_TABLE } from "./icinga.ts";

const IcingaAgent: ModulePage = {
  id: "IcingaAgent",
  Panel: IcingaPage,
  handledKeys: tableKeys(ICINGA_TABLE),
  purpose: {
    tools: ["Icinga"],
    transport: "Icinga 2 cluster protocol",
    direction: "active",
    platforms: ["windows", "linux", "macos"],
  },
};

export default IcingaAgent;
