// What the Icinga agent page knows about IcingaAgent's settings
// (modules/IcingaAgent).
import { Settings } from "../../api/api.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";

export const ICINGA_MODULE = "IcingaAgent";
export const ICINGA_PATH = "/settings/Icinga/agent";

export const ICINGA_TABLE: KeyTable = {
  [ICINGA_PATH]: {
    "parent host": { builtIn: fixed("") },
    "parent port": { builtIn: fixed("5665") },
    "parent endpoint": { builtIn: fixed("") },
    // The lower-cased host name, worked out by the agent.
    "endpoint name": { builtIn: fixed(""), computedDefault: true },
    "parent fingerprint": { builtIn: fixed("") },
    "trust on first use": { builtIn: fixed("true") },
    ticket: { builtIn: fixed("") },
    "certificate directory": { builtIn: fixed("${certificate-path}/icinga") },
    "certificate request interval": { builtIn: fixed("86400") },
    "allow arguments": { builtIn: fixed("true") },
    threads: { builtIn: fixed("4") },
    "tls version": { builtIn: fixed("1.2+") },
    "allowed ciphers": { builtIn: fixed("") },
  },
};

const isTrue = (v: string) => v.toLowerCase() === "true" || v === "1";

export function offNote(parentHost: string): Risk | null {
  return parentHost.trim() === ""
    ? {
        severity: "warning",
        title: "The agent is off",
        reason: "It connects to nothing until a parent host is set.",
      }
    : null;
}

// A parent presenting a certificate for any other name is refused, so with no
// parent endpoint every connection fails.
export function endpointProblem(parentHost: string, parentEndpoint: string): string | null {
  return parentHost.trim() !== "" && parentEndpoint.trim() === ""
    ? "The parent endpoint (the CN of the parent's certificate) is required; the agent refuses any parent without it."
    : null;
}

export function firstUseRisk(
  parentHost: string,
  trustOnFirstUse: string,
  fingerprint: string,
): Risk | null {
  if (parentHost.trim() === "" || !isTrue(trustOnFirstUse) || fingerprint.trim() !== "")
    return null;
  return {
    severity: "warning",
    title: "The first connection is not verified",
    reason:
      "Until the agent holds the Icinga CA it trusts whoever answers at the parent address, sends it the ticket, and takes its CA. Set the parent fingerprint to pin the parent from the start.",
  };
}

export function icingaIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, ICINGA_MODULE, ICINGA_TABLE, includeDefaults);
}
