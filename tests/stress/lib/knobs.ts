/**
 * The knobs every stress scenario reads, all from the environment so a CI job
 * and a developer's shell drive the same file:
 *
 *   NSCP_STRESS_DURATION       seconds of load, default 60
 *   NSCP_STRESS_CONCURRENCY    parallel workers (open connections), default 32
 *   NSCP_STRESS_RPS            requests per second across all workers, 0 = unbounded
 *   NSCP_STRESS_RSS_TOLERANCE  percent the RSS may grow from after warm-up to
 *                              the end before the leak canary fails, default 20
 *
 * A soak is the same scenario with a long duration: NSCP_STRESS_DURATION=1800.
 */

function intKnob(name: string, def: number, min = 0): number {
  const raw = process.env[name];
  if (raw === undefined || raw === "") return def;
  const n = Number(raw);
  if (!Number.isFinite(n) || n < min) {
    throw new Error(`${name}=${raw} is not a number >= ${min}`);
  }
  return Math.floor(n);
}

export interface Knobs {
  durationSeconds: number;
  concurrency: number;
  rps: number;
  rssTolerancePct: number;
}

export function readKnobs(): Knobs {
  return {
    durationSeconds: intKnob("NSCP_STRESS_DURATION", 60, 5),
    concurrency: intKnob("NSCP_STRESS_CONCURRENCY", 32, 1),
    rps: intKnob("NSCP_STRESS_RPS", 0),
    rssTolerancePct: intKnob("NSCP_STRESS_RSS_TOLERANCE", 20),
  };
}

/**
 * How long a scenario warms the agent up before the baseline RSS sample is
 * taken: a tenth of the run, between 5 and 30 s. Thread pools, caches and the
 * allocator's arenas all grow in the first seconds of load; measuring growth
 * from a cold start would make every run look like a leak.
 */
export function warmupMs(k: Knobs): number {
  return Math.min(30_000, Math.max(5_000, (k.durationSeconds * 1000) / 10));
}

/** Per-test timeout: the run, the warm-up, and three minutes for boot and teardown. */
export function scenarioTimeoutMs(k: Knobs): number {
  return k.durationSeconds * 1000 + warmupMs(k) + 180_000;
}
