// What the NSClient (check_nt) server page knows about NSClientServer's
// settings (modules/NSClientServer/NSClientServer.cpp and
// check_nt_commands.cpp).
import { Settings } from "../../api/api.ts";
import { readStored } from "../kit/changes.ts";
import { fixed, KeyTable, tableIni } from "../kit/keys.ts";
import type { Risk } from "../kit/RiskBanner.tsx";
import { coreServerKeys } from "../kit/socketServer.ts";
import { socketTlsKeys, SocketTlsDefaults } from "../kit/tls.ts";

export const NSCLIENT_MODULE = "NSClientServer";
export const NSCLIENT_PATH = "/settings/NSClient/server";

// The agent registers the socket servers' TLS options here too, with TLS on
// by default.
export const NSCLIENT_TLS: SocketTlsDefaults = {
  ssl: true,
  ciphers: "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH",
  dh: "",
};
const TLS_KEYS = socketTlsKeys(NSCLIENT_TLS);

export const NSCLIENT_TABLE: KeyTable = {
  [NSCLIENT_PATH]: {
    port: { builtIn: fixed("12489") },
    ...coreServerKeys(),
    // Shared with the web UI and other inbound protocols through
    // [/settings/default], where `nscp web install` stores it hashed.
    password: { builtIn: fixed(""), inherits: true },
    allow: { builtIn: fixed("any") },
    "performance data": { builtIn: fixed("true") },
    ...TLS_KEYS,
  },
};

// check_nt has no TLS, so the page leaves the agent's TLS keys to expert mode.
export const NSCLIENT_TLS_KEYS = Object.keys(TLS_KEYS);

// --- Allowed commands ------------------------------------------------------

export interface CommandGroup {
  id: string;
  label: string;
  commands: string[];
  // What a caller learns or reaches with it, when that is more than metrics.
  exposes?: string;
}

// The groups and commands `allow` takes (check_nt_commands.cpp,
// add_allow_token), in the order the picker shows them.
export const COMMAND_GROUPS: CommandGroup[] = [
  {
    id: "metrics",
    label: "System metrics",
    commands: ["cpuload", "uptime", "useddiskspace", "memuse"],
  },
  { id: "info", label: "Agent version", commands: ["clientversion"] },
  {
    id: "service",
    label: "Services",
    commands: ["servicestate"],
    exposes: "which services exist and run",
  },
  {
    id: "process",
    label: "Processes",
    commands: ["procstate"],
    exposes: "which processes run",
  },
  {
    id: "counters",
    label: "Performance counters",
    commands: ["counter", "instances"],
    exposes: "any performance counter, by name",
  },
  {
    id: "files",
    label: "File age",
    commands: ["fileage"],
    exposes: "whether any file exists, and its age",
  },
];

export const ALL_COMMANDS = COMMAND_GROUPS.flatMap((g) => g.commands);

export interface AllowSpec {
  commands: Set<string>;
  // Tokens the agent does not know; it ignores them, and the page keeps them.
  unknown: string[];
}

export function parseAllow(spec: string): AllowSpec {
  const commands = new Set<string>();
  const unknown: string[] = [];
  for (const raw of spec.split(",")) {
    const t = raw.trim().toLowerCase();
    if (t === "") continue;
    if (t === "any" || t === "all") ALL_COMMANDS.forEach((c) => commands.add(c));
    else {
      const group = COMMAND_GROUPS.find((g) => g.id === t);
      if (group) group.commands.forEach((c) => commands.add(c));
      else if (ALL_COMMANDS.includes(t)) commands.add(t);
      else unknown.push(raw.trim());
    }
  }
  return { commands, unknown };
}

// The shortest spelling: `any` for everything, a group when all of its
// commands are in, single commands otherwise.
export function formatAllow({ commands, unknown }: AllowSpec): string {
  const parts: string[] = [];
  if (ALL_COMMANDS.every((c) => commands.has(c))) parts.push("any");
  else
    for (const g of COMMAND_GROUPS) {
      if (g.commands.every((c) => commands.has(c))) parts.push(g.id);
      else parts.push(...g.commands.filter((c) => commands.has(c)));
    }
  return [...parts, ...unknown].join(", ");
}

const sameSet = (a: Set<string>, b: Set<string>) =>
  a.size === b.size && [...a].every((x) => b.has(x));

// The value to store for `next`: what is stored already when it means the
// same, so a spelling the page would write differently is left alone.
export function allowValue(stored: string, next: AllowSpec): string {
  const current = parseAllow(stored);
  return sameSet(current.commands, next.commands) &&
    current.unknown.join(",") === next.unknown.join(",")
    ? stored
    : formatAllow(next);
}

export function allowRisk(commands: Set<string>): Risk | null {
  const reached = COMMAND_GROUPS.filter(
    (g) => g.exposes && g.commands.some((c) => commands.has(c)),
  );
  if (reached.length === 0) return null;
  return {
    severity: "warning",
    title: "check_nt can read more than metrics",
    reason: `Any caller with the password learns ${reached.map((g) => g.exposes).join("; ")}. System metrics and the agent version are enough for most setups.`,
  };
}

export function emptyAllowRisk(commands: Set<string>): Risk | null {
  return commands.size === 0
    ? {
        severity: "warning",
        title: "No command is allowed",
        reason: "check_nt gets a refusal for everything it asks.",
      }
    : null;
}

// --- Risks -----------------------------------------------------------------

export function legacyRisk(): Risk {
  return {
    severity: "warning",
    title: "check_nt is a legacy protocol",
    reason:
      "It sends the password in clear in every request and has no way to authenticate the agent. Prefer REST or NRPE for new setups, and keep the allowed-hosts list tight.",
  };
}

export function passwordRisk(password: string): Risk | null {
  return password === ""
    ? {
        severity: "warning",
        title: "No password: every request is refused",
        reason:
          "Set a password here or in [/settings/default] (nscp web install does) for check_nt to answer.",
      }
    : null;
}

// Whether the agent's TLS is on: the module default is on, which plain
// check_nt clients cannot connect to.
export function tlsOn(stored: Settings[] | undefined): boolean {
  const v = (readStored(stored, NSCLIENT_PATH, "use ssl") ?? "true").toLowerCase();
  return v === "true" || v === "1";
}

export function nsclientIni(stored: Settings[] | undefined, includeDefaults: boolean): string {
  return tableIni(stored, NSCLIENT_MODULE, NSCLIENT_TABLE, includeDefaults);
}
