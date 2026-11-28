import type { ModulePage } from "../types.ts";
import { tableKeys } from "../kit/keys.ts";
import SchedulerPage from "./SchedulerPage.tsx";
import SchedulesEditor from "./SchedulesEditor.tsx";
import { SCHEDULER_TABLE, SCHEDULES } from "./scheduler.ts";

const Scheduler: ModulePage = {
  id: "Scheduler",
  Panel: SchedulerPage,
  handledKeys: tableKeys(SCHEDULER_TABLE),
  purpose: {
    tools: ["Nagios", "Icinga", "Naemon", "op5", "Graphite", "Elastic"],
    transport: "any client module",
    direction: "passive",
    platforms: ["windows", "linux", "macos"],
  },
  collections: { [SCHEDULES]: { kind: "instances", Editor: SchedulesEditor } },
};

export default Scheduler;
