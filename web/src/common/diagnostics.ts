import { DiagnosticsColumn } from "../api/api.ts";
import { formatBytes, formatDecimal } from "./format.ts";

/** The section every snapshot holds, and the one to read first: the run-up to the failure. */
const FIRST_SECTION = "failures.recent";

/** Section names in reading order: `failures.recent` first, then alphabetical. */
export function orderSectionNames(names: string[]): string[] {
  return [...names].sort((a, b) => {
    if (a === FIRST_SECTION) return b === FIRST_SECTION ? 0 : -1;
    if (b === FIRST_SECTION) return 1;
    return a.localeCompare(b);
  });
}

/** Shown for a null cell: a value the producer did not have. */
export const NULL_CELL = "-";

export type DiagnosticsCell = string | number | boolean | null | undefined;

/** A timestamp as the browser's local date and time; the ISO text if it does not parse. */
export function formatTimestamp(iso: string): string {
  const when = new Date(iso);
  return isNaN(when.getTime()) ? iso : when.toLocaleString();
}

/** A cell as text, by its column's type and unit. */
export function formatCell(column: DiagnosticsColumn, value: DiagnosticsCell): string {
  if (value === null || value === undefined) return NULL_CELL;
  if (typeof value === "boolean") return value ? "yes" : "no";
  if (typeof value === "string") {
    return column.type === "timestamp" ? formatTimestamp(value) : value;
  }
  switch (column.unit) {
    case "bytes":
      return formatBytes(value);
    case "percent":
      return `${value.toFixed(1)} %`;
    case undefined:
    case "":
      return column.type === "float" ? formatDecimal(value) : String(value);
    default:
      return `${column.type === "float" ? formatDecimal(value) : String(value)} ${column.unit}`;
  }
}
