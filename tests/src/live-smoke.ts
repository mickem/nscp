/**
 * The agent checks every live target must pass, whatever it is for.
 *
 * Run by live/live-checks.test.ts against one target (NSCP_TARGET_*), and by
 * live/estate.test.ts against every machine of the estate, next to its role's
 * checks. Assertions are deliberately "shape + healthy", not "forced
 * WARNING/CRITICAL": these run against an unmanipulated real machine, so they
 * verify that the REST API answers and the standard checks return a valid
 * status with the performance data they should expose. Threshold behaviour is
 * covered by the self-spawning suites.
 *
 * A case throws when it fails, with the check's own output in the message, so
 * a caller can record the reason as well as fail the test.
 */
import { liveExecute, livePoll, liveVersion, type LiveTarget, type TargetOs } from "./live-target";
import { OK, WARNING, CRITICAL, messageOf, perfOf, type QueryResult } from "./queries";

export interface SmokeCase {
  name: string;
  /** The query it runs; "info" for the version endpoint. */
  command: string;
  args?: Record<string, string>;
  /** Only on this platform; every platform when unset. */
  os?: TargetOs;
  /** Resolves with the last result, throws on failure. */
  run(t: LiveTarget): Promise<QueryResult | undefined>;
}

/** A status that is a real Nagios code (not the 3/UNKNOWN "couldn't run"). */
export function ranOk(q: QueryResult): boolean {
  return [OK, WARNING, CRITICAL].includes(q.result);
}

function hasPerf(q: QueryResult): boolean {
  return Object.keys(perfOf(q)).length > 0;
}

function must(ok: boolean, why: string, q: QueryResult): QueryResult {
  if (!ok) throw new Error(`${why} (result=${q.result} :: ${messageOf(q)})`);
  return q;
}

/** A check that must run (any state but UNKNOWN), retried past collector warm-up. */
function ran(
  name: string,
  command: string,
  opts: { args?: Record<string, string>; os?: TargetOs; perf?: boolean; poll?: boolean } = {},
): SmokeCase {
  const args = opts.args ?? {};
  const good = (q: QueryResult) => ranOk(q) && (!opts.perf || hasPerf(q));
  return {
    name,
    command,
    args,
    os: opts.os,
    async run(t) {
      const q = opts.poll
        ? await livePoll(t, command, args, good)
        : await liveExecute(t, command, args);
      return must(good(q), opts.perf ? "did not run with perfdata" : "did not run", q);
    },
  };
}

export const SMOKE_CASES: SmokeCase[] = [
  {
    name: "reports a version",
    command: "info",
    async run(t) {
      const version = await liveVersion(t);
      if (!/\d+\.\d+/.test(version))
        throw new Error(`no version in /api/v1/info (got '${version}')`);
      return undefined;
    },
  },
  {
    name: "runs the trivial check_ok probe",
    command: "check_ok",
    async run(t) {
      const q = await liveExecute(t, "check_ok");
      return must(q.result === OK, "check_ok is not OK", q);
    },
  },
  ran("check_uptime returns a status with uptime perfdata", "check_uptime", {
    perf: true,
    poll: true,
  }),
  // CPU and memory read from a 1 Hz collector; poll past warm-up.
  ran("check_cpu returns a status with load perfdata", "check_cpu", { perf: true, poll: true }),
  ran("check_memory returns a status with memory perfdata", "check_memory", {
    perf: true,
    poll: true,
  }),
  ran("check_drivesize evaluates at least one drive", "check_drivesize", {
    perf: true,
    poll: true,
  }),
  ran("check_load runs on an idle host", "check_load", { os: "linux" }),
  ran("check_swap_io reports swap paging rates", "check_swap_io", { os: "linux", poll: true }),
  ran("check_service (systemd) evaluates units", "check_service", { os: "linux" }),
  {
    name: "check_mount verifies the root filesystem is mounted",
    command: "check_mount",
    args: { mount: "/" },
    os: "linux",
    async run(t) {
      const q = await liveExecute(t, "check_mount", { mount: "/" });
      return must(q.result === OK, "/ is not reported mounted", q);
    },
  },
  ran("check_service evaluates Windows services", "check_service", { os: "windows" }),
  ran("check_eventlog runs against the System log", "check_eventlog", {
    os: "windows",
    args: { log: "System" },
  }),
];

/** The cases that apply to a target of this OS (only the cross-platform ones when it is unknown). */
export function smokeCasesFor(os: TargetOs | undefined): SmokeCase[] {
  return SMOKE_CASES.filter((c) => !c.os || c.os === os);
}
