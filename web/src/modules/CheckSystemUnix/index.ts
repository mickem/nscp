// The real-time collections of the Linux / macOS system module. Same layout as
// CheckSystem's, but the vocabularies differ: memory types are physical,
// cached and swap; `type` / `time` / `process` take one value and their plural
// siblings (`types`, `times`, `processes`) the comma-separated list; and the
// process collector only sees running processes, so there is no `new` flag and
// no `state=stopped` row to match on.
import type { ModulePage } from "../types.ts";
import { plainOptions } from "../shared/options.ts";
import { REALTIME_FILTER_FIELD_GROUPS } from "../shared/realtime.ts";

const REALTIME = "/settings/system/unix/real-time";
const MEMORY_TYPES = ["physical", "cached", "swap"];

const CheckSystemUnix: ModulePage = {
  id: "CheckSystemUnix",
  collections: {
    [`${REALTIME}/cpu`]: {
      kind: "instances",
      fieldGroups: REALTIME_FILTER_FIELD_GROUPS,
      previewColumn: { key: "time", label: "Time" },
      presets: [
        {
          name: "Cpu above 80%",
          description: "Raise an event when CPU usage stays above 80% for a minute.",
          defaultName: "cpu_above_80",
          fields: {
            time: "60",
            filter: "usage > 80%",
            warning: "count > 0",
            "top syntax": "%(list)",
            "detail syntax": "%(core): system: %(system)%, user: %(user)",
            "silent period": "5m",
            destination: "event",
          },
        },
      ],
    },
    [`${REALTIME}/memory`]: {
      kind: "instances",
      fieldGroups: REALTIME_FILTER_FIELD_GROUPS,
      previewColumn: { key: "type", label: "Type" },
      fields: {
        type: { kind: "select", options: plainOptions(MEMORY_TYPES) },
        types: { kind: "multi", options: MEMORY_TYPES },
      },
      presets: [
        {
          name: "Memory above 50%",
          description: "Raise an event when physical memory usage exceeds 50%.",
          defaultName: "memory_above_50",
          fields: {
            type: "physical",
            filter: "used > 50%",
            warning: "count > 0",
            "top syntax": "%(list)",
            "detail syntax": "%(type): %(used)",
            "silent period": "5m",
            destination: "event",
          },
        },
      ],
    },
    [`${REALTIME}/process`]: {
      kind: "instances",
      fieldGroups: REALTIME_FILTER_FIELD_GROUPS,
      previewColumn: { key: "process", label: "Process" },
      presets: [
        {
          name: "Process is not running",
          description: "Alert when no process with this executable name is running.",
          defaultName: "process_stopped",
          fields: {
            process: "sshd",
            critical: "count == 0",
            severity: "critical",
            "top syntax": "%(list)",
            "detail syntax": "%(exe)",
            "empty message": "sshd is not running",
            destination: "event",
          },
        },
      ],
    },
  },
};

export default CheckSystemUnix;
