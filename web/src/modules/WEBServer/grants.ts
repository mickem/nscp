// The grants a web role can hold: every name the web server's controllers ask
// for (modules/WEBServer/*_controller.cpp), grouped by area.
//
// A role's grant string is a comma-separated list. Each grant is matched
// segment by segment (grant_store::validate_grants): `queries.execute` grants
// exactly that, and a `*` segment grants everything below it - `*` alone is
// every grant, `modules.*` every `modules.…` grant. `queries` does not grant
// `queries.execute`.

export interface Grant {
  id: string;
  label: string;
  // Lets the holder change the agent, run arbitrary commands, or pass
  // arguments to checks.
  powerful?: boolean;
}

export interface GrantArea {
  id: string;
  label: string;
  grants: Grant[];
  // A `<area>.*` wildcard is offered when the area has grants below a prefix.
  wildcard?: string;
}

// Script grants are per runtime: scripts.<verb>.<runtime>.
const RUNTIMES = ["ext", "py", "lua"];
const perRuntime = (verb: string, label: string, powerful?: boolean): Grant[] =>
  RUNTIMES.map((r) => ({ id: `scripts.${verb}.${r}`, label: `${label} (${r})`, powerful }));

export const GRANT_AREAS: GrantArea[] = [
  {
    id: "basics",
    label: "Basics",
    grants: [
      { id: "public", label: "The API index" },
      { id: "login.get", label: "Log in" },
      { id: "info.get", label: "Agent information" },
      { id: "info.get.version", label: "Agent version" },
    ],
  },
  {
    id: "queries",
    label: "Checks",
    wildcard: "queries.*",
    grants: [
      { id: "queries.list", label: "List checks" },
      { id: "queries.get", label: "Describe a check" },
      { id: "queries.execute", label: "Run checks, with arguments", powerful: true },
      { id: "queries.execute.noargs", label: "Run checks, without arguments" },
      { id: "aliases.list", label: "List aliases" },
    ],
  },
  {
    id: "metrics",
    label: "Metrics",
    grants: [
      { id: "metrics.list", label: "Metrics" },
      { id: "openmetrics.list", label: "OpenMetrics (Prometheus)" },
    ],
  },
  {
    id: "observe",
    label: "Logs, events and failures",
    grants: [
      { id: "logs.list", label: "Read the log" },
      { id: "logs.put", label: "Write to the log" },
      { id: "logs.delete", label: "Clear the log" },
      { id: "events.list", label: "Read events" },
      { id: "events.delete", label: "Drain events" },
      { id: "failures.list", label: "Read failed checks" },
      { id: "failures.delete", label: "Clear failed checks" },
      { id: "diagnostics.list", label: "Read diagnostics" },
      { id: "results.list", label: "List passive results" },
      { id: "results.get", label: "Read passive results" },
      { id: "results.delete", label: "Drop passive results" },
    ],
  },
  {
    id: "facts",
    label: "Facts",
    grants: [
      { id: "facts.get", label: "Read facts" },
      { id: "facts.refresh", label: "Refresh facts" },
      { id: "tags.get", label: "Read tags" },
      { id: "metadata.list", label: "Metadata index" },
      { id: "metadata.get.channels", label: "Channel metadata" },
      { id: "metadata.get.counters", label: "Counter metadata" },
    ],
  },
  {
    id: "modules",
    label: "Modules",
    wildcard: "modules.*",
    grants: [
      { id: "modules", label: "The modules API" },
      { id: "modules.list", label: "List modules" },
      { id: "modules.get", label: "Describe a module" },
      { id: "modules.load", label: "Load modules", powerful: true },
      { id: "modules.unload", label: "Unload modules", powerful: true },
      { id: "modules.enable", label: "Enable modules", powerful: true },
      { id: "modules.disable", label: "Disable modules", powerful: true },
      { id: "modules.post", label: "Module commands (POST)", powerful: true },
      { id: "modules.put", label: "Module changes (PUT)", powerful: true },
    ],
  },
  {
    id: "settings",
    label: "Settings",
    wildcard: "settings.*",
    grants: [
      { id: "settings.get", label: "Read settings" },
      { id: "settings.put", label: "Change settings", powerful: true },
      { id: "settings.delete", label: "Delete settings", powerful: true },
    ],
  },
  {
    id: "scripts",
    label: "Scripts",
    wildcard: "scripts.*",
    grants: [
      { id: "scripts", label: "The scripts API" },
      { id: "scripts.list.runtimes", label: "List runtimes" },
      ...perRuntime("lists", "List scripts"),
      ...perRuntime("get", "Read scripts"),
      ...perRuntime("add", "Upload scripts", true),
      ...perRuntime("delete", "Delete scripts", true),
    ],
  },
  {
    id: "console",
    label: "Console",
    grants: [{ id: "console.exec", label: "Run console commands", powerful: true }],
  },
  {
    id: "legacy",
    label: "Legacy API",
    grants: [{ id: "legacy", label: "The pre-v1 API: runs any command", powerful: true }],
  },
];

export const KNOWN_GRANTS = GRANT_AREAS.flatMap((a) => a.grants);
const KNOWN_IDS = new Set(KNOWN_GRANTS.map((g) => g.id));
const WILDCARDS = new Set(GRANT_AREAS.flatMap((a) => (a.wildcard ? [a.wildcard] : [])));

export function parseGrants(value: string): string[] {
  return value
    .split(",")
    .map((g) => g.trim())
    .filter((g) => g !== "");
}

export function formatGrants(grants: string[]): string {
  return grants.join(",");
}

// Whether `rule` grants `need`, as the agent decides it.
export function grants(rule: string, need: string): boolean {
  const r = rule.split(".");
  const n = need.split(".");
  for (let i = 0; i < n.length; ++i) {
    if (i >= r.length) return false;
    if (r[i] === "*") return true;
    if (r[i] !== n[i]) return false;
  }
  return r.length === n.length;
}

export function holds(role: string[], need: string): boolean {
  return role.some((rule) => grants(rule, need));
}

// Grants in a role the picker does not offer; they are kept as they are.
export function otherGrants(role: string[]): string[] {
  return role.filter((g) => g !== "*" && !KNOWN_IDS.has(g) && !WILDCARDS.has(g));
}

// The powerful grants a role holds, by label.
export function powerfulHeld(role: string[]): string[] {
  return KNOWN_GRANTS.filter((g) => g.powerful && holds(role, g.id)).map((g) => g.label);
}

// The built-in roles (docs/docs/setup/web-interface.md), as starting points.
export const ROLE_PRESETS: Array<{ name: string; label: string; grants: string }> = [
  { name: "full", label: "Full access", grants: "*" },
  {
    name: "client",
    label: "Monitoring client that also browses the agent",
    grants:
      "public,info.get,info.get.version,queries.list,queries.get,queries.execute,aliases.list,login.get,modules.list",
  },
  {
    name: "monitoring",
    label: "Checks with arguments, and metrics",
    grants: "public,queries.execute,aliases.list,login.get,metrics.list,openmetrics.list",
  },
  {
    name: "restricted",
    label: "Checks without arguments",
    grants: "public,queries.execute.noargs,aliases.list,login.get",
  },
  {
    name: "metrics",
    label: "Metrics only (a Prometheus scraper)",
    grants: "public,metrics.list,openmetrics.list,login.get",
  },
];
