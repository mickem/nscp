/**
 * Role-aware live suite for the Azure estate (build/powershell/run-estate.ps1).
 *
 * For every machine that is up in the estate manifests, logs in to its agent
 * and runs the checks its role declares in
 * build/powershell/estate/roles/<role>/role.json, then the agent checks every
 * live target must pass (src/live-smoke.ts). This is the estate's check
 * runner: the gate (estate/verify-estate.ps1) waits for the fleet sync and
 * then runs this suite, taking each machine's verdict from the checks.json it
 * writes. By hand, against an estate kept with -KeepVms:
 *
 *   npm run test:estate                  # every estate that is up
 *   npm run test:estate -- -t mssql      # one role's machines
 *
 *   NSCP_ESTATE_MANIFEST   the manifest(s), joined with the path delimiter.
 *                          `npm run test:estate` finds them when unset (see
 *                          src/estate-manifest.ts); under `npm run test:live`
 *                          the suite is skipped without one.
 *   NSCP_ESTATE_ROLES      the roles directory. Default
 *                          build/powershell/estate/roles.
 *   NSCP_ESTATE_MACHINES   only these machines (comma-separated names).
 *   NSCP_ESTATE_CHECK_WAIT_SECONDS
 *                          how long one check is retried before it fails
 *                          (default 60): a collector warming up or a bundle
 *                          reload restarting the web server is waited out.
 *   NSCP_ESTATE_RESULTS_DIR
 *                          write <dir>/<machine>/checks.json: per check the
 *                          verdict, the reason and the raw REST result of
 *                          the last attempt.
 *
 * Each machine's address and web password come from the .vm.<name>.pwd file
 * the manifest points at, so the suite runs from the machine that provisioned
 * the estate.
 */
import * as fs from "fs";
import * as path from "path";
import { spawnSync } from "child_process";
import { liveLogin, liveExecute, readPwdFile, type LiveTarget } from "@fixtures/live-target";
import { smokeCasesFor } from "@fixtures/live-smoke";
import { messageOf, perfOf, type QueryResult } from "@fixtures/queries";
import {
  liveMachines,
  readJson,
  POWERSHELL_DIR,
  type ManifestEntry,
} from "@fixtures/estate-manifest";

interface RoleCheck {
  command: string;
  args?: Record<string, string | string[]>;
  expect?: string;
  perf?: string[];
  os?: string[];
  /** Run once per machine of this role in the same manifest (a hub's targets). */
  each?: string;
}

/** A role check made concrete for one machine: an "each" check is one of these per target. */
interface RunCheck {
  label: string;
  command: string;
  /** What is sent, with ${target.*} filled in. */
  args: Record<string, string | string[]>;
  /** What checks.json records: the role's own arguments, so no password is written out. */
  shownArgs: Record<string, string | string[]>;
  /** Why it cannot run at all (an "each" check with no target up). */
  unrunnable?: string;
  expect: string;
  perf: string[];
}

interface Machine {
  entry: ManifestEntry;
  /** Every machine of its manifest that is up, itself included. */
  peers: ManifestEntry[];
}

interface CheckRecord {
  check: string;
  command: string;
  args: Record<string, string | string[]>;
  expect: string;
  perf: string[];
  passed: boolean;
  state: string | null;
  reason: string | null;
  message: string;
  perfKeys: string[];
  response: QueryResult | null;
}

const STATES = ["OK", "WARNING", "CRITICAL", "UNKNOWN"];

const manifests = (process.env.NSCP_ESTATE_MANIFEST ?? "")
  .split(path.delimiter)
  .filter((p) => p.trim());
const rolesDir = process.env.NSCP_ESTATE_ROLES ?? path.join(POWERSHELL_DIR, "estate", "roles");
const only = (process.env.NSCP_ESTATE_MACHINES ?? "")
  .split(",")
  .map((n) => n.trim())
  .filter((n) => n);
const waitSeconds = Number(process.env.NSCP_ESTATE_CHECK_WAIT_SECONDS);
const waitMs = (Number.isFinite(waitSeconds) && waitSeconds >= 0 ? waitSeconds : 60) * 1000;
const resultsDir = process.env.NSCP_ESTATE_RESULTS_DIR;
const checkNsclientExe = process.env.NSCP_ESTATE_CHECK_NSCLIENT || undefined;

// A check may retry for the whole wait, plus the request that ends it.
jest.setTimeout(waitMs + 90_000);

function machines(): Machine[] {
  const all = manifests.flatMap((file) => {
    const peers = liveMachines(file);
    return peers.map((entry) => ({ entry, peers }));
  });
  return only.length > 0 ? all.filter((m) => only.includes(m.entry.Name)) : all;
}

function roleChecks(role: string): RoleCheck[] {
  return readJson<{ checks?: RoleCheck[] }>(path.join(rolesDir, role, "role.json")).checks ?? [];
}

/** No "os" is everywhere; "linux" is every non-Windows family. */
function applies(check: RoleCheck, family: string): boolean {
  if (!check.os || check.os.length === 0) return true;
  if (check.os.includes(family)) return true;
  return family !== "windows" && check.os.includes("linux");
}

/** One KEY=VALUE token per value: an array repeats the key, as REST does. */
function argTokens(args: Record<string, string | string[]>): string[] {
  return Object.entries(args).flatMap(([k, v]) => [v].flat().map((one) => `${k}=${one}`));
}

/** For a label: an array shown as one comma-joined value. */
function argText(args: Record<string, string | string[]>): string[] {
  return Object.entries(args).map(([k, v]) => `${k}=${[v].flat().join(",")}`);
}

/**
 * The role's checks for one machine. An "each" check becomes one run per
 * machine of that role in the same manifest, with ${target.ip},
 * ${target.name} and ${target.password} (its web password, from its pwd
 * file) filled in; with no such machine it becomes a run that fails saying so.
 */
function runChecks(checks: RoleCheck[], peers: ManifestEntry[]): RunCheck[] {
  const base = (
    c: RoleCheck,
    label: string,
    args: Record<string, string | string[]>,
  ): RunCheck => ({
    label,
    command: c.command,
    args,
    shownArgs: c.args ?? {},
    expect: c.expect ?? "OK",
    perf: c.perf ?? [],
  });
  return checks.flatMap((c) => {
    const plain = [c.command, ...argText(c.args ?? {})].join(" ");
    if (!c.each) return [base(c, plain, c.args ?? {})];
    const targets = peers.filter((p) => (p.Role || "base") === c.each);
    if (targets.length === 0)
      return [
        { ...base(c, plain, {}), unrunnable: `no '${c.each}' machine is up in the manifest` },
      ];
    return targets.map((t) => {
      const values: Record<string, string> = {
        "${target.ip}": t.PublicIp ?? "",
        "${target.name}": t.Name,
        "${target.password}":
          t.PwdFile && fs.existsSync(t.PwdFile) ? (readPwdFile(t.PwdFile).password ?? "") : "",
      };
      const fill = (s: string) =>
        Object.entries(values).reduce((acc, [k, v]) => acc.split(k).join(v), s);
      const args = Object.fromEntries(
        Object.entries(c.args ?? {}).map(([k, v]) => [k, Array.isArray(v) ? v.map(fill) : fill(v)]),
      );
      return base(c, `${plain} -> ${t.Name}`, args);
    });
  });
}

/**
 * Passes when the state is the expected one ("any" accepts every state but
 * UNKNOWN, which is what a missing module, a bad argument or an unreachable
 * workload returns) and every name in "perf" occurs in some perfdata key,
 * case-insensitively. Returns why the result fails, or undefined.
 */
function judge(check: RunCheck, result: number, perfKeys: string[]): string | undefined {
  const state = STATES[result];
  if (!state) return `no result state (got ${result})`;
  if (check.expect === "any" && state === "UNKNOWN")
    return "returned UNKNOWN (expected any state but UNKNOWN)";
  if (check.expect !== "any" && state !== check.expect)
    return `returned ${state}, expected ${check.expect}`;
  const missing = check.perf.filter(
    (want) => !perfKeys.some((k) => k.toLowerCase().includes(want.toLowerCase())),
  );
  if (missing.length > 0)
    return `no perfdata matching ${missing.join(", ")} (got: ${perfKeys.join(", ") || "none"})`;
  return undefined;
}

/** Perfdata keys of a Nagios line: what follows the first '|', quoted or bare. */
function nagiosPerfKeys(line: string): string[] {
  const bar = line.indexOf("|");
  if (bar < 0) return [];
  return [...line.slice(bar + 1).matchAll(/'([^']+)'=|(?:^|\s)([^\s'=]+)=/g)].map(
    (m) => m[1] ?? m[2],
  );
}

/**
 * The check_nsclient plugin (NSCP_ESTATE_CHECK_NSCLIENT): the binary
 * operators wire into Nagios, logged in to one profile per machine.
 */
function checkNsclient(
  args: string[],
  env: NodeJS.ProcessEnv = {},
): { status: number; out: string } {
  const r = spawnSync(checkNsclientExe!, ["nsclient", ...args], {
    encoding: "utf8",
    timeout: 120_000,
    env: { ...process.env, ...env },
  });
  const out = `${r.stdout ?? ""}${r.stderr ?? ""}`.trim();
  if (r.error) return { status: -1, out: `${r.error.message} ${out}`.trim() };
  return { status: r.status ?? -1, out };
}

/** The machine's pwd file, else the manifest's public IP on 8443. */
function credentials(m: ManifestEntry): { url?: string; password?: string } {
  const fallback = m.PublicIp ? `https://${m.PublicIp}:8443` : undefined;
  if (!m.PwdFile || !fs.existsSync(m.PwdFile)) return { url: fallback };
  const { url, password } = readPwdFile(m.PwdFile);
  return { url: url ?? fallback, password };
}

function firstLine(e: unknown): string {
  return String(e instanceof Error ? e.message : e).split("\n")[0];
}

const describeEstate = manifests.length > 0 ? describe : describe.skip;

describeEstate("estate role checks", () => {
  const list = machines();

  it("the manifests list at least one machine that is up", () => {
    expect(list.length).toBeGreaterThan(0);
  });

  for (const { entry: m, peers } of list) {
    const role = m.Role || "base";
    const family = m.Family || m.Os || "linux";
    const checks = runChecks(
      roleChecks(role).filter((c) => applies(c, family)),
      peers,
    );
    const agentChecks = smokeCasesFor(family === "windows" ? "windows" : "linux");
    const nsclientChecks = checkNsclientExe ? checks : [];
    const total = checks.length + agentChecks.length + nsclientChecks.length;

    describe(`${m.Name} (${role}, ${family})`, () => {
      const { url, password } = credentials(m);
      let target: LiveTarget | undefined;
      let loginError: string | undefined;
      const records: CheckRecord[] = [];

      async function login(): Promise<LiveTarget> {
        if (!url || !password) throw new Error(`no web address or password in ${m.PwdFile}`);
        return liveLogin({ base: url, user: "admin", password, insecure: true });
      }

      beforeAll(async () => {
        try {
          target = await login();
        } catch (e) {
          loginError = `REST login to ${url ?? "?"} failed: ${firstLine(e)}`;
        }
      });

      afterAll(() => {
        if (!resultsDir) return;
        const failed = records.filter((r) => !r.passed);
        const missing = total - records.length;
        let detail: string | null = null;
        if (loginError) detail = loginError;
        else if (failed.length > 0)
          detail = `${failed.length} of ${total} check(s) failing; first: ${failed[0].check}: ${failed[0].reason}`;
        else if (missing > 0) detail = `${missing} of ${total} check(s) did not run`;
        const dir = path.join(resultsDir, m.Name);
        fs.mkdirSync(dir, { recursive: true });
        fs.writeFileSync(
          path.join(dir, "checks.json"),
          JSON.stringify(
            {
              machine: m.Name,
              role,
              family,
              url: url ?? null,
              passed: detail === null,
              detail,
              checks: records,
            },
            null,
            2,
          ),
        );
      });

      for (const check of checks) {
        it(check.label, async () => {
          if (loginError) throw new Error(loginError);
          const deadline = Date.now() + waitMs;
          let q: QueryResult | null = null;
          let why: string | undefined = check.unrunnable;
          while (!check.unrunnable) {
            try {
              if (!target) target = await login();
              q = await liveExecute(target, check.command, check.args);
              why = judge(check, q.result, Object.keys(perfOf(q)));
            } catch (e) {
              q = null;
              why = `request failed: ${firstLine(e)}`;
              // A bundle reload restarts the web server, which forgets its
              // sessions: log in again on the next attempt.
              if (/40[13]/.test(why)) target = undefined;
            }
            if (!why || Date.now() >= deadline) break;
            await new Promise((r) => setTimeout(r, 5_000));
          }
          const keys = q ? Object.keys(perfOf(q)) : [];
          records.push({
            check: check.label,
            command: check.command,
            args: check.shownArgs,
            expect: check.expect,
            perf: check.perf,
            passed: !why,
            state: q ? (STATES[q.result] ?? null) : null,
            reason: why ?? null,
            message: q ? messageOf(q) : "",
            perfKeys: keys,
            response: q,
          });
          // The reason and the check's own output, so a failure reads on its own.
          expect(why ? `${why} :: ${q ? messageOf(q) : ""}` : undefined).toBeUndefined();
        });
      }

      // The agent checks every live target must pass, whatever its role
      // (src/live-smoke.ts; the same cases as live/live-checks.test.ts).
      for (const c of agentChecks) {
        it(`agent: ${c.name}`, async () => {
          if (loginError) throw new Error(loginError);
          let q: QueryResult | undefined;
          let why: string | undefined;
          try {
            if (!target) target = await login();
            q = await c.run(target);
          } catch (e) {
            why = firstLine(e);
          }
          records.push({
            check: `agent: ${c.name}`,
            command: c.command,
            args: c.args ?? {},
            expect: "agent",
            perf: [],
            passed: !why,
            state: q ? (STATES[q.result] ?? null) : null,
            reason: why ?? null,
            message: q ? messageOf(q) : "",
            perfKeys: q ? Object.keys(perfOf(q)) : [],
            response: q ?? null,
          });
          expect(why).toBeUndefined();
        });
      }

      // The same role checks through the check_nsclient plugin, the way a
      // Nagios server runs them (-CheckNsclient on the gate). One profile per
      // machine, logged in once and out again; its exit code is the state.
      if (nsclientChecks.length > 0) {
        const profile = `estate-${m.Name}`;
        let pluginLoginError: string | undefined;
        describe("check_nsclient", () => {
          beforeAll(() => {
            if (!url || !password) {
              pluginLoginError = `no web address or password in ${m.PwdFile}`;
              return;
            }
            const r = checkNsclient(
              ["auth", "login", profile, "--url", url, "--username", "admin", "--insecure"],
              {
                CHECK_NSCLIENT_PASSWORD: password,
              },
            );
            if (r.status !== 0)
              pluginLoginError = `check_nsclient auth login failed (exit ${r.status}): ${r.out}`;
          });
          afterAll(() => {
            checkNsclient(["auth", "logout", profile]);
          });
          for (const check of nsclientChecks) {
            it(check.label, () => {
              let why: string | undefined;
              let state: string | null = null;
              let out = "";
              let keys: string[] = [];
              if (pluginLoginError) why = pluginLoginError;
              else if (check.unrunnable) why = check.unrunnable;
              else {
                const r = checkNsclient([
                  "--profile",
                  profile,
                  "queries",
                  "execute-nagios",
                  check.command,
                  ...argTokens(check.args),
                ]);
                out = r.out;
                keys = nagiosPerfKeys(out.split("\n")[0]);
                // A tool error exits 1 too, which reads as WARNING: tell them apart.
                if (r.status < 0 || r.status > 3 || /^Error:/m.test(out))
                  why = `check_nsclient failed (exit ${r.status}): ${out}`;
                else {
                  state = STATES[r.status];
                  why = judge(check, r.status, keys);
                }
              }
              records.push({
                check: `check_nsclient: ${check.label}`,
                command: check.command,
                args: check.shownArgs,
                expect: check.expect,
                perf: check.perf,
                passed: !why,
                state,
                reason: why ?? null,
                message: out.split("|")[0].trim(),
                perfKeys: keys,
                response: null,
              });
              expect(why ? `${why} :: ${out}` : undefined).toBeUndefined();
            });
          }
        });
      }
    });
  }
});
