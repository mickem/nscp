// A mode: one choice that writes several keys and reads back from them. The
// keys that match no mode read back as CUSTOM.
import type { ReactNode } from "react";

export const CUSTOM = "custom";
export type Custom = typeof CUSTOM;

export interface ModeOption<M extends string> {
  value: M;
  label: string;
  description?: ReactNode;
}
