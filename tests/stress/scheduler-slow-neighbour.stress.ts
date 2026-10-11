/**
 * Scenario: the scheduler keeps time while some of its checks are slow.
 *
 * A real agent schedules a mix of quick checks and a few slow ones (a script
 * waiting on a remote service, an external command). The slow ones each hold
 * a pool thread while they run - that is expected - but they must not hold up
 * anything else: not the other workers, not the engines the quick checks run
 * in, not the submission path.
 *
 * One slow schedule per engine (lib/engines.ts: an external script, Python,
 * Lua and the two cross-engine paths, each taking NSCP_STRESS_SLOW_MS, default
 * 8 s) runs next to NSCP_STRESS_SCHEDULES (default 200) quick ones spread over
 * every fast path, all at a 5 s interval on the default 10-thread pool, so the
 * slow schedules hold half the pool at any moment. Every result goes into the
 * WEBServer's passive result cache, drained over REST once a second.
 *
 * Asserted: every quick schedule delivered its beats on time (at least all but
 * one of the whole intervals in the window - a schedule queued behind a slow
 * check would lose one per slow check), every one of them answered OK with the
 * message its own path builds from its alias (so the engines kept their calls
 * apart), every slow schedule delivered too, the scheduler never logged a late
 * start, plus the usual: prompt afterwards, no growth, a clean log.
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
import { percentiles } from "@stress/load";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";
import type { RestClient } from "@stress/rest-client";

const knobs = readKnobs();
const SLOW_MS = Number(process.env.NSCP_STRESS_SLOW_MS ?? 8_000);
const SCHEDULES = Number(process.env.NSCP_STRESS_SCHEDULES ?? 200);
const INTERVAL_S = 5;
const POOL_THREADS = 10;
const ALIAS = "stress_sched";
const BASE = `/settings/${ALIAS}`;
const CHANNEL = "STRESS_RESULTS";
const POLL_MS = 1_000;
jest.setTimeout(scenarioTimeoutMs(knobs) + 2 * SLOW_MS);

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

interface CachedResult {
  alias: string;
  status: number;
  message: string;
  count: number;
}

/** The quick schedules: `fast_00001` .. , round-robin over the fast paths, each passing its alias as the token. */
const fast = Array.from({ length: SCHEDULES }, (_, i) => {
  const alias = `fast_${String(i + 1).padStart(5, "0")}`;
  const check = FAST_CHECKS[i % FAST_CHECKS.length];
  return { alias, check, commandLine: [check.command, ...check.args(alias)].join(" ") };
});
const slow = SLOW_CHECKS.map((s) => ({
  alias: `slow_${s.command}`,
  check: s,
  commandLine: `${s.command} ${SLOW_MS} sched_${s.command}`,
}));

describe("stress: the scheduler keeps time next to slow checks", () => {
  let agent: EngineAgent;
  let client: RestClient;

  beforeAll(async () => {
    agent = await startEngineAgent({
      "/modules": { [ALIAS]: "Scheduler" },
      "/settings/WEB/server/results": {
        enabled: "true",
        channel: CHANNEL,
        "primary index": "${alias-or-command}",
      },
      [BASE]: { threads: String(POOL_THREADS) },
      [`${BASE}/schedules/default`]: {
        channel: CHANNEL,
        interval: `${INTERVAL_S}s`,
        randomness: "0%",
        report: "all",
      },
      [`${BASE}/schedules`]: Object.fromEntries(
        [...fast, ...slow].map((s) => [s.alias, s.commandLine]),
      ),
    });
    client = await agent.rest(2);
  });

  afterAll(async () => {
    await client?.close();
    await agent?.nscp.stop();
  });

  it("delivers every quick schedule on time while the slow ones run", async () => {
    const pid = agent.nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);

    const perAlias = new Map<string, number>();
    const wrong: { alias: string; status: number; message: string }[] = [];
    const pollLatencies: number[] = [];
    const expectMessage = new Map<string, string>([
      ...fast.map((s) => [s.alias, s.check.expect(s.alias)] as [string, string]),
      ...slow.map((s) => [s.alias, s.check.expect(SLOW_MS)] as [string, string]),
    ]);

    /** Drain the cache once; `count` is how often that alias reported since the last drain. */
    const drain = async (counting: boolean): Promise<void> => {
      const t0 = Date.now();
      const results = await client.getJson<CachedResult[]>("/api/v2/results");
      pollLatencies.push(Date.now() - t0);
      for (const r of results) {
        if (r.status !== 0 || r.message !== expectMessage.get(r.alias)) {
          wrong.push({ alias: r.alias, status: r.status, message: r.message });
        }
        if (counting) perAlias.set(r.alias, (perAlias.get(r.alias) ?? 0) + r.count);
      }
    };

    let started = 0;
    let elapsedSeconds = 0;
    let responsiveAfterMs: number;
    try {
      // Warm up past the first slow round, so every slow schedule is already
      // holding its thread when counting starts.
      const warmupUntil = Date.now() + Math.max(warmupMs(knobs), SLOW_MS + 2_000);
      while (Date.now() < warmupUntil) {
        await sleep(POLL_MS);
        await drain(false);
      }
      await sampler.markBaseline();

      started = Date.now();
      const deadline = started + knobs.durationSeconds * 1000;
      while (Date.now() < deadline) {
        await sleep(POLL_MS);
        await drain(true);
      }
      elapsedSeconds = (Date.now() - started) / 1000;
      await sampler.markEnd();
      responsiveAfterMs = await expectResponsive(() =>
        agent.nrpe.expect("check_ok", ["message=after"], "after"),
      );
    } catch (e) {
      await sampler.stop().catch(() => undefined);
      throw e;
    }
    const samples = await sampler.stop();

    // Whole intervals in the window; the window is not aligned to the
    // schedules' phase, so one beat either side is neither late nor early.
    const wholeIntervals = Math.floor(elapsedSeconds / INTERVAL_S);
    const minBeats = Math.max(1, wholeIntervals - 1);
    const fastBeats = fast.map((s) => perAlias.get(s.alias) ?? 0);
    const lagging = fast
      .map((s, i) => ({ alias: s.alias, path: s.check.path, beats: fastBeats[i] }))
      .filter((s) => s.beats < minBeats);
    const late = agent.nscp
      .capturedStdout()
      .split(/\r?\n/)
      .filter((l) => /seconds to late/.test(l));

    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    writeReport({
      scenario: "scheduler-slow-neighbour",
      startedAt: new Date(started).toISOString(),
      platform: process.platform,
      knobs,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        schedules: SCHEDULES,
        slowSchedules: slow.length,
        slowMs: SLOW_MS,
        intervalSeconds: INTERVAL_S,
        poolThreads: POOL_THREADS,
        elapsedSeconds,
        minBeats,
        fastBeats: percentiles([...fastBeats].sort((a, b) => a - b)),
        slowBeats: Object.fromEntries(slow.map((s) => [s.alias, perAlias.get(s.alias) ?? 0])),
        wrongResults: wrong.length,
        lateStarts: late.length,
        pollLatencyMs: percentiles([...pollLatencies].sort((a, b) => a - b)),
      },
    });

    expect({ wrong: wrong.slice(0, 10), count: wrong.length }).toMatchObject({ count: 0 });
    expect({ lagging: lagging.slice(0, 20), count: lagging.length }).toMatchObject({ count: 0 });
    const silentSlow = slow.filter((s) => !perAlias.has(s.alias)).map((s) => s.alias);
    if (elapsedSeconds * 1000 > SLOW_MS + INTERVAL_S * 1000) {
      expect({ silentSlow, count: silentSlow.length }).toMatchObject({ count: 0 });
    }
    expect({ late: late.slice(0, 10), count: late.length }).toMatchObject({ count: 0 });
    expectCleanLog(agent.nscp, ENGINE_LOG_NOISE);
  });
});
