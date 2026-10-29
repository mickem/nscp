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
  const t0 = performance.now();
  await probe();
  const tookMs = performance.now() - t0;
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
  // One object per assertion, so a failure prints the numbers behind it. A
  // toEqual against a copy with only the verdict flipped, not toMatchObject:
  // the latter trims the diff down to the expected keys, and a failed run then
  // says `withinTolerance: false` and nothing else.
  const rss = {
    baselineMiB: Math.round(baseline.rssBytes / 2 ** 20),
    finalMiB: Math.round(final.rssBytes / 2 ** 20),
    rssGrowthPct: Math.round(rssGrowthPct * 10) / 10,
    tolerancePct,
    withinTolerance: rssGrowthPct <= tolerancePct,
  };
  expect(rss).toEqual({ ...rss, withinTolerance: true });
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
 * `ignore` drops lines a scenario knows are its own doing (a port probe that
 * connects and hangs up) before either check.
 */
export function expectCleanLog(nscp: NscpInstance, ignore: RegExp[] = []): void {
  const lines = nscp
    .capturedStdout()
    .split(/\r?\n/)
    .filter((l) => !ignore.some((re) => re.test(l)));
  const died = lines.filter((l) => /terminated by an uncaught exception/.test(l));
  const failed = lines.filter((l) => /Failed to /.test(l));
  expect({ died, count: died.length }).toEqual({ died: [], count: 0 });
  expect({ failed: failed.slice(0, 20), count: failed.length }).toEqual({ failed: [], count: 0 });
}
