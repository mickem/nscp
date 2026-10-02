/**
 * Exercises the IIS checks of the CheckWindowsApps module end-to-end (Windows
 * only) via one-shot client queries: `nscp client --module CheckWindowsApps --boot --query <cmd> ...`. `k=v`
 * arguments travel as single tokens, exercising the same REST-style argument
 * parsing as the web API (including valued booleans like averages=true).
 *
 * Hosts running this suite may not have the IIS role, so every case accepts
 * both documented shapes: real per-pool/site/queue output on an IIS host, and
 * the clean "not available - is the Web Server (IIS) role installed?" message
 * everywhere else. Both come from the same argument-parsing and gather path,
 * so a regression in either fails the test on every machine.
 * Client-query output is the raw Nagios message with no status-word prefix.
 */
import { execFileSync } from "node:child_process";
import path from "node:path";

import { NscpInstance, describeOnWindows } from "@fixtures/index";

jest.setTimeout(120_000);

const NOT_AVAILABLE = /not available - is the Web Server \(IIS\) role installed\?/;

// Strict mode: the CI job provisions the real IIS role (see
// integration-tests-windows.yml) and sets NSCP_EXPECT_IIS=1, turning the
// role-not-installed fallback from an accepted contract into a failure - the
// checks must then produce live counter data. Without the flag the suite
// keeps accepting both shapes so it runs on any developer machine.
const expectIis = process.env.NSCP_EXPECT_IIS === "1";

describeOnWindows("CheckWindowsApps IIS commands", () => {
  let nscp: NscpInstance;

  /** Run a CheckWindowsApps query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckWindowsApps", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    if (expectIis) expect(out).not.toMatch(/not available/);
    return out;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  /** Drive IIS' own appcmd (ships with the Web-Server role). */
  function appcmd(...args: string[]): string {
    const exe = path.join(process.env.windir ?? "C:\\Windows", "system32", "inetsrv", "appcmd.exe");
    return execFileSync(exe, args, { encoding: "utf8", timeout: 60_000 });
  }

  const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

  // --- check_iis_app_pools --------------------------------------------------

  it("check_iis_app_pools reports pool state or the documented no-IIS contract", async () => {
    const out = await query("check_iis_app_pools");
    if (NOT_AVAILABLE.test(out)) {
      expect(out).toMatch(/IIS performance counters \(APP_POOL_WAS\) not available/);
    } else if (expectIis) {
      // The CI job started the default site, so DefaultAppPool has a live
      // WAS counter instance: a real pool record, not the empty-set message.
      expect(out).toMatch(/uptime \d+s, \d+ recycles/);
    } else {
      // Real IIS: every pool line carries a state word and an uptime.
      expect(out).toMatch(/uptime \d+s, \d+ recycles|No application pools found/);
    }
  });

  it("check_iis_app_pools accepts pinned REST-style thresholds as single tokens", async () => {
    // Pinned always-false thresholds: deterministic regardless of host state.
    const out = await query("check_iis_app_pools", ["warning=recycles < 0", "critical=recycles < 0", "empty-state=ok"]);
    expect(out).toMatch(/not available|OK|No application pools found/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  // --- check_iis_sites ------------------------------------------------------

  it("check_iis_sites accepts averages=true as a valued boolean (the REST shape)", async () => {
    // A bool_switch would reject `averages=true` with "does not take any
    // arguments" before ever reaching the counters; both accepted shapes
    // prove the option parsed.
    const out = await query("check_iis_sites", ["averages=true", "warning=connections < 0", "critical=connections < 0", "empty-state=ok"]);
    expect(out).not.toMatch(/does not take any arguments/);
    expect(out).toMatch(/not available|OK|No web sites found|connections/);
    // Provisioned IIS: the started default site is a real record with a
    // connection count, not the empty-set message.
    if (expectIis) expect(out).toMatch(/Default Web Site: \w+, \d+ connections/);
  });

  // --- empty result sets ----------------------------------------------------

  // The #1499 shape: the default crit (`state != 'running' and auto_start != 0`)
  // is force-evaluated with no pool/site bound once a filter matched nothing.
  // Both host shapes must end on a documented message, never WARNING/CRITICAL.

  it("check_iis_app_pools with a filter that matches nothing takes the empty state", async () => {
    const out = await query("check_iis_app_pools", ["filter=pool = 'nosuchpool-1499'", "empty-state=ok"]);
    expect(out).toMatch(/not available|No application pools found/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  it("check_iis_sites with a filter that matches nothing takes the empty state", async () => {
    const out = await query("check_iis_sites", ["filter=site = 'nosuchsite-1499'", "empty-state=ok"]);
    expect(out).toMatch(/not available|No web sites found/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  // --- check_iis_worker_processes -------------------------------------------

  it("check_iis_worker_processes reports workers or the documented contracts", async () => {
    // No workers is a normal state (idle pools spin down), so an IIS host may
    // also answer with the empty-set OK message. That holds in strict mode
    // too: with no w3wp.exe alive the W3SVC_W3WP object has zero instances
    // and PDH expands the wildcard to PDH_CSTATUS_NO_INSTANCE, which the
    // gather must treat as an empty set - not as the role-not-installed
    // fallback, which the query() helper rejects under NSCP_EXPECT_IIS.
    const out = await query("check_iis_worker_processes");
    expect(out).toMatch(/not available|No IIS worker processes running|active requests/);
  });

  it("check_iis_worker_processes reports an idle IIS as no workers, not as counters missing", async () => {
    // Strict mode only: it needs a real IIS to stop. With DefaultAppPool
    // stopped no w3wp.exe is alive, W3SVC_W3WP has zero instances and PDH
    // answers the wildcard with PDH_CSTATUS_NO_INSTANCE. That is the empty
    // set, and the query() helper above fails the test should it come back
    // as the role-not-installed message instead (the shape that made this
    // suite flaky once the primed worker had idled out).
    if (!expectIis) return;
    appcmd("stop", "apppool", "/apppool.name:DefaultAppPool");
    try {
      // WAS takes the worker down asynchronously; give it a moment.
      let out = "";
      for (let attempt = 0; attempt < 30; attempt++) {
        out = await query("check_iis_worker_processes");
        if (/No IIS worker processes running/.test(out)) break;
        await sleep(1000);
      }
      expect(out).toMatch(/No IIS worker processes running/);
      expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL|UNKNOWN)\b/);
    } finally {
      // Hand the pool back warm for whatever queries IIS after this.
      appcmd("start", "apppool", "/apppool.name:DefaultAppPool");
      execFileSync("powershell", ["-NoProfile", "-Command", "Invoke-WebRequest -UseBasicParsing http://localhost/ | Out-Null"], {
        encoding: "utf8",
        timeout: 60_000,
      });
    }
  });

  // --- check_iis_request_queues ---------------------------------------------

  it("check_iis_request_queues reports queues or the documented contracts", async () => {
    const out = await query("check_iis_request_queues", ["warning=queue_length > 800", "critical=queue_length > 1000"]);
    expect(out).toMatch(/not available|No HTTP.sys request queues found|queued/);
  });
});
