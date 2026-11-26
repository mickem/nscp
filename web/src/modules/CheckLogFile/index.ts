import type { ModulePage } from "../types.ts";
import { REALTIME_FILTER_FIELD_GROUPS } from "../shared/realtime.ts";

const CheckLogFile: ModulePage = {
  id: "CheckLogFile",
  collections: {
    "/settings/logfile/real-time/checks": {
      kind: "instances",
      fieldGroups: REALTIME_FILTER_FIELD_GROUPS,
      // With real-time polling off the checks are dead config.
      gate: { path: "/settings/logfile/real-time", key: "enabled" },
    },
  },
};

export default CheckLogFile;
