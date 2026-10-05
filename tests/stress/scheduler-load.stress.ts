/**
 * Scenario 3: scheduler load, the spirit of the retired scripts/python/test_stress.py.
 *
 * A thousand schedules fire every 5 s on a 50-thread pool, each result landing
 * in the WEBServer's passive result cache, which the test drains once a second
 * over REST and counts. No external client: the load is the agent's own
 * scheduler and submit path, which is exactly where the legacy test looked for
 * trouble (it routed the same beats over NSCA and NRPE loopback, which the
 * passive-flood and nrpe-flood scenarios cover separately).
 *
 * Measured and recorded: delivered beats per second against the expected rate,
 * missed beats, cache poll latency, post-load responsiveness. Asserted: every
 * schedule delivered at least once (none starved), nothing failed to submit, the
 * agent prompt once the pool is stopped, no growth, a clean log.
 */
import * as fs from "fs";
import * as path from "path";

import { NscpInstance, OK, executeQuery, setupQueryNscp } from "@fixtures/index";

import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import { readKnobs, scenarioTimeoutMs, warmupMs } from "@stress/knobs";
import { percentiles } from "@stress/load";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";
import { RestClient } from "@stress/rest-client";

const knobs = readKnobs();
jest.setTimeout(scenarioTimeoutMs(knobs));

/** How many schedules; the legacy test installed 1000 at 5 s. */
const SCHEDULES = Number(process.env.NSCP_STRESS_SCHEDULES ?? 1000);
const INTERVAL_S = 5;
const POOL_THREADS = 50;
const ALIAS = "stress_sched";
const BASE = `/settings/${ALIAS}`;
const CHANNEL = "STRESS_RESULTS";
const POLL_MS = 1_000;

/**
 * The legacy teardown as a Lua query: an empty pool applied by reloading the
 * instance. A reload cannot be asked for over REST directly, and from inside a
 * dispatched call the core defers it to its own scheduler, which is fine here.
 */
const LUA = `
local function sched_stop(command, args)
  Settings():set_string('${BASE}', 'threads', '0')
  Core():reload('${ALIAS}')
  return 'ok', 'stopping'
end
Registry():simple_function('sched_stop', sched_stop, 'stop the stress scheduler')
`;

interface CachedResult {
  key: string;
  alias: string;
  status: number;
  count: number;
}

describe("stress: scheduler load into the result cache", () => {
  let nscp: NscpInstance;
  let client: RestClient;

  beforeAll(async () => {
    nscp = new NscpInstance();
    const script = path.join(nscp.scratch("lua"), "stress.lua");
    fs.writeFileSync(script, LUA);
    await nscp.configure({
      "/modules": {
        CheckHelpers: "enabled",
        LUAScript: "enabled",
        WEBServer: "enabled",
        [ALIAS]: "Scheduler",
      },
      "/settings/lua/scripts": { stress: script },
      "/settings/WEB/server/results": {
        enabled: "true",
        channel: CHANNEL,
        // One key per schedule, so a drained poll reports each beat under the
        // schedule that produced it.
        "primary index": "${alias-or-command}",
      },
      [BASE]: { threads: String(POOL_THREADS) },
      [`${BASE}/schedules/default`]: {
        channel: CHANNEL,
        command: "check_ok",
        interval: `${INTERVAL_S}s`,
        randomness: "0%",
        report: "all",
      },
    });
    // The thousand schedules go straight into the INI: one `nscp settings`
    // call each would take longer than the run. Shorthand entries, key =
    // command, inheriting the template above.
    const lines = [`[${BASE}/schedules]`];
    for (let i = 1; i <= SCHEDULES; i++)
      lines.push(`stress_${String(i).padStart(5, "0")} = check_ok`);
    fs.appendFileSync(nscp.settingsFile, `\n${lines.join("\n")}\n`);

    // setupQueryNscp adds the REST users and starts the agent; the modules are
    // already enabled above, so its own /modules entries are a no-op.
    await setupQueryNscp(nscp, "CheckHelpers");
    client = new RestClient(2);
    await client.login();
  });

  afterAll(async () => {
    await client?.close();
    await nscp?.stop();
  });

  it("delivers every schedule, stays prompt and does not grow", async () => {
    const pid = nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);

    const perAlias = new Map<string, number>();
    let delivered = 0;
    let notOk = 0;
    const pollLatencies: number[] = [];
    const perSecondSeries: { t: number; delivered: number }[] = [];
    const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

    /** Drain the cache once; `count` per entry is how often that key reported since the last drain. */
    const drain = async (): Promise<number> => {
      const t0 = Date.now();
      const results = await client.getJson<CachedResult[]>("/api/v2/results");
      pollLatencies.push(Date.now() - t0);
      let n = 0;
      for (const r of results) {
        n += r.count;
        perAlias.set(r.alias, (perAlias.get(r.alias) ?? 0) + r.count);
        if (r.status !== OK) notOk += r.count;
      }
      return n;
    };

    // Warm up: the pool starts, the first beats land. Then baseline.
    const warmupUntil = Date.now() + warmupMs(knobs);
    while (Date.now() < warmupUntil) {
      await sleep(POLL_MS);
      await drain();
    }
    await sampler.markBaseline();
    perAlias.clear();

    const started = Date.now();
    const deadline = started + knobs.durationSeconds * 1000;
    while (Date.now() < deadline) {
      await sleep(POLL_MS);
      const n = await drain();
      delivered += n;
      perSecondSeries.push({ t: (Date.now() - started) / 1000, delivered: n });
    }
    const elapsedSeconds = (Date.now() - started) / 1000;
    await sampler.markEnd();

    // Stop the pool, then the agent must answer a fresh check promptly. The
    // reload is deferred inside the agent, so give it a moment to land.
    expect((await executeQuery(await adminKey(), "sched_stop")).result).toBe(OK);
    await sleep(2_000);
    const responsiveAfterMs = await expectResponsive(() => client.query("check_ok"));
    // Beats that were in flight when the window closed.
    const late = await drain();
    const samples = await sampler.stop();

    // Whole beats in the window: the window is not aligned to the schedules'
    // phase, so a fraction of a beat is neither delivered nor missed.
    const expected = Math.floor(elapsedSeconds / INTERVAL_S) * SCHEDULES;
    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    writeReport({
      scenario: "scheduler-load",
      startedAt: new Date(started).toISOString(),
      platform: process.platform,
      knobs,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        schedules: SCHEDULES,
        intervalSeconds: INTERVAL_S,
        poolThreads: POOL_THREADS,
        elapsedSeconds,
        delivered,
        deliveredLate: late,
        deliveredPerSecond: delivered / elapsedSeconds,
        expectedPerSecond: SCHEDULES / INTERVAL_S,
        expectedBeats: expected,
        missedBeats: Math.max(0, expected - delivered - late),
        deliveredRatio: expected > 0 ? (delivered + late) / expected : null,
        notOk,
        schedulesSeen: perAlias.size,
        pollLatencyMs: percentiles([...pollLatencies].sort((a, b) => a - b)),
        perSecond: perSecondSeries,
      },
    });

    // Every result was check_ok's OK; anything else is a failed dispatch.
    expect(notOk).toBe(0);
    expect(delivered).toBeGreaterThan(0);
    // No schedule starved: with at least two intervals in the window each one
    // reported at least once.
    if (elapsedSeconds >= 2 * INTERVAL_S) {
      const missing = Array.from(
        { length: SCHEDULES },
        (_, i) => `stress_${String(i + 1).padStart(5, "0")}`,
      ).filter((a) => !perAlias.has(a));
      expect({ missing: missing.slice(0, 20), count: missing.length }).toMatchObject({ count: 0 });
    }
    expectCleanLog(nscp);
  });

  /** The bearer key the fixture's executeQuery wants; the client holds its own. */
  async function adminKey(): Promise<string> {
    const body = await client.getJson<{ key: string }>("/api/v1/login");
    return body.key;
  }
});
