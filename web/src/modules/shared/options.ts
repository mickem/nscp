import type { EnumOption } from "../types.ts";

// Options whose value is also their label.
export function plainOptions(values: string[]): EnumOption[] {
  return values.map((v) => ({ value: v, label: v }));
}

// The `tlsvX.Y` vocabulary (plus the `tlsv1.2+` "1.2 or newer" shorthand)
// read by the Boost.Asio / OpenSSL-backed code paths. Other backends take the
// bare version numbers, which is the generic default.
export const ASIO_TLS_VERSION_OPTIONS: EnumOption[] = plainOptions([
  "tlsv1.0",
  "tlsv1.1",
  "tlsv1.2",
  "tlsv1.2+",
  "tlsv1.3",
]);
