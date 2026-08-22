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
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32";

const NOT_AVAILABLE = /not available - is the Web Server \(IIS\) role installed\?/;

// Strict mode: the CI job provisions the real IIS role (see
// integration-tests-windows.yml) and sets NSCP_EXPECT_IIS=1, turning the
// role-not-installed fallback from an accepted contract into a failure - the
// checks must then produce live counter data. Without the flag the suite
// keeps accepting both shapes so it runs on any developer machine.
const expectIis = process.env.NSCP_EXPECT_IIS === "1";

(onWindows ? describe : describe.skip)("CheckWindowsApps IIS commands", () => {
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

  // --- check_iis_worker_processes -------------------------------------------

  it("check_iis_worker_processes reports workers or the documented contracts", async () => {
    // No workers is a normal state (idle pools spin down), so an IIS host may
    // also answer with the empty-set OK message.
    const out = await query("check_iis_worker_processes");
    expect(out).toMatch(/not available|No IIS worker processes running|active requests/);
  });

  // --- check_iis_request_queues ---------------------------------------------

  it("check_iis_request_queues reports queues or the documented contracts", async () => {
    const out = await query("check_iis_request_queues", ["warning=queue_length > 800", "critical=queue_length > 1000"]);
    expect(out).toMatch(/not available|No HTTP.sys request queues found|queued/);
  });
});
