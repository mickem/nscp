import * as fs from "fs";
import execa from "execa";

import { onDarwin, onLinux, onWindows } from "@fixtures/platform";

/** One sample of the agent process. */
export interface ProcessSample {
  /** Seconds since the sampler started. */
  t: number;
  /** Resident set size in bytes. */
  rssBytes: number;
  /** Threads in the process. */
  threads: number;
}

/**
 * RSS and thread count of a process, read the way each OS exposes them: procfs
 * on Linux, `ps` on macOS, Get-Process on Windows. Throws when the process is
 * gone, which a sampler reports as the agent having died under load.
 */
export async function sampleProcess(pid: number): Promise<{ rssBytes: number; threads: number }> {
  if (onLinux) {
    const status = fs.readFileSync(`/proc/${pid}/status`, "utf8");
    const rssKb = Number(/^VmRSS:\s+(\d+)\s+kB/m.exec(status)?.[1]);
    const threads = Number(/^Threads:\s+(\d+)/m.exec(status)?.[1]);
    if (!Number.isFinite(rssKb) || !Number.isFinite(threads)) {
      throw new Error(`could not parse /proc/${pid}/status`);
    }
    return { rssBytes: rssKb * 1024, threads };
  }
  if (onDarwin) {
    const rss = await execa("ps", ["-o", "rss=", "-p", String(pid)]);
    // `ps -M` prints one line per thread under a header.
    const threads = await execa("ps", ["-M", "-p", String(pid)]);
    const rssKb = Number(rss.stdout.trim());
    const lines = threads.stdout.split("\n").filter((l) => l.trim() !== "").length - 1;
    if (!Number.isFinite(rssKb) || lines < 1) throw new Error(`ps gave no data for pid ${pid}`);
    return { rssBytes: rssKb * 1024, threads: lines };
  }
  if (onWindows) {
    const r = await execa("powershell", [
      "-NoProfile",
      "-Command",
      `$p = Get-Process -Id ${pid}; @{ rss = $p.WorkingSet64; threads = $p.Threads.Count } | ConvertTo-Json -Compress`,
    ]);
    const parsed = JSON.parse(r.stdout) as { rss: number; threads: number };
    return { rssBytes: parsed.rss, threads: parsed.threads };
  }
  throw new Error(`no process sampler for ${process.platform}`);
}

export interface Sampler {
  /** Take one sample now and keep it; returns it. */
  sample(): Promise<ProcessSample>;
  /** Stop the periodic sampling and return everything collected. */
  stop(): Promise<ProcessSample[]>;
  /** Take a sample and mark it as the warm-up baseline; returns its index. */
  markBaseline(): Promise<number>;
  /**
   * Take a sample and mark it as the end of the load; returns its index. The
   * leak canary compares this one with the baseline, so it is taken while the
   * load is still on, before anything is torn down (a stopped pool has no
   * threads to count).
   */
  markEnd(): Promise<number>;
  readonly samples: ProcessSample[];
  baselineIndex: number;
  endIndex: number;
}

/**
 * Sample `pid` every `everyMs` until stopped. A sample that fails (the process
 * is gone) is recorded as an error and surfaces in `stop()`; a scenario asserts
 * on the agent being alive separately, so the sampler never throws into a
 * timer callback.
 */
export function startSampler(pid: number, everyMs = 1_000): Sampler {
  const started = Date.now();
  const samples: ProcessSample[] = [];
  let lastError: Error | undefined;
  let inFlight: Promise<void> = Promise.resolve();

  const take = async (): Promise<ProcessSample> => {
    const s = await sampleProcess(pid);
    const sample = { t: (Date.now() - started) / 1000, ...s };
    samples.push(sample);
    return sample;
  };
  const tick = (): void => {
    inFlight = take()
      .then(() => undefined)
      .catch((e: Error) => {
        lastError = e;
      });
  };
  const timer = setInterval(tick, everyMs);

  const sampler: Sampler = {
    samples,
    baselineIndex: -1,
    endIndex: -1,
    async sample() {
      await inFlight;
      return take();
    },
    async markBaseline() {
      await this.sample();
      this.baselineIndex = samples.length - 1;
      return this.baselineIndex;
    },
    async markEnd() {
      await this.sample();
      this.endIndex = samples.length - 1;
      return this.endIndex;
    },
    async stop() {
      clearInterval(timer);
      await inFlight;
      if (lastError) throw new Error(`sampling the agent failed: ${lastError.message}`);
      return samples;
    },
  };
  return sampler;
}
