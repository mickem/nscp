/**
 * Scenario: a slow check in one engine must not hold up anything else.
 *
 * A check that takes a while is normal - a script waiting on a remote service,
 * an external command, a slow WMI query - and an agent serving a monitoring
 * server always has a few of them in flight. What must never happen is that
 * one of them stalls the core or another engine: a lock held across the call
 * into a script, an interpreter lock not released while the script sleeps, a
 * dispatch path that waits for the slowest request.
 *
 * Each round starts one slow check per engine and per cross-engine path at
 * once (lib/engines.ts: an external script, Python time.sleep, Lua
 * nscp.sleep, Lua -> core -> Python, Python -> core -> Lua), waits until every
 * one of them has marked itself running - which proves they overlap, rather
 * than inferring it from a clock - and then, while they are all still inside
 * the agent, probes every fast path (C++, Python, Lua, the cross-engine calls,
 * an external process). Each probe must answer its own token, correctly,
 * within PROBE_BUDGET_MS, a fraction of the slow checks' duration, so an
 * answer that waited for a slow check cannot pass. Then every slow check must
 * come back with its own answer.
 *
 * Rounds repeat for NSCP_STRESS_DURATION, so a soak of this scenario is a long
 * run of overlapping slow checks; the usual leak canary and clean-log check
 * apply. NSCP_STRESS_SLOW_MS (default 8000) is how long each slow check runs.
 *
 * Over NRPE, whose server runs a worker pool, this is expected to pass, and
 * over REST, which runs requests on the WEB server's `threads` pool.
 *
 * Not asserted: a Lua check that spins without calling nscp.sleep or the core
 * holds the one Lua lock and serialises every other Lua check behind it. That
 * is how the Lua runtime is built, not a regression this scenario could catch.
 */
import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import {
  ENGINE_LOG_NOISE,
  type EngineAgent,
  FAST_CHECKS,
  SLOW_CHECKS,
  startEngineAgent,
} from "@stress/engines";
import { readKnobs, scenarioTimeoutMs } from "@stress/knobs";
import { percentiles } from "@stress/load";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";
import type { RestClient } from "@stress/rest-client";

const knobs = readKnobs();
const SLOW_MS = Number(process.env.NSCP_STRESS_SLOW_MS ?? 8_000);
/** A probe that took longer than this waited for something; a quarter of a slow check. */
const PROBE_BUDGET_MS = Math.min(2_000, SLOW_MS / 4);
jest.setTimeout(scenarioTimeoutMs(knobs) + 4 * SLOW_MS);

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

interface Answer {
  result: number;
  message: string;
  /** Time spent waiting for a local port before the request went out; not the agent's. */
  localWaitMs?: number;
}

/** One way into the agent: `positional` is what NRPE sends, `options` what REST sends. */
type Call = (
  command: string,
  positional: string[],
  options: Record<string, string>,
) => Promise<Answer>;

interface Probe {
  path: string;
  ms: number;
  ok: boolean;
  detail?: string;
}

interface RoundResult {
  /** ms from the first slow check sent to every one of them marked running. */
  overlapAfterMs: number;
  probes: Probe[];
  slow: { path: string; ms: number; ok: boolean; detail?: string }[];
}

describe("stress: a slow check in one engine does not hold up the others", () => {
  let agent: EngineAgent;
  let rest: RestClient;

  beforeAll(async () => {
    agent = await startEngineAgent();
    rest = await agent.rest(SLOW_CHECKS.length + FAST_CHECKS.length + 2);
  });

  afterAll(async () => {
    await rest?.close();
    await agent?.nscp.stop();
  });

  const nrpeCall: Call = async (command, positional) => {
    const a = await agent.nrpe.query(command, positional);
    return { result: a.result, message: a.message, localWaitMs: a.localWaitMs };
  };

  const restCall: Call = async (command, _positional, options) => {
    const qs = new URLSearchParams(options).toString();
    const body = await rest.getJson<{ result: number; lines: { message: string }[] }>(
      `/api/v1/queries/${command}/commands/execute${qs ? `?${qs}` : ""}`,
    );
    return { result: body.result, message: body.lines?.[0]?.message ?? "" };
  };

  /**
   * One round: every slow check started at once, every fast path probed while
   * they all run, every slow check's own answer collected. Throws when the slow
   * checks never all run at the same time.
   */
  async function round(call: Call, tag: string): Promise<RoundResult> {
    const markers = SLOW_CHECKS.map((s) => `${tag}_${s.command}`);
    const t0 = performance.now();
    let answered = 0;
    const slow = SLOW_CHECKS.map(async (s, i) => {
      const started = performance.now();
      try {
        const a = await call(s.command, [String(SLOW_MS), markers[i]], {
          [String(SLOW_MS)]: "",
          [markers[i]]: "",
        });
        const ms = performance.now() - started;
        const ok = a.result === 0 && a.message === s.expect(SLOW_MS);
        return {
          path: s.path,
          ms,
          ok,
          detail: ok ? undefined : `${a.result} ${JSON.stringify(a.message)}`,
        };
      } catch (e) {
        return {
          path: s.path,
          ms: performance.now() - started,
          ok: false,
          detail: (e as Error).message,
        };
      } finally {
        answered++;
      }
    });

    // Every slow check inside the agent at the same moment. Half a slow check's
    // duration is ample for five requests to reach their handlers; a path that
    // serialises behind another never gets there.
    const allRunning = () => markers.every((m) => agent.running().includes(m));
    const overlapDeadline = t0 + SLOW_MS / 2;
    while (!allRunning() && performance.now() < overlapDeadline) await sleep(20);
    const overlapAfterMs = performance.now() - t0;
    if (!allRunning()) {
      const running = agent.running();
      await Promise.all(slow);
      throw new Error(
        `the slow checks never ran at the same time: after ${overlapAfterMs} ms only ` +
          `[${markers.filter((m) => running.includes(m)).join(", ")}] of [${markers.join(", ")}] were running`,
      );
    }

    // Probe every fast path, all at once, for as long as no slow check has
    // answered. A slow answer cannot arrive before its check is done, so a
    // batch that completes while all of them are unanswered ran alongside all
    // of them; one that ends after a slow check answered proves nothing and is
    // not counted. (The markers prove the overlap began; they are not re-read
    // here, a directory listing racing five writers is a poor clock.)
    const probes: Probe[] = [];
    let n = 0;
    while (answered === 0 && performance.now() < t0 + SLOW_MS - PROBE_BUDGET_MS) {
      const batch = await Promise.all(
        FAST_CHECKS.map(async (c): Promise<Probe> => {
          const token = `${tag}-${n}-${c.command}`;
          const started = performance.now();
          try {
            const a = await call(c.command, c.args(token), c.restArgs(token));
            const ms = performance.now() - started - (a.localWaitMs ?? 0);
            const right = a.result === 0 && a.message === c.expect(token);
            return {
              path: c.path,
              ms,
              ok: right && ms <= PROBE_BUDGET_MS,
              detail: right
                ? ms <= PROBE_BUDGET_MS
                  ? undefined
                  : `took ${ms} ms`
                : `${a.result} ${JSON.stringify(a.message)}`,
            };
          } catch (e) {
            return {
              path: c.path,
              ms: performance.now() - started,
              ok: false,
              detail: (e as Error).message,
            };
          }
        }),
      );
      n++;
      if (answered === 0) probes.push(...batch);
      else break;
    }
    return { overlapAfterMs, probes, slow: await Promise.all(slow) };
  }

  /** Rounds for the configured duration, then the invariants. */
  async function soak(call: Call, transport: string): Promise<void> {
    const pid = agent.nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);
    const rounds: RoundResult[] = [];
    const started = performance.now();
    let responsiveAfterMs: number;
    try {
      // The first round is the warm-up: pools, interpreters and the scripts'
      // first calls all grow the process. The baseline is taken after it.
      rounds.push(await round(call, `${transport}0`));
      await sampler.markBaseline();
      const deadline = performance.now() + knobs.durationSeconds * 1000;
      for (let i = 1; performance.now() < deadline; i++)
        rounds.push(await round(call, `${transport}${i}`));
      await sampler.markEnd();
      responsiveAfterMs = await expectResponsive(() => call("check_ok", [], {}));
    } catch (e) {
      await sampler.stop().catch(() => undefined);
      throw e;
    }
    const samples = await sampler.stop();

    const probes = rounds.flatMap((r) => r.probes);
    const slow = rounds.flatMap((r) => r.slow);
    const byPath: Record<string, ReturnType<typeof percentiles>> = {};
    for (const c of FAST_CHECKS) {
      byPath[c.path] = percentiles(
        probes
          .filter((p) => p.path === c.path)
          .map((p) => p.ms)
          .sort((a, b) => a - b),
      );
    }
    const growth = rounds.length > 2 ? expectNoGrowth(sampler, knobs.rssTolerancePct) : undefined;
    writeReport({
      scenario: `slow-check-isolation-${transport}`,
      startedAt: new Date(Date.now() - (performance.now() - started)).toISOString(),
      platform: process.platform,
      knobs,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        // Requests that waited for a local port (client-side; see nrpe-client.ts).
        nrpeLocalRetries: agent.nrpe.localRetries,
        slowMs: SLOW_MS,
        probeBudgetMs: PROBE_BUDGET_MS,
        rounds: rounds.length,
        overlapAfterMs: percentiles(rounds.map((r) => r.overlapAfterMs).sort((a, b) => a - b)),
        probes: probes.length,
        probeLatencyMs: byPath,
        slowLatencyMs: percentiles(slow.map((s) => s.ms).sort((a, b) => a - b)),
        perRound: rounds.map((r) => ({
          overlapAfterMs: r.overlapAfterMs,
          probes: r.probes.length,
          slow: r.slow.map((s) => `${s.path} ${s.ms} ms${s.ok ? "" : ` FAILED ${s.detail}`}`),
        })),
      },
    });

    // Every fast path was probed while the slow checks ran, in every round.
    const unprobed = rounds
      .map((r, i) => ({ round: i, probes: r.probes.length }))
      .filter((r) => r.probes < FAST_CHECKS.length);
    expect({ unprobed, count: unprobed.length }).toEqual({ unprobed: [], count: 0 });
    const badProbes = probes.filter((p) => !p.ok);
    expect({ bad: badProbes.slice(0, 20), count: badProbes.length }).toEqual({ bad: [], count: 0 });
    // Every slow check came back with its own answer, and none was cut short.
    const badSlow = slow.filter((s) => !s.ok || s.ms < SLOW_MS - 1_000);
    expect({ bad: badSlow.slice(0, 20), count: badSlow.length }).toEqual({ bad: [], count: 0 });
    expectCleanLog(agent.nscp, ENGINE_LOG_NOISE);
  }

  it("over NRPE: every engine answers promptly while every other one is busy", async () => {
    await soak(nrpeCall, "nrpe");
  });

  it("over REST: every engine answers promptly while every other one is busy", async () => {
    await soak(restCall, "rest");
  });
});
