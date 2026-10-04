/**
 * Scenario 1: a REST flood.
 *
 * N parallel workers hammer the agent's REST API with a mix of a trivial
 * check (`check_ok`, the dispatch path and nothing else), a collector-backed
 * check (`check_cpu`, which reads the 1 Hz CPU ring under a lock the collector
 * also takes) and the metrics snapshot (`/api/v2/metrics`, the whole bundle
 * tree serialised). It is the cheapest scenario and runs on every platform: it
 * exercises the WEBServer's threads, the dispatch path and the metrics
 * collection without a second process.
 *
 * Pass/fail is on the invariants in stress/lib/invariants.ts: every request
 * answered, the agent prompt once the load stops, no growth from warm-up to
 * end, a clean log. Throughput and latency go to stress/results/rest-flood.json.
 */
import { NscpInstance, OK, messageOf, pollQuery, setupQueryNscp } from "@fixtures/index";

import { expectCleanLog, expectNoGrowth, expectResponsive } from "@stress/invariants";
import { readKnobs, scenarioTimeoutMs, warmupMs } from "@stress/knobs";
import { runLoad } from "@stress/load";
import { startSampler } from "@stress/process-stats";
import { writeReport } from "@stress/report";
import { RestClient } from "@stress/rest-client";

const knobs = readKnobs();
jest.setTimeout(scenarioTimeoutMs(knobs));

/** The thresholds that keep check_cpu OK on any machine, so a failure is a real one. */
const CPU_ARGS = { warning: "usage > 101", critical: "usage > 101" };

describe("stress: REST flood", () => {
  let nscp: NscpInstance;
  let client: RestClient;

  beforeAll(async () => {
    nscp = new NscpInstance();
    // The extra tree replaces the fixture's /modules entry whole, so name all three.
    const key = await setupQueryNscp(nscp, "CheckSystem", {
      "/modules": { CheckSystem: "enabled", WEBServer: "enabled", CheckHelpers: "enabled" },
    });
    // check_cpu answers UNKNOWN until the collector has pushed its first sample.
    const warm = await pollQuery(key, "check_cpu", CPU_ARGS, (q) => q.result === OK);
    if (warm.result !== OK) {
      throw new Error(`CPU collector produced no sample before the run: ${messageOf(warm)}`);
    }
    client = new RestClient(knobs.concurrency);
    await client.login();
    // /api/v2/metrics is an empty body until the core has taken its first
    // snapshot, which it does every `metrics interval` (10 s by default) - so
    // a flood that starts straight after boot would count expected empties as
    // failures. Wait for the first snapshot the way the CPU ring was waited for.
    const metricsDeadline = Date.now() + 30_000;
    for (;;) {
      if ((await client.get("/api/v2/metrics")) !== "") break;
      if (Date.now() >= metricsDeadline) throw new Error("no metrics snapshot within 30 s of boot");
      await new Promise((r) => setTimeout(r, 500));
    }
  });

  afterAll(async () => {
    await client?.close();
    await nscp?.stop();
  });

  it("answers every request, stays prompt and does not grow", async () => {
    const pid = nscp.pid;
    if (pid === undefined) throw new Error("the agent is not running");
    const sampler = startSampler(pid);
    const mix = { check_ok: 0, check_cpu: 0, metrics: 0 };

    // The mix, spread deterministically over each worker's own counter:
    // half check_ok, three in ten check_cpu, two in ten the metrics tree.
    const task = async (_worker: number, n: number): Promise<void> => {
      const slot = n % 10;
      if (slot < 5) {
        mix.check_ok++;
        await client.query("check_ok");
      } else if (slot < 8) {
        mix.check_cpu++;
        await client.query("check_cpu", CPU_ARGS);
      } else {
        mix.metrics++;
        const metrics = await client.getJson<Record<string, unknown>>("/api/v2/metrics");
        if (Object.keys(metrics).length === 0)
          throw new Error("/api/v2/metrics answered an empty tree");
      }
    };

    // Warm up, then take the RSS baseline the leak canary measures from.
    const warmup = await runLoad({
      durationMs: warmupMs(knobs),
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });
    await sampler.markBaseline();
    const counted = { ...mix };

    const load = await runLoad({
      durationMs: knobs.durationSeconds * 1000,
      concurrency: knobs.concurrency,
      rps: knobs.rps,
      task,
    });

    await sampler.markEnd();

    // Post-load: the agent answers a fresh check promptly.
    const responsiveAfterMs = await expectResponsive(() => client.query("check_ok"));
    const samples = await sampler.stop();

    const growth = expectNoGrowth(sampler, knobs.rssTolerancePct);
    writeReport({
      scenario: "rest-flood",
      startedAt: new Date(Date.now() - samples[samples.length - 1].t * 1000).toISOString(),
      platform: process.platform,
      knobs,
      load,
      responsiveAfterMs,
      growth,
      samples,
      extra: {
        warmup,
        mix: {
          check_ok: mix.check_ok - counted.check_ok,
          check_cpu: mix.check_cpu - counted.check_cpu,
          metrics: mix.metrics - counted.metrics,
        },
      },
    });

    // Zero failed requests, in the warm-up as well as the run.
    expect(warmup.errors).toEqual({ count: 0, samples: [] });
    expect(load.errors).toEqual({ count: 0, samples: [] });
    expect(load.requests).toBeGreaterThan(0);
    expectCleanLog(nscp);
  });
});
