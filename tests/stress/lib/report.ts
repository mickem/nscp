import * as fs from "fs";
import * as path from "path";

import type { Knobs } from "./knobs";
import type { LoadStats } from "./load";
import type { GrowthReport } from "./invariants";
import type { ProcessSample } from "./process-stats";

/** Where the per-scenario results land; uploaded as a CI artifact. */
export const RESULTS_DIR = path.resolve(__dirname, "..", "results");

export interface ScenarioReport {
  scenario: string;
  startedAt: string;
  platform: string;
  knobs: Knobs;
  /** Throughput and latency of the load the scenario generated, when it did. */
  load?: LoadStats;
  /** check_ok latency in ms after the load stopped. */
  responsiveAfterMs?: number;
  growth?: GrowthReport;
  /** RSS and thread samples over the run, one per second. */
  samples: ProcessSample[];
  /** Whatever else the scenario measured (delivered beats, mix counts, ...). */
  extra?: Record<string, unknown>;
}

/** Write `stress/results/<scenario>.json` and print the one-line summary. */
export function writeReport(report: ScenarioReport): string {
  fs.mkdirSync(RESULTS_DIR, { recursive: true });
  const file = path.join(RESULTS_DIR, `${report.scenario}.json`);
  fs.writeFileSync(file, JSON.stringify(report, null, 2) + "\n");
  const parts = [`[stress] ${report.scenario}:`];
  if (report.load) {
    const l = report.load;
    parts.push(
      `${l.requests} requests, ${l.perSecond.toFixed(0)}/s, p50 ${l.latencyMs.p50.toFixed(1)} ms, ` +
        `p95 ${l.latencyMs.p95.toFixed(1)} ms, p99 ${l.latencyMs.p99.toFixed(1)} ms, ${l.errors.count} errors;`,
    );
  }
  if (report.growth) {
    const g = report.growth;
    parts.push(
      `RSS ${(g.baseline.rssBytes / 1048576).toFixed(1)} -> ${(g.final.rssBytes / 1048576).toFixed(1)} MiB ` +
        `(${g.rssGrowthPct >= 0 ? "+" : ""}${g.rssGrowthPct.toFixed(1)}%), threads ${g.baseline.threads} -> ${g.final.threads};`,
    );
  }
  if (report.responsiveAfterMs !== undefined) {
    parts.push(`check_ok ${report.responsiveAfterMs} ms after load;`);
  }
  parts.push(`written to ${path.relative(process.cwd(), file)}`);
  // eslint-disable-next-line no-console
  console.log(parts.join(" "));
  return file;
}
