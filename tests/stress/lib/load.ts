/**
 * The load generator: N workers calling `task` in a loop until the deadline,
 * optionally paced to a request rate, with every call timed and every failure
 * counted. It knows nothing about HTTP; the scenario decides what a request
 * is and throws when its answer is wrong.
 *
 * Every duration and deadline in the harness reads performance.now(), never
 * Date.now(): the wall clock is not monotonic, and under WSL2 it steps back
 * about 2.5 s every 30 s as the host corrects it - enough to make an 8 s check
 * look like 5.5 s and a 2 ms probe look like 5 s.
 */

export interface LoadOptions {
  durationMs: number;
  concurrency: number;
  /** Requests per second across all workers; 0 = as fast as the agent answers. */
  rps: number;
  /**
   * One request. `worker` is the worker's index, `n` the worker's own call
   * counter, so a task can spread a mix deterministically. Throw to record a
   * failure; the message is kept (first few verbatim) for the report.
   */
  task: (worker: number, n: number) => Promise<void>;
  /** How many distinct failure messages to keep verbatim; default 10. */
  keepErrors?: number;
}

export interface Percentiles {
  p50: number;
  p95: number;
  p99: number;
  max: number;
}

export interface LoadStats {
  requests: number;
  perSecond: number;
  elapsedSeconds: number;
  latencyMs: Percentiles;
  errors: { count: number; samples: string[] };
}

export function percentiles(sortedMs: number[]): Percentiles {
  if (sortedMs.length === 0) return { p50: 0, p95: 0, p99: 0, max: 0 };
  const at = (q: number) =>
    sortedMs[Math.min(sortedMs.length - 1, Math.floor(q * sortedMs.length))];
  return { p50: at(0.5), p95: at(0.95), p99: at(0.99), max: sortedMs[sortedMs.length - 1] };
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

/** Hands out start times `1000 / rps` ms apart, shared by every worker. */
class Pacer {
  private next = performance.now();
  constructor(private readonly intervalMs: number) {}
  async wait(): Promise<void> {
    const now = performance.now();
    const slot = Math.max(now, this.next);
    this.next = slot + this.intervalMs;
    if (slot > now) await sleep(slot - now);
  }
}

export async function runLoad(opts: LoadOptions): Promise<LoadStats> {
  const latencies: number[] = [];
  const errors: string[] = [];
  let errorCount = 0;
  const keep = opts.keepErrors ?? 10;
  const pacer = opts.rps > 0 ? new Pacer(1000 / opts.rps) : undefined;
  const started = performance.now();
  const deadline = started + opts.durationMs;

  const worker = async (id: number): Promise<void> => {
    for (let n = 0; performance.now() < deadline; n++) {
      if (pacer) {
        await pacer.wait();
        if (performance.now() >= deadline) return;
      }
      const t0 = process.hrtime.bigint();
      try {
        await opts.task(id, n);
      } catch (e) {
        errorCount++;
        const msg = e instanceof Error ? e.message : String(e);
        if (errors.length < keep && !errors.includes(msg)) errors.push(msg);
      }
      latencies.push(Number(process.hrtime.bigint() - t0) / 1e6);
    }
  };

  await Promise.all(Array.from({ length: opts.concurrency }, (_, i) => worker(i)));
  const elapsedSeconds = (performance.now() - started) / 1000;
  latencies.sort((a, b) => a - b);
  return {
    requests: latencies.length,
    perSecond: latencies.length / elapsedSeconds,
    elapsedSeconds,
    latencyMs: percentiles(latencies),
    errors: { count: errorCount, samples: errors },
  };
}
