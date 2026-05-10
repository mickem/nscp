import { SettingsDescription } from "../../api/api.ts";

export interface EnumOption {
  value: string;
  label: string;
}

export const COUNTER_TYPE_OPTIONS: EnumOption[] = ["double", "long", "large", "long long"].map(
  (v) => ({ value: v, label: v }),
);
export const REALTIME_PHYS_OPTIONS: EnumOption[] = ["physical", "committed", "virtual"].map(
  (v) => ({ value: v, label: v }),
);
export const COLLECTION_STRATEGY_OPTIONS: EnumOption[] = [
  { value: "static", label: "static" },
  { value: "rrd", label: "round robin" },
];
export const SEVERITY_OPTIONS: EnumOption[] = ["OK", "WARNING", "CRITICAL", "UNKNOWN"].map(
  (v) => ({ value: v, label: v }),
);
export const PDH_SUBSYSTEM_OPTIONS: EnumOption[] = ["default", "thread-safe"].map((v) => ({
  value: v,
  label: v,
}));
export const COUNTER_FLAGS_OPTIONS = ["nocap100", "1000", "noscale"];

export function toEnumOptions(opts: string[] | undefined): EnumOption[] | undefined {
  return opts?.map((o) => ({ value: o, label: o }));
}

// Best-effort enum extraction from a description text.
// Looks for a "Values: a, b, c" pattern (length 2-8 short tokens).
export function inferEnum(text: string | undefined): string[] | undefined {
  if (!text) return undefined;
  const valuesMatch = text.match(/Values?:\s*([A-Za-z0-9_,\s]+)/i);
  if (valuesMatch) {
    const opts = valuesMatch[1]
      .split(/[,\s]+/)
      .map((s) => s.trim())
      .filter(Boolean);
    if (opts.length >= 2 && opts.length <= 8) return opts;
  }
  return undefined;
}

// Extract a list of options from a parenthetical, e.g. "Extra flags (a, b, c)".
export function inferOptionsFromParens(text: string | undefined): string[] | undefined {
  if (!text) return undefined;
  const m = text.match(/\(([A-Za-z0-9_,\s]+)\)/);
  if (!m) return undefined;
  const opts = m[1]
    .split(/[,\s]+/)
    .map((s) => s.trim())
    .filter(Boolean);
  if (opts.length >= 2 && opts.length <= 8) return opts;
  return undefined;
}

// Resolve the `type` field's allowed values based on which collection the
// instance lives under. Multiple collections expose a `type` field with
// completely different vocabularies (PDH counter type vs realtime resource
// type), so we key off the path.
export function resolveTypeOptions(path: string): EnumOption[] | undefined {
  if (path.includes("/system/windows/counters/")) return COUNTER_TYPE_OPTIONS;
  if (/\/system\/windows\/real-time\/(cpu|memory)\//.test(path)) return REALTIME_PHYS_OPTIONS;
  return undefined;
}

// Resolve the dropdown options for a given description, if any. Returns
// undefined for plain text fields. `parentOptions` (when provided) is used
// for the `parent` field on collection instances.
export function resolveEnumOptions(
  description: SettingsDescription,
  path: string,
  parentOptions?: string[],
): EnumOption[] | undefined {
  if (description.key === "type") {
    return resolveTypeOptions(path) ?? toEnumOptions(inferEnum(description.description));
  }
  if (description.key === "collection strategy") return COLLECTION_STRATEGY_OPTIONS;
  if (description.key === "severity") return SEVERITY_OPTIONS;
  if (description.key === "subsystem" && path === "/settings/system/windows") {
    return PDH_SUBSYSTEM_OPTIONS;
  }
  if (description.key === "parent" && parentOptions) {
    return parentOptions.map((n) => ({ value: n, label: n }));
  }
  return toEnumOptions(inferEnum(description.description));
}
