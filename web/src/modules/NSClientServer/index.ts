import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import NsclientPage from "./NsclientPage.tsx";
import { NSCLIENT_PATH, NSCLIENT_TABLE, NSCLIENT_TLS_KEYS } from "./nsclient.ts";

const NSClientServer: ModulePage = {
  id: "NSClientServer",
  Panel: NsclientPage,
  // check_nt has no TLS: the agent's TLS keys are reachable in expert mode only.
  handledKeys: {
    [NSCLIENT_PATH]: tableKeys(NSCLIENT_TABLE)[NSCLIENT_PATH].filter(
      (k) => !NSCLIENT_TLS_KEYS.includes(k),
    ),
  },
  expertOnlyKeys: { [NSCLIENT_PATH]: NSCLIENT_TLS_KEYS },
  purpose: {
    tools: ["Nagios"],
    transport: "NSClient (check_nt)",
    direction: "active",
    platforms: ["windows"],
  },
};

export default NSClientServer;
