/**
 * Scenario: every engine under load at once, with long checks running behind.
 *
 * N workers flood the agent over NRPE (whose socket server dispatches on a
 * worker pool, so requests really run in parallel) with every fast path in
 * lib/engines.ts: a C++ module, Python, Lua, Lua -> core -> Python, Python ->
 * core -> Lua, Lua -> core -> C++, and an external process. Each request
 * carries a token of its own and must come back with exactly the message that
 * path builds from it, so an answer crossed with a concurrent request's, a
 * dropped one or a script engine that got confused under contention all count
 * as failures - a status code alone would not show them.
 *
 * Behind the flood, a slow lane keeps one long check per engine running for
 * the whole window (NSCP_STRESS_SLOW_MS each, back to back). The flood must not
 * notice: no fast request may take as long as a slow check, which is what a
 * request queued behind one would take.
 *
 * The usual invariants apply: nothing failed, the agent prompt afterwards, no
 * RSS or thread growth from warm-up to end, a clean log. Throughput and latency
 * per path go to stress/results/mixed-flood.json.
 */
import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import {
  ENGINE_LOG_NOISE,
  type EngineAgent,
  FAST_CHECKS,
  SLOW_CHECKS,
  startEngineAgent,
} from "@stress/engines";
import { readKnobs, scenarioTimeoutMs, warmupMs } from "@stress/knobs";
import { percentiles, runLoad } from "@stress/load";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";

const knobs = readKnobs();
const SLOW_MS = Number(process.env.NSCP_STRESS_SLOW_MS ?? 8_000);
jest.setTimeout(scenarioTimeoutMs(knobs) + 2 * SLOW_MS);

describe("stress: every engine flooded, with long checks behind", () => {
  let agent: EngineAgent;

  beforeAll(async () => {
    // The flood's workers plus the slow lane, with room to spare: a pool the
    // load itself exhausts would measure the pool size, not the agent.
    agent = await startEngineAgent({
      "/settings/NRPE/server": {
        "thread pool": String(knobs.concurrency + SLOW_CHECKS.length + 4),
      },
    });
  });

  afterAll(async () => {
    agent?.nrpe.close();
    await agent?.nscp.stop();
  });

  it("answers every request correctly and never queues one behind a slow check", async () => {
    const pid = agent.nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);

    // The slow lane: each slow check back to back until told to stop. Its own
    // answers are checked as well - a long check must also come back intact.
    let slowStop = false;
    const slowResults: { path: string; ms: number; ok: boolean; detail?: string }[] = [];
    const slowLane = Promise.all(
      SLOW_CHECKS.map(async (s) => {
        for (let i = 0; !slowStop; i++) {
          const started = performance.now();
          try {
            const a = await agent.nrpe.query(s.command, [
              String(SLOW_MS),
              `lane_${s.command}_${i}`,
            ]);
            const ok = a.result === 0 && a.message === s.expect(SLOW_MS);
            slowResults.push({
              path: s.path,
              ms: performance.now() - started,
              ok,
              detail: ok ? undefined : `${a.result} ${JSON.stringify(a.message)}`,
            });
          } catch (e) {
            slowResults.push({
              path: s.path,
              ms: performance.now() - started,
              ok: false,
              detail: (e as Error).message,
            });
          }
        }
      }),
    );

    // Per path: latencies and counts, for the report and the queueing bound.
    const perPath = new Map<string, number[]>(FAST_CHECKS.map((c) => [c.path, []]));
    const task = async (worker: number, n: number): Promise<void> => {
      const c = FAST_CHECKS[(worker + n) % FAST_CHECKS.length];
      const token = `w${worker}n${n}`;
      const started = performance.now();
      const a = await agent.nrpe.query(c.command, c.args(token));
      // Minus any wait for a local port: a flood this fast can exhaust the
      // client's ephemeral range, and that time never reached the agent.
      perPath.get(c.path)?.push(performance.now() - started - a.localWaitMs);
      if (a.result !== 0 || a.message !== c.expect(token)) {
        throw new Error(
          `${c.command} answered ${a.result} ${JSON.stringify(a.message)}, expected ${JSON.stringify(c.expect(token))}`,
        );
      }
    };

    let warmup;
    let load;
    let responsiveAfterMs: number;
    try {
      warmup = await runLoad({
        durationMs: warmupMs(knobs),
        concurrency: knobs.concurrency,
        rps: knobs.rps,
        task,
      });
      await sampler.markBaseline();
      for (const v of perPath.values()) v.length = 0;
      load = await runLoad({
        durationMs: knobs.durationSeconds * 1000,
        concurrency: knobs.concurrency,
        rps: knobs.rps,
        task,
      });
      await sampler.markEnd();
    } finally {
      slowStop = true;
      // Let the lane's in-flight checks finish; each is at most one slow check away.
      await slowLane;
    }
    try {
      responsiveAfterMs = await expectResponsive(() =>
        agent.nrpe.expect("check_ok", ["message=after"], "after"),
      );
    } catch (e) {
      await sampler.stop().catch(() => undefined);
      throw e;
    }
    const samples = await sampler.stop();

    const latencyByPath: Record<string, ReturnType<typeof percentiles>> = {};
    for (const [p, v] of perPath) latencyByPath[p] = percentiles([...v].sort((a, b) => a - b));
    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    writeReport({
      scenario: "mixed-flood",
      startedAt: new Date(Date.now() - samples[samples.length - 1].t * 1000).toISOString(),
      platform: process.platform,
      knobs,
      load,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        // Requests that waited for a local port (client-side; see nrpe-client.ts).
        nrpeLocalRetries: agent.nrpe.localRetries,
        warmup,
        slowMs: SLOW_MS,
        latencyByPath,
        requestsByPath: Object.fromEntries([...perPath].map(([p, v]) => [p, v.length])),
        slowChecks: slowResults.length,
        slowLatencyMs: percentiles(slowResults.map((s) => s.ms).sort((a, b) => a - b)),
      },
    });

    // Zero failed or crossed answers, in the warm-up as well as the run.
    expect(warmup.errors).toEqual({ count: 0, samples: [] });
    expect(load.errors).toEqual({ count: 0, samples: [] });
    expect(load.requests).toBeGreaterThan(0);
    // No fast request waited as long as a slow check takes: that is what one
    // queued behind a running slow check would have cost.
    const queued = Object.entries(latencyByPath)
      .filter(([, l]) => l.max >= SLOW_MS)
      .map(([p, l]) => ({ path: p, maxMs: l.max }));
    expect({ queued, count: queued.length }).toEqual({ queued: [], count: 0 });
    // The slow lane ran throughout and every long check came back intact.
    expect(slowResults.length).toBeGreaterThanOrEqual(SLOW_CHECKS.length);
    const badSlow = slowResults.filter((s) => !s.ok);
    expect({ bad: badSlow.slice(0, 10), count: badSlow.length }).toEqual({ bad: [], count: 0 });
    expectCleanLog(agent.nscp, ENGINE_LOG_NOISE);
  });
});
