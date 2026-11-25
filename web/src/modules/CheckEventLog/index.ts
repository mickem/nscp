import type { ModulePage } from "../types.ts";
import { REALTIME_FILTER_FIELD_GROUPS } from "../shared/realtime.ts";

const CheckEventLog: ModulePage = {
  id: "CheckEventLog",
  collections: {
    "/settings/eventlog/real-time/filters": {
      kind: "instances",
      fieldGroups: REALTIME_FILTER_FIELD_GROUPS,
      // With real-time polling off the filters are dead config.
      gate: { path: "/settings/eventlog/real-time", key: "enabled" },
    },
  },
};

export default CheckEventLog;
