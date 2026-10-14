/**
 * The estate roles (build/powershell/estate/roles/<role>/role.json) as the
 * tests read them: what a role checks, how a result is judged, and the
 * bundle it ships.
 *
 * Two suites use this. live/estate.test.ts runs a role's checks against the
 * Azure estate: real packages, a real fleet server, real machines - and so
 * only when someone runs the estate. estate-roles-local.test.ts runs the same
 * role on every CI build: its bundle, built here the way the estate builds
 * it, served by a fake fleet server to a local agent, and judged by the same
 * functions. That keeps a role's descriptor and the judging code honest
 * between estate runs, so an estate run only has the real infrastructure left
 * to prove.
 */
import * as fs from "fs";
import * as path from "path";
import { POWERSHELL_DIR, readJson } from "./estate-manifest";
import type { ZipEntry } from "./fleet";

export const ESTATE_ROLES_DIR = path.join(POWERSHELL_DIR, "estate", "roles");

export interface RoleCheck {
  command: string;
  args?: Record<string, string | string[]>;
  expect?: string;
  perf?: string[];
  /** Text the message must contain (case-insensitively). */
  message?: string;
  os?: string[];
  /** Run once per machine of this role in the same manifest (a hub's targets). */
  each?: string;
}

/**
 * A request to the agent's own API, for what a role delivers that no check
 * reports: the scripts a runtime lists under /api/v2/scripts, what one of
 * them holds, or a script added and removed again. Entries run in order, so
 * a PUT can be followed by the GET that proves it landed.
 */
export interface RoleRest {
  /** GET (default), PUT or DELETE. */
  method?: "GET" | "PUT" | "DELETE";
  path: string;
  /** What a PUT sends, as text/plain. */
  body?: string;
  /** The HTTP status expected (default 200). */
  status?: number;
  /** Text the body must contain, case-insensitively; `\` reads as `/`. */
  contains?: string[];
  /** Text the body must not contain, read the same way. */
  excludes?: string[];
  os?: string[];
}

export interface Role {
  name: string;
  os: string[];
  modules?: string[];
  install?: Record<string, string>;
  localSettings?: Record<string, Record<string, string>>;
  bundle?: { settings?: Record<string, unknown>; scripts?: string };
  checks?: RoleCheck[];
  rest?: RoleRest[];
}

export function readRole(name: string, rolesDir = ESTATE_ROLES_DIR): Role {
  return readJson<Role>(path.join(rolesDir, name, "role.json"));
}

export const ESTATE_STATES = ["OK", "WARNING", "CRITICAL", "UNKNOWN"];

/** No "os" is everywhere; "linux" is every non-Windows family. */
export function applies(entry: { os?: string[] }, family: string): boolean {
  if (!entry.os || entry.os.length === 0) return true;
  if (entry.os.includes(family)) return true;
  return family !== "windows" && entry.os.includes("linux");
}

/**
 * Passes when the state is the expected one ("any" accepts every state but
 * UNKNOWN, which is what a missing module, a bad argument or an unreachable
 * workload returns), every name in "perf" occurs in some perfdata key,
 * case-insensitively, and the message contains "message" when the check
 * names one. Returns why the result fails, or undefined.
 */
export function judgeCheck(
  check: { expect?: string; perf?: string[]; message?: string },
  result: number,
  perfKeys: string[],
  message = "",
): string | undefined {
  const expect = check.expect ?? "OK";
  const state = ESTATE_STATES[result];
  if (!state) return `no result state (got ${result})`;
  if (expect === "any" && state === "UNKNOWN")
    return "returned UNKNOWN (expected any state but UNKNOWN)";
  if (expect !== "any" && state !== expect) return `returned ${state}, expected ${expect}`;
  const missing = (check.perf ?? []).filter(
    (want) => !perfKeys.some((k) => k.toLowerCase().includes(want.toLowerCase())),
  );
  if (missing.length > 0)
    return `no perfdata matching ${missing.join(", ")} (got: ${perfKeys.join(", ") || "none"})`;
  if (check.message && !message.toLowerCase().includes(check.message.toLowerCase()))
    return `message does not contain "${check.message}"`;
  return undefined;
}

/**
 * Passes when the response has the expected status and its body, with every
 * run of backslashes read as `/` (Windows lists `scripts\x.bat`, and JSON
 * doubles the backslash), contains every `contains` and no `excludes`,
 * case-insensitively. Returns why it fails, or undefined.
 */
export function judgeRest(r: RoleRest, status: number, text: string): string | undefined {
  const want = r.status ?? 200;
  if (status !== want) return `answered HTTP ${status}, expected ${want}`;
  const norm = (s: string) => s.replace(/\\+/g, "/").toLowerCase();
  const body = norm(text);
  const missing = (r.contains ?? []).filter((s) => !body.includes(norm(s)));
  if (missing.length > 0) return `body does not contain ${missing.map((s) => `"${s}"`).join(", ")}`;
  const present = (r.excludes ?? []).filter((s) => body.includes(norm(s)));
  if (present.length > 0) return `body contains ${present.map((s) => `"${s}"`).join(", ")}`;
  return undefined;
}

/** How a REST entry is named in test titles and checks.json. */
export function restLabel(r: RoleRest): string {
  return `${r.method ?? "GET"} ${r.path}`;
}

export interface RestOutcome {
  why?: string;
  res: { status: number; text: string } | null;
}

/**
 * Send a REST entry until it passes or `waitMs` is over, and judge it. A
 * dropped connection or a 401/403 is retried after `relogin`: the reload
 * that applies a bundle restarts the web server, which forgets its sessions.
 * A GET is also retried while it fails, since what it reads may still be on
 * its way; a PUT or DELETE that got an answer is not, because sending it again
 * would change what it is judged on (a second DELETE finds nothing to delete).
 */
export async function attemptRoleRest(
  r: RoleRest,
  send: () => Promise<{ status: number; text: string }>,
  relogin: () => Promise<void>,
  waitMs: number,
  pauseMs = 5_000,
): Promise<RestOutcome> {
  const deadline = Date.now() + waitMs;
  for (;;) {
    let outcome: RestOutcome;
    let answered = false;
    try {
      const res = await send();
      if (res.status === 401 || res.status === 403) {
        outcome = { res, why: `answered HTTP ${res.status} (session lost)` };
        await relogin().catch(() => undefined);
      } else {
        outcome = { res, why: judgeRest(r, res.status, res.text) };
        answered = true;
      }
    } catch (e) {
      outcome = { res: null, why: `request failed: ${String(e).split("\n")[0]}` };
      await relogin().catch(() => undefined);
    }
    if (!outcome.why || Date.now() >= deadline) return outcome;
    if (answered && (r.method ?? "GET") !== "GET") return outcome;
    await new Promise((resolve) => setTimeout(resolve, pauseMs));
  }
}

/**
 * The bundle config the estate composes for a role: its modules enabled and
 * its bundle settings with ${ROLE_PASSWORD} filled in - the TypeScript twin of
 * ConvertTo-EstateBundleConfig (estate-api.ps1). A role that names its
 * companions' addresses (${ESTATE_IPS:...}) needs a manifest and is refused.
 */
export function roleBundleConfig(role: Role, rolePassword: string): Record<string, unknown> {
  const modules = Object.fromEntries((role.modules ?? []).map((m) => [m, "enabled"]));
  const config: Record<string, unknown> = { modules };
  if (role.bundle?.settings) {
    const json = JSON.stringify(role.bundle.settings).split("${ROLE_PASSWORD}").join(rolePassword);
    const left = /\$\{[A-Za-z_:]+\}/.exec(json);
    if (left)
      throw new Error(`Role '${role.name}': ${left[0]} cannot be filled in without an estate`);
    config.settings = JSON.parse(json);
  }
  return config;
}

/**
 * The entries of the bundle zip the estate uploads for a role that ships
 * scripts - the TypeScript twin of New-EstateBundleZip: bundle.toml,
 * config.json and every file under the role's scripts folder as
 * `scripts/<relative path>`, shell scripts with LF line endings.
 */
export function roleBundleEntries(
  role: Role,
  version: string,
  rolePassword: string,
  rolesDir = ESTATE_ROLES_DIR,
): ZipEntry[] {
  const entries: ZipEntry[] = [
    {
      name: "bundle.toml",
      data: `name = "${role.name}"\nversion = "${version}"\nschema_version = 1\n`,
    },
    { name: "config.json", data: JSON.stringify(roleBundleConfig(role, rolePassword)) },
  ];
  if (!role.bundle?.scripts) return entries;
  const root = path.join(rolesDir, role.name, role.bundle.scripts);
  const walk = (dir: string): string[] =>
    fs
      .readdirSync(dir, { withFileTypes: true })
      .flatMap((d) => (d.isDirectory() ? walk(path.join(dir, d.name)) : [path.join(dir, d.name)]));
  for (const file of walk(root).sort()) {
    const rel = path.relative(root, file).split(path.sep).join("/");
    let data: Buffer = fs.readFileSync(file);
    if (file.endsWith(".sh")) data = Buffer.from(data.toString("utf8").replace(/\r\n/g, "\n"));
    entries.push({ name: `scripts/${rel}`, data });
  }
  return entries;
}
