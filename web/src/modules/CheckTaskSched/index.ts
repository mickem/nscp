// Scheduled tasks (modules/CheckTaskSched): the check takes no settings; the
// task inventory is a fact set switched on here.
import { fixed, KeyTable, tableKeys } from "../kit/keys.ts";
import { simplePage } from "../kit/SimplePage.tsx";
import type { ModulePage } from "../types.ts";

const PATH = "/settings/task schedule/facts";
const TABLE: KeyTable = { [PATH]: { "tasks.scheduled": { builtIn: fixed("false") } } };

const CheckTaskSched: ModulePage = {
  id: "CheckTaskSched",
  Panel: simplePage({
    moduleId: "CheckTaskSched",
    title: "Scheduled tasks",
    subheader: "Checks the Windows Task Scheduler's tasks and their last runs.",
    path: PATH,
    table: TABLE,
    section: "Facts",
    fields: [
      {
        key: "tasks.scheduled",
        label: "Report the scheduled tasks",
        kind: "bool",
        help: "Path, name, folder and the enabled and hidden flags of every local task, as tasks.scheduled. No actions, accounts or results.",
      },
    ],
  }),
  handledKeys: tableKeys(TABLE),
};

export default CheckTaskSched;
