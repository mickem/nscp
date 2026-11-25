import type { ModulePage } from "../types.ts";
import { plainOptions } from "../shared/options.ts";
import { REALTIME_FILTER_FIELD_GROUPS } from "../shared/realtime.ts";

const ROOT = "/settings/system/windows";
const COUNTERS = `${ROOT}/counters`;
const REALTIME = `${ROOT}/real-time`;

const CheckSystem: ModulePage = {
  id: "CheckSystem",
  sections: {
    [ROOT]: {
      subsystem: { kind: "select", options: plainOptions(["default", "thread-safe"]) },
    },
  },
  collections: {
    [COUNTERS]: {
      kind: "instances",
      fieldGroups: [
        { name: "Source", keys: ["counter", "instances", "type", "flags"] },
        { name: "Storage", keys: ["collection strategy", "buffer size"] },
      ],
      previewColumn: { key: "counter", label: "Counter" },
      fields: {
        counter: { kind: "counter" },
        type: { kind: "select", options: plainOptions(["double", "long", "large", "long long"]) },
        flags: { kind: "multi", options: ["nocap100", "1000", "noscale"] },
        "collection strategy": {
          kind: "select",
          options: [
            { value: "static", label: "static" },
            { value: "rrd", label: "round robin" },
          ],
        },
      },
      // The buffer only exists for the round-robin strategy.
      isFieldVisible: (key, { fields }) => {
        if (key !== "buffer size") return undefined;
        return (fields.find((f) => f.key === "collection strategy")?.value ?? "") === "rrd";
      },
    },
    // Superseded by the per-resource collections below.
    [`${REALTIME}/checks`]: { kind: "hidden" },
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
            filter: "total > 80%",
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
      // Any combination of memory types, as a comma-separated list.
      fields: { type: { kind: "multi", options: ["physical", "committed", "virtual"] } },
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
            "detail syntax": "%(type): %(used_pct)%",
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
          name: "Any process has started",
          description: "Alert when a new instance of any process appears.",
          defaultName: "process_started",
          fields: {
            process: "*",
            filter: "new = 1",
            warning: "count>0",
            severity: "ok",
            "top syntax": "%(list)",
            "detail syntax": "%(exe)",
            destination: "event",
          },
        },
        {
          name: "Process has stopped",
          description: "Alert when a named process is no longer running.",
          defaultName: "process_stopped",
          fields: {
            process: "CalculatorApp",
            filter: "state=stopped",
            critical: "count==0",
            severity: "critical",
            "top syntax": "%(list)",
            "detail syntax": "%(exe)",
            destination: "event",
          },
        },
      ],
    },
  },
};

export default CheckSystem;
