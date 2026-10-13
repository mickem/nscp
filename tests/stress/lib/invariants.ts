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
  expect({ tookMs, withinMs, responsive: tookMs <= withinMs }).toEqual({
    tookMs,
    withinMs,
    responsive: true,
  });
  return tookMs;
}

export interface GrowthReport {
  baseline: ProcessSample;
  final: ProcessSample;
  /** The highest RSS over the first `PEAK_WINDOW` samples of the load. */
  baselinePeakRss: number;
  /** The highest RSS over the last `PEAK_WINDOW` samples of the load. */
  finalPeakRss: number;
  rssGrowthPct: number;
  threadGrowth: number;
}

/**
 * How many samples (one a second) each end of the load takes its RSS peak
 * over. The working set of an agent under load is a sawtooth, not a level: the
 * passive flood's drain builds a response for a few thousand results twice a
 * second and the heap swings 8 MiB with it, so two single samples measured
 * where each happened to land - 41 MiB at the baseline, 50 at the end, +22% -
 * from an agent that held 50-55 MiB at its peaks for five minutes and two
 * million submissions. A leak raises the peak; a sawtooth does not.
 */
const PEAK_WINDOW = 5;

/**
 * The leak canary: the RSS peak at the end of the load within `tolerancePct`
 * of the peak just after warm-up, and the thread count flat (a pool that keeps
 * spawning shows up here before it shows up as a crash). The peaks are taken
 * over the samples from `markBaseline()` on and up to `markEnd()`; the thread
 * counts are those two samples'. The report is returned for the results file.
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
  const peak = (from: number, to: number) =>
    Math.max(...samples.slice(from, to + 1).map((x) => x.rssBytes));
  const baselinePeakRss = peak(baselineIndex, Math.min(baselineIndex + PEAK_WINDOW - 1, endIndex));
  const finalPeakRss = peak(Math.max(endIndex - PEAK_WINDOW + 1, baselineIndex), endIndex);
  const rssGrowthPct = ((finalPeakRss - baselinePeakRss) / baselinePeakRss) * 100;
  const threadGrowth = final.threads - baseline.threads;
  const report = { baseline, final, baselinePeakRss, finalPeakRss, rssGrowthPct, threadGrowth };
  // One object per assertion, so a failure prints the numbers behind it -
  // which takes toEqual against the same numbers: toMatchObject only diffs the
  // keys it was given, and printed nothing but `withinTolerance: false`.
  expect({ ...report, tolerancePct, withinTolerance: rssGrowthPct <= tolerancePct }).toEqual({
    ...report,
    tolerancePct,
    withinTolerance: true,
  });
  // "Flat" allows a thread or two: a worker that was mid-exit at the baseline
  // sample, or a lazily started helper. A pool that leaks threads shows a count
  // that climbs with the load, not by two.
  expect({ ...report, flat: threadGrowth <= 2 }).toEqual({ ...report, flat: true });
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
