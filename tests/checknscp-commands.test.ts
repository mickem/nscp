/**
 * Exercises the CheckNSCP module end-to-end against the real nscp binary.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckNSCP
 * --boot --query <cmd> ...` — which loads the module, runs the check and prints
 * the Nagios-style result line. No server/port/docker needed, and because the
 * client passes each argument as a single `key=value` token it exercises the
 * same argument parsing REST does.
 *
 * The interesting surface is check_nscp: it reads the crash archive folder from
 * `/settings/crash` `archive folder`, so the suite points that at a scratch
 * directory and plants crash reports in it with controlled timestamps. That
 * makes the crash count, `${last_crash}` and `${crash_age}` deterministic on
 * any host, including Linux where nothing ever writes a real crash report.
 */
import fs from "node:fs";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

/** Write a file into `dir` and stamp it `ageSeconds` old. */
function plant(dir: string, name: string, ageSeconds: number): string {
  const file = path.join(dir, name);
  fs.writeFileSync(file, "crash report\n");
  const when = new Date(Date.now() - ageSeconds * 1000);
  fs.utimesSync(file, when, when);
  return file;
}

describe("CheckNSCP", () => {
  /** An agent whose crash archive folder is empty. */
  let clean: NscpInstance;
  /** An agent whose crash archive folder holds two reports and a decoy. */
  let crashed: NscpInstance;

  /** Run a CheckNSCP query and return the output plus the Nagios exit code. */
  async function queryOn(
    instance: NscpInstance,
    command: string,
    args: string[] = [],
  ): Promise<{ out: string; code: number }> {
    const r = await instance.run(["client", "--module", "CheckNSCP", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  const query = (command: string, args: string[] = []) => queryOn(clean, command, args);

  beforeAll(async () => {
    clean = new NscpInstance();
    const emptyDir = clean.scratch("crash-dumps");
    await clean.configure({ "/settings/crash": { "archive folder": emptyDir } });

    crashed = new NscpInstance();
    const crashDir = crashed.scratch("crash-dumps");
    // 2h old, and 30m old: the newer one must win as ${last_crash}.
    plant(crashDir, "2026-01-01-00-00-00.crash", 2 * 60 * 60);
    plant(crashDir, "2026-01-02-00-00-00.crash", 30 * 60);
    // Neither of these is a crash report; they must not be counted, and must
    // never be picked as the newest even though this one is the newest file.
    plant(crashDir, "nsclient.log", 0);
    await crashed.configure({ "/settings/crash": { "archive folder": crashDir } });
  });

  // --- check_nscp ----------------------------------------------------------

  it("reports a healthy agent with no crashes", async () => {
    const { out, code } = await query("check_nscp", ["critical=crashes > 0"]);
    expect(out).toMatch(/^OK: 0 crash\(es\), \d+ error\(s\), uptime /m);
    expect(code).toBe(0);
  });

  it("emits perfdata for the keywords the thresholds name", async () => {
    const { out } = await query("check_nscp", ["warning=crashes > 5", "critical=crashes > 10"]);
    // perf-syntax defaults to "nscp", and the crashes keyword adds a _crashes
    // suffix, so the metric carries both thresholds.
    expect(out).toMatch(/'nscp_crashes'=0;5;10/);
  });

  it("counts the crash reports in the configured archive folder", async () => {
    const { out, code } = await queryOn(crashed, "check_nscp", ["critical=crashes > 0"]);
    // Two .crash files; the .log decoy in the same folder is not a report.
    expect(out).toMatch(/^CRITICAL: 2 crash\(es\), /m);
    expect(code).toBe(2);
  });

  it("names the most recent crash report", async () => {
    const { out } = await queryOn(crashed, "check_nscp", [
      "critical=crashes > 0",
      "detail-syntax=last=${last_crash}",
    ]);
    expect(out).toMatch(/last=2026-01-02-00-00-00\.crash/);
  });

  it("never picks a non-report as the most recent crash", async () => {
    // nsclient.log is the newest file in the folder but must be ignored.
    const { out } = await queryOn(crashed, "check_nscp", ["detail-syntax=last=${last_crash}"]);
    expect(out).not.toMatch(/nsclient\.log/);
  });

  it("applies duration units to crash_age thresholds", async () => {
    // The newest report is 30 minutes old. With the duration converter wired up
    // "1h" is 3600 seconds, so 1800 < 3600 trips. Without it "1h" would read as
    // 1 and this would stay OK — which is exactly the regression to catch.
    const { out, code } = await queryOn(crashed, "check_nscp", ["critical=crash_age < 1h"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(code).toBe(2);

    const older = await queryOn(crashed, "check_nscp", ["critical=crash_age < 1m"]);
    expect(older.out).toMatch(/^OK/m);
    expect(older.code).toBe(0);
  });

  it("renders crash_age as a duration", async () => {
    const { out } = await queryOn(crashed, "check_nscp", ["detail-syntax=age=${crash_age}", "max-unit=m"]);
    // 30 minutes, rendered with minutes as the largest unit.
    expect(out).toMatch(/age=0:30/);
  });

  it("reports crash_age as 'none' when nothing has crashed", async () => {
    const { out } = await query("check_nscp", ["detail-syntax=age=${crash_age}"]);
    expect(out).toMatch(/age=none/);
  });

  it("exposes uptime, version and the error counter as keywords", async () => {
    const { out } = await query("check_nscp", [
      "detail-syntax=up=${uptime} v=${version} e=${errors} lc=[${last_crash}] le=[${last_error}]",
    ]);
    expect(out).toMatch(/up=\S+/);
    expect(out).toMatch(/v=\d+\.\d+\.\d+/);
    expect(out).toMatch(/e=\d+/);
    // Nothing has crashed and (on a clean boot) nothing has been logged.
    expect(out).toMatch(/lc=\[\]/);
  });

  it("evaluates thresholds against the errors keyword", async () => {
    // errors is always >= 0, so this must trip regardless of host state; it
    // proves the keyword resolves as a number rather than an unknown name.
    const { out, code } = await query("check_nscp", ["warning=errors >= 0"]);
    expect(out).toMatch(/^WARNING/m);
    expect(code).toBe(1);
  });

  it("honours max-unit when rendering uptime", async () => {
    const { out } = await query("check_nscp", ["max-unit=s", "detail-syntax=up=${uptime}"]);
    expect(out).toMatch(/up=\d+/);
  });

  it("rejects an invalid max-unit", async () => {
    const { out, code } = await query("check_nscp", ["max-unit=x"]);
    expect(out).toMatch(/Invalid time unit/);
    expect(code).toBe(3);
  });

  it("accepts a valued boolean the way REST sends it", async () => {
    // REST passes flags as a single "show-all=true" token; an option declared
    // with bool_switch would reject that with "does not take any arguments".
    const { out, code } = await query("check_nscp", ["show-all=true", "critical=crashes > 0"]);
    expect(out).not.toMatch(/does not take any arguments/);
    expect(code).toBe(0);
  });

  it("supports a custom top-syntax", async () => {
    const { out } = await query("check_nscp", ["critical=crashes > 0", "top-syntax=agent is ${status}"]);
    expect(out).toMatch(/^agent is OK/m);
  });

  it("filters the agent out entirely", async () => {
    // An unmatched filter leaves nothing to report, which the filter engine
    // reports as UNKNOWN unless the caller says otherwise via empty-state.
    const excluded = await query("check_nscp", ["filter=crashes > 0"]);
    expect(excluded.code).toBe(3);

    const { out, code } = await query("check_nscp", [
      "filter=crashes > 0",
      "empty-state=ok",
      "empty-syntax=nothing to report",
    ]);
    expect(out).toMatch(/nothing to report/);
    expect(code).toBe(0);
  });

  // --- check_nscp_version --------------------------------------------------

  it("reports the running version", async () => {
    const { out, code } = await query("check_nscp_version");
    expect(out).toMatch(/^OK: \d+\.\d+\.\d+/m);
    expect(code).toBe(0);
  });

  it("evaluates thresholds against the version components", async () => {
    const { out, code } = await query("check_nscp_version", ["critical=release >= 0"]);
    expect(out).toMatch(/^CRITICAL/m);
    expect(code).toBe(2);
  });
});
