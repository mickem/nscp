// What the WMI page knows about CheckWMI's settings (modules/CheckWMI,
// include/check/access_policy.hpp).
import { fixed, KeyTable } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import type { TargetSpec } from "../kit/targets.ts";

export const WMI_MODULE = "CheckWMI";
export const WMI_PATH = "/settings/wmi";
export const WMI_QUERIES = `${WMI_PATH}/queries`;
export const WMI_TARGETS = `${WMI_PATH}/targets`;

export const WMI_TABLE: KeyTable = {
  [WMI_PATH]: {
    "query access": { builtIn: fixed("any") },
    "allowed classes": { builtIn: fixed("") },
    "allowed namespaces": { builtIn: fixed("") },
  },
};

export type QueryAccess = "any" | "allowed" | "predefined";

// The agent's spellings of each mode; anything else fails closed to
// predefined, logging the error.
export function queryAccessOf(v: string): QueryAccess | null {
  const s = v.trim().toLowerCase();
  if (["", "any", "all", "unrestricted"].includes(s)) return "any";
  if (["allowed", "allow", "list"].includes(s)) return "allowed";
  if (["predefined", "defined", "named"].includes(s)) return "predefined";
  return null;
}

export function accessRisk(mode: QueryAccess | null, classes: string): Risk | null {
  if (mode === null)
    return {
      severity: "warning",
      title: "The query access setting is not one the agent reads",
      reason: "The agent falls back to predefined queries only, and logs the error. Pick a mode.",
    };
  if (mode === "any")
    return {
      severity: "warning",
      title: "Callers can run any WMI query",
      reason:
        "Anyone who can run check_wmi can read any class in any namespace, on this host or a remote one. Restrict it to the classes you check, or to the queries named below.",
    };
  if (mode === "allowed" && classes.trim() === "")
    return {
      severity: "warning",
      title: "No class is allowed",
      reason: "Every query that is not one of the named queries below is refused.",
    };
  return null;
}

export const WMI_TARGET_SPEC: TargetSpec = {
  moduleId: WMI_MODULE,
  path: WMI_TARGETS,
  noun: "WMI host",
  fields: {
    hostname: {
      label: "Host",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "The computer to query; the target's name when empty.",
    },
    username: {
      label: "User",
      kind: "text",
      builtIn: "",
      inherits: true,
      help: "DOMAIN\\user; the agent's own account when empty.",
    },
    password: { label: "Password", kind: "secret", builtIn: "", inherits: true },
  },
  sections: [
    { title: "Host", keys: ["hostname"] },
    { title: "Login", keys: ["username", "password"] },
  ],
  risks: () => [],
  security: (v) => (v("username") ? `as ${v("username")}` : "as the agent's account"),
};
