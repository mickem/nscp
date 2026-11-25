// Shared layout for real-time filter collections (CheckSystem,
// CheckSystemUnix, CheckEventLog, CheckLogFile): one instance per filter,
// with the same source / filter / display / target split.
import type { FieldGroup } from "../types.ts";

export const REALTIME_FILTER_FIELD_GROUPS: FieldGroup[] = [
  { name: "Source", keys: ["process", "time", "type", "log", "file", "files", "column-split"] },
  { name: "Filter", keys: ["filter", "warning", "critical", "ok"] },
  {
    name: "Display",
    keys: [
      "top syntax",
      "ok syntax",
      "detail syntax",
      "severity",
      "perf config",
      "silent period",
      "empty message",
      "maximum age",
      "run on startup",
      "escape html",
      "debug",
    ],
  },
  { name: "Target", keys: ["destination", "target id", "source id", "command"] },
];
