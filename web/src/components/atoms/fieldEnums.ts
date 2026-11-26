import { SettingsDescription } from "../../api/api.ts";
import type { EnumOption } from "../../modules/types.ts";
import { findFieldWidget } from "../../modules/registry.ts";

export type { EnumOption };

export const SEVERITY_OPTIONS: EnumOption[] = ["OK", "WARNING", "CRITICAL", "UNKNOWN"].map(
  (v) => ({ value: v, label: v }),
);
export const TLS_VERSION_OPTIONS: EnumOption[] = ["1.0", "1.1", "1.2", "1.3"].map((v) => ({
  value: v,
  label: v,
}));
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

// Resolve the dropdown options for a given description, if any. Returns
// undefined for plain text fields. `parentOptions` (when provided) is used
// for the `parent` field on collection instances.
export function resolveEnumOptions(
  description: SettingsDescription,
  path: string,
  parentOptions?: string[],
): EnumOption[] | undefined {
  // A dropdown declared by the module page that owns the path wins.
  const widget = findFieldWidget(path, description.key);
  if (widget?.kind === "select") return widget.options;
  if (description.key === "severity") return SEVERITY_OPTIONS;
  if (description.key === "tls version") return TLS_VERSION_OPTIONS;
  if (description.key === "parent" && parentOptions) {
    return parentOptions.map((n) => ({ value: n, label: n }));
  }
  return toEnumOptions(inferEnum(description.description));
}
