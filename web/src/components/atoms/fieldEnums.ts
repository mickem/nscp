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
export const TLS_VERSION_OPTIONS: EnumOption[] = ["1.0", "1.1", "1.2", "1.3"].map((v) => ({
  value: v,
  label: v,
}));
// The self-update HTTPS client uses a different `tls version` vocabulary —
// `tlsv1.x` prefixes plus a `tlsv1.2+` shorthand for "1.2 or newer".
export const NSCP_UPDATE_TLS_VERSION_OPTIONS: EnumOption[] = [
  "tlsv1.0",
  "tlsv1.1",
  "tlsv1.2",
  "tlsv1.2+",
  "tlsv1.3",
].map((v) => ({ value: v, label: v }));
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
  if (description.key === "tls version") {
    // Boost.Asio / OpenSSL-backed code paths use `tlsvX.Y` (plus the
    // `tlsv1.2+` "or newer" shorthand). Other backends just want bare
    // version numbers.
    if (
      path === "/settings/nscp/check/update" ||
      path === "/settings/NRPE/server"
    ) {
      return NSCP_UPDATE_TLS_VERSION_OPTIONS;
    }
    return TLS_VERSION_OPTIONS;
  }
  if (description.key === "parent" && parentOptions) {
    return parentOptions.map((n) => ({ value: n, label: n }));
  }
  return toEnumOptions(inferEnum(description.description));
}
