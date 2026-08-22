/**
 * Exercises the Remote Desktop Services checks of the CheckWindowsApps module
 * end-to-end (Windows only) via one-shot client queries: `nscp client --module CheckWindowsApps
 * --boot --query <cmd> ...`. No server/port needed, and `k=v` arguments travel
 * as single tokens, exercising the same REST-style argument parsing as the web
 * API.
 *
 * The machines running this suite are not RD licensing servers, so
 * check_rds_licenses asserts the documented no-data contract (a clean
 * "role is not installed" message) — with an escape hatch that still validates
 * the real output shape if the suite ever runs on a licensing server.
 * Client-query output is the raw Nagios message with no status-word prefix.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32";

(onWindows ? describe : describe.skip)("CheckWindowsApps RDS commands", () => {
  let nscp: NscpInstance;

  /** Run a CheckWindowsApps query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckWindowsApps", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  // --- check_rds_licenses ---------------------------------------------------

  it("check_rds_licenses reports the documented contract (key packs or role-not-installed)", async () => {
    const out = await query("check_rds_licenses");
    expect(out).toMatch(
      /Remote Desktop licensing information not available: the Remote Desktop licensing role is not installed \(Win32_TSLicenseKeyPack missing\)|issued/,
    );
  });

  it("check_rds_licenses accepts pinned REST-style thresholds as single tokens", async () => {
    // Pinned always-false thresholds: deterministic regardless of how many
    // CALs the host has - OK when the role is present, the documented message
    // when it is not. Also proves `warning=...`/`critical=...` parse as single
    // k=v tokens (the REST transport's argument shape).
    const out = await query("check_rds_licenses", ["warning=total_licenses < 0", "critical=total_licenses < 0"]);
    expect(out).toMatch(/licensing role is not installed|OK/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  it("check_rds_licenses accepts filter expressions on the licensing keywords", async () => {
    // An impossible filter must not error out even without the role installed;
    // with the role it yields the empty-set state (unknown by default).
    const out = await query("check_rds_licenses", ["filter=keypack_type = 999", "empty-state=ok", "top-syntax=no packs matched"]);
    expect(out).toMatch(/licensing role is not installed|no packs matched/);
  });

  // --- check_rds_sessions ---------------------------------------------------
  // The "Terminal Services" counter object exists on every Windows SKU (the
  // console session counts), so these run against live data everywhere; the
  // not-available message stays accepted for stripped-down hosts.

  it("check_rds_sessions reports session counts with perfdata", async () => {
    const out = await query("check_rds_sessions");
    if (/counters \(Terminal Services\) not available/.test(out)) return;
    expect(out).toMatch(/\d+ active, \d+ inactive \(\d+ total\)/);
    expect(out).toMatch(/'sessions_active'=\d+/);
    expect(out).toMatch(/'sessions_total'=\d+/);
  });

  it("check_rds_sessions applies pinned thresholds deterministically", async () => {
    // total_sessions >= 0 is always true -> WARNING regardless of how many sessions
    // the host happens to have.
    const out = await query("check_rds_sessions", ["warning=total_sessions >= 0"]);
    expect(out).toMatch(/counters \(Terminal Services\) not available|WARNING/);
  });

  // --- check_rds_session_load -----------------------------------------------

  it("check_rds_session_load reports per-session cpu and working set", async () => {
    const out = await query("check_rds_session_load");
    if (/counters \(Terminal Services Session\) not available/.test(out)) return;
    // At least the console/services sessions exist on any interactive host.
    expect(out).toMatch(/% cpu, \d+B working set/);
    expect(out).toMatch(/_working_set'=\d+B/);
  });

  it("check_rds_session_load accepts averages=true and sessions-only=true as valued booleans", async () => {
    // A bool_switch would reject the valued form with "does not take any
    // arguments" before touching any counters.
    const out = await query("check_rds_session_load", ["averages=true", "sessions-only=true"]);
    expect(out).not.toMatch(/does not take any arguments/);
    if (/counters \(Terminal Services Session\) not available/.test(out)) return;
    // The Services aggregate must be filtered out by sessions-only.
    expect(out).not.toMatch(/Services:/);
  });

  // --- check_rds_broker -----------------------------------------------------

  it("check_rds_broker reports broker counters or the documented no-broker contract", async () => {
    const out = await query("check_rds_broker");
    expect(out).toMatch(/Connection Broker counters \(Remote Desktop Connection Broker Counterset\) not available - is this host an RD Connection Broker\?| = /);
  });

  it("check_rds_broker accepts averages=true and counter filters", async () => {
    const out = await query("check_rds_broker", ["averages=true", "filter=counter like 'Failed'", "empty-state=ok"]);
    expect(out).not.toMatch(/does not take any arguments/);
    expect(out).toMatch(/not available|OK|Failed/);
  });
});
