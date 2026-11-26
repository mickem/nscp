import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import NsclientPage from "./NsclientPage.tsx";
import { NSCLIENT_TABLE } from "./nsclient.ts";

const NSClientServer: ModulePage = {
  id: "NSClientServer",
  Panel: NsclientPage,
  handledKeys: tableKeys(NSCLIENT_TABLE),
  purpose: {
    tools: ["Nagios"],
    transport: "NSClient (check_nt)",
    direction: "active",
    platforms: ["windows"],
  },
};

export default NSClientServer;
