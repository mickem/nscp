/**
 * Whether a boolean setting's text is on, read as the agent reads it
 * (`settings_interface::string_to_bool`): `true` in any case, or `1`.
 */
export function isBoolTrue(value: string | undefined): boolean {
  const v = (value ?? "").toLowerCase();
  return v === "true" || v === "1";
}
