// What the Scheduler pages know about its settings (modules/Scheduler).
//
// Every schedule key inherits: a schedule takes what it leaves out from its
// parent (`default` unless it names another), and only the `default` schedule
// has built-in values (channel NSCA, report all, run on startup false) -
// schedules_handler.hpp registers the keys with a default there and without
// one everywhere else.
import { Settings } from "../../api/api.ts";
import { readStored } from "../kit/changes.ts";
import { Entry, readEntries } from "../kit/entries.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";

export const SCHEDULER_MODULE = "Scheduler";
export const SCHEDULER_PATH = "/settings/scheduler";
export const SCHEDULES = `${SCHEDULER_PATH}/schedules`;
export const DEFAULT_SCHEDULE = "default";

export const SCHEDULER_TABLE: KeyTable = {
  [SCHEDULER_PATH]: {
    threads: { builtIn: fixed("5") },
    "startup window": { builtIn: fixed("0s") },
    timezone: { builtIn: fixed("local") },
  },
};

export function schedulerIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, SCHEDULER_MODULE, SCHEDULER_TABLE, includeDefaults);
}

export const SCHEDULE_KEYS = [
  "command",
  "interval",
  "schedule",
  "randomness",
  "channel",
  "target",
  "source",
  "report",
  "run on startup",
  "parent",
  "alias",
  "is template",
] as const;

const BUILT_IN: Record<string, string> = {
  channel: "NSCA",
  report: "all",
  "run on startup": "false",
};

export function readSchedules(stored: Settings[] | undefined): Entry[] {
  return readEntries(stored, SCHEDULES, "command");
}

// A schedule's value for a key: its own, else its parent's, else the
// built-in one the default schedule has.
export function scheduleValue(
  stored: Settings[] | undefined,
  name: string,
  key: string,
  own: Record<string, string> = {},
  seen: Set<string> = new Set(),
): string {
  if (own[key] !== undefined && own[key] !== "") return own[key];
  if (name === DEFAULT_SCHEDULE) {
    return readStored(stored, `${SCHEDULES}/${DEFAULT_SCHEDULE}`, key) ?? BUILT_IN[key] ?? "";
  }
  seen.add(name);
  const parent = own.parent || DEFAULT_SCHEDULE;
  if (seen.has(parent)) return BUILT_IN[key] ?? "";
  const parentOwn = parent === DEFAULT_SCHEDULE ? {} : (readEntryValues(stored, parent) ?? {});
  return scheduleValue(stored, parent, key, parentOwn, seen);
}

function readEntryValues(stored: Settings[] | undefined, name: string) {
  return readSchedules(stored).find((e) => e.name === name)?.values;
}

// --- Formats ------------------------------------------------------------------

// An interval: digits and a unit by its first letter - s, m, h, d, w - or
// seconds without one (str::format::stox_as_time_sec).
export function intervalSeconds(v: string): number | null {
  const m = /^\s*(\d+)\s*([a-z]*)\s*$/i.exec(v);
  if (!m) return null;
  const n = Number(m[1]);
  const unit = m[2].toLowerCase()[0] ?? "s";
  const scale: Record<string, number> = { s: 1, m: 60, h: 3600, d: 86400, w: 604800 };
  return unit in scale ? n * scale[unit] : null;
}

// A cron line as the agent reads it: five fields of * or a comma list of
// numbers, no ranges, steps or names (include/parsers/cron/cron_parser.hpp).
const CRON_FIELDS: Array<[string, number, number]> = [
  ["minute", 0, 59],
  ["hour", 0, 23],
  ["day of month", 1, 31],
  ["month", 1, 12],
  ["day of week", 0, 6],
];

export function cronProblem(v: string): string | null {
  const fields = v.trim().split(/\s+/);
  if (fields.length !== 5)
    return "A schedule has five fields: minute, hour, day of month, month, day of week.";
  for (let i = 0; i < 5; ++i) {
    const [name, lo, hi] = CRON_FIELDS[i];
    if (fields[i] === "*") continue;
    for (const part of fields[i].split(",")) {
      if (!/^\d+$/.test(part))
        return `The ${name} field takes * or numbers separated by commas; ranges, steps and names are not read.`;
      const n = Number(part);
      if (n < lo || n > hi) return `The ${name} field runs from ${lo} to ${hi}.`;
    }
  }
  return null;
}

export const REPORTS = ["ok", "warning", "critical", "unknown"] as const;

// The results a `report` value submits (nscapi_helper.cpp: case-insensitive,
// warn/crit/err spellings, unknown words ignored).
export function reportSet(v: string): Set<string> {
  const out = new Set<string>();
  for (const w of v.split(",").map((x) => x.trim().toLowerCase())) {
    if (w === "all") REPORTS.forEach((r) => out.add(r));
    else if (w === "ok") out.add("ok");
    else if (w === "warning" || w === "warn") out.add("warning");
    else if (w === "critical" || w === "crit" || w === "error" || w === "err") out.add("critical");
    else if (w === "unknown") out.add("unknown");
  }
  return out;
}

export function reportValue(set: Set<string>): string {
  return REPORTS.every((r) => set.has(r)) ? "all" : REPORTS.filter((r) => set.has(r)).join(",");
}

export const isTemplate = (values: Record<string, string>) =>
  ["true", "1"].includes((values["is template"] ?? "").toLowerCase());

// Why the agent would refuse a schedule with these effective values, or null.
// A template (`default`, or one marked so) is never run: only what is set on
// it is checked.
export function scheduleProblem(get: (key: string) => string, isDefault: boolean): string | null {
  const interval = get("interval");
  const cron = get("schedule");
  if (interval && cron)
    return isDefault
      ? "Set an interval or a schedule, not both: every schedule that sets neither would be refused."
      : "This schedule has both an interval and a schedule (one may come from its parent): the agent refuses it.";
  if (interval) {
    const s = intervalSeconds(interval);
    if (s === null) return "An interval is a number and a unit: 30s, 5m, 1h, 1d, 1w.";
    if (s === 0) return "An interval of 0 is refused.";
  }
  if (cron) return cronProblem(cron);
  if (!isDefault && !get("command").trim()) return "Give the command it runs.";
  if (!isDefault && !interval) return "Give it an interval or a schedule.";
  return null;
}

export function describeWhen(get: (key: string) => string): string {
  const interval = get("interval");
  const cron = get("schedule");
  const r = get("randomness").replace("%", "");
  const jitter = r && r !== "0" ? ` ±${r}%` : "";
  if (interval) return `every ${interval}${jitter}`;
  if (cron) return `at ${cron}`;
  return "not scheduled";
}
