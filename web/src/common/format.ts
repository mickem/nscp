/**
 * Locale-independent number formatting for tables.
 *
 * These deliberately avoid toLocaleString(): a grouping separator differs per
 * locale (a Swedish host renders a narrow no-break space), so text built with
 * it cannot be asserted on in a test, and a byte count needs no grouping once
 * it is scaled.
 */

const BYTE_UNITS = ["KiB", "MiB", "GiB", "TiB", "PiB"];

/** A byte count, scaled: "512 B", "2.0 KiB", "1.5 GiB". Keeps the sign of a negative change. */
export function formatBytes(bytes: number): string {
  const sign = bytes < 0 ? "-" : "";
  let value = Math.abs(bytes);
  if (value < 1024) return `${sign}${value} B`;
  let unit = -1;
  while (value >= 1024 && unit < BYTE_UNITS.length - 1) {
    value /= 1024;
    unit++;
  }
  return `${sign}${value.toFixed(1)} ${BYTE_UNITS[unit]}`;
}

/** A float with at most `digits` decimals and no trailing zeros: 2.5, 87.13, 3. */
export function formatDecimal(value: number, digits = 2): string {
  return String(Number(value.toFixed(digits)));
}
