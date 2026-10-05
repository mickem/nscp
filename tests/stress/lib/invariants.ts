import type { NscpInstance } from "@fixtures/nscp";

import type { ProcessSample, Sampler } from "./process-stats";

/**
 * What every scenario asserts, whatever its load: the agent is still there,
 * still answers promptly, has not grown, and its log holds none of the lines
 * that mean a worker died or a submission was lost. Throughput is recorded,
 * never asserted - a baseline with regression bars comes once a few runs exist.
 */

/**
 * The agent answers `check_ok` within `withinMs` after the load has stopped.
 * `probe` runs one check_ok and resolves when the answer is OK.
 */
export async function expectResponsive(
  probe: () => Promise<unknown>,
  withinMs = 2_000,
): Promise<number> {
  const t0 = Date.now();
  await probe();
  const tookMs = Date.now() - t0;
  expect({ tookMs, withinMs, responsive: tookMs <= withinMs }).toMatchObject({ responsive: true });
  return tookMs;
}

export interface GrowthReport {
  baseline: ProcessSample;
  final: ProcessSample;
  rssGrowthPct: number;
  threadGrowth: number;
}

/**
 * The leak canary: RSS at the end of the load within `tolerancePct` of RSS
 * after warm-up, and the thread count flat (a pool that keeps spawning shows up
 * here before it shows up as a crash). Compares the sample `markBaseline()`
 * took with the one `markEnd()` took. The report is returned for the results
 * file.
 */
export function expectNoGrowth(
  sampler: Pick<Sampler, "samples" | "baselineIndex" | "endIndex">,
  tolerancePct: number,
): GrowthReport {
  const { samples, baselineIndex, endIndex } = sampler;
  expect(baselineIndex).toBeGreaterThanOrEqual(0);
  expect(endIndex).toBeGreaterThan(baselineIndex);
  const baseline = samples[baselineIndex];
  const final = samples[endIndex];
  const rssGrowthPct = ((final.rssBytes - baseline.rssBytes) / baseline.rssBytes) * 100;
  const threadGrowth = final.threads - baseline.threads;
  const report = { baseline, final, rssGrowthPct, threadGrowth };
  // One object per assertion, so a failure prints the numbers behind it.
  expect({ ...report, withinTolerance: rssGrowthPct <= tolerancePct }).toMatchObject({
    withinTolerance: true,
  });
  // "Flat" allows a thread or two: a worker that was mid-exit at the baseline
  // sample, or a lazily started helper. A pool that leaks threads shows a count
  // that climbs with the load, not by two.
  expect({ ...report, flat: threadGrowth <= 2 }).toMatchObject({ flat: true });
  return report;
}

/**
 * Lines in the agent's output that a stress run must never produce: a guarded
 * thread reporting it died (`terminated by an uncaught exception`, the wording
 * from threads::detail::render_thread_event) and any `Failed to ...` error,
 * which is how a lost submission, a bind failure or a refused reload shows up.
 */
export function expectCleanLog(nscp: NscpInstance): void {
  const lines = nscp.capturedStdout().split(/\r?\n/);
  const died = lines.filter((l) => /terminated by an uncaught exception/.test(l));
  const failed = lines.filter((l) => /Failed to /.test(l));
  expect({ died, count: died.length }).toMatchObject({ count: 0 });
  expect({ failed: failed.slice(0, 20), count: failed.length }).toMatchObject({ count: 0 });
}
