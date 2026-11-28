/**
 * Exercises the CheckWindowsApps DHCP Server checks end-to-end against the
 * real nscp binary.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckWindowsApps
 * --boot --query <cmd> ...` — which loads the module, runs the check and
 * prints the Nagios-style result line. No server/port/docker needed, and the
 * `k=v` tokens exercise the same REST-style argument parsing as the web API.
 *
 * A build machine is not a DHCP server, so the suite pins the documented
 * contract for whichever state the host is in: without the role both checks
 * are UNKNOWN with a message that says the role is missing; with it they
 * render their scopes or counters. Either way, never an argument error.
 *
 * The module is Windows-only, so the whole suite is skipped elsewhere.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

// What the option parser and the filter engine actually say about a bad
// argument, a mistyped keyword or a malformed expression. Option and filter
// parsing run before the DHCP server is read, so these surface on any host,
// DHCP role or not - which the "rejects" cases below prove for the pattern
// itself.
const ARGUMENT_ERROR =
  /Failed to validate filter|expression is not valid|Failed to parse|unrecognised option|does not take any arguments|Invalid command line/i;
/does not take any arguments|unknown option|invalid expression|error parsing/i;

onWindows("CheckWindowsApps DHCP", () => {
  let nscp: NscpInstance;

  /**
   * Run a DHCP query and return the combined output and the exit code,
   * which is the Nagios status (0 OK / 1 WARNING / 2 CRITICAL / 3 UNKNOWN).
   */
  async function queryWithCode(
    command: string,
    args: string[] = [],
  ): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      ["client", "--module", "CheckWindowsApps", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
      },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  async function query(command: string, args: string[] = []): Promise<string> {
    return (await queryWithCode(command, args)).out;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  // --- check_dhcp_scopes -------------------------------------------------------

  it("check_dhcp_scopes reports scope utilisation or the no-DHCP-server contract", async () => {
    const out = await query("check_dhcp_scopes", []);
    // DHCP server: "all N scopes are below their thresholds", a problem list
    // of "<subnet>/<prefix> (<name>): N% used", or "No enabled DHCP scopes
    // found". Anything else: the service is not there to ask.
    expect(out).toMatch(
      /scopes are below their thresholds|\d+\.\d+\.\d+\.\d+\/\d+ \(.*\): ([\d.]+% used|no statistics)|No DHCP scopes matched the filter|DHCP Server service is not running or not installed on this host|dhcpsapi\.dll\) is not available/,
    );
    expect(out).not.toMatch(ARGUMENT_ERROR);
  });

  it("check_dhcp_scopes accepts its keywords and the short thresholds", async () => {
    const out = await query("check_dhcp_scopes", [
      "filter=state = 'enabled' or state = 'disabled'",
      "warning=free < 20 or used_pct > 70",
      "critical=free < 5",
      "detail-syntax=${scope} ${name} ${state} ${in_use}/${addresses} ${pending_offers} ${free_pct} ${usage} ${stats_known} ${enabled}",
      "empty-state=ok",
    ]);
    expect(out).not.toMatch(ARGUMENT_ERROR);
    const short = await query("check_dhcp_scopes", ["w=70", "c=90"]);
    expect(short).not.toMatch(ARGUMENT_ERROR);
  });

  it("check_dhcp_scopes is UNKNOWN, not OK, on a host without the DHCP server", async () => {
    // empty-state only covers a server with no scopes; a server that cannot be
    // asked must never read as healthy.
    const { out, code } = await queryWithCode("check_dhcp_scopes", ["empty-state=ok"]);
    if (
      /DHCP Server service is not running or not installed|dhcpsapi\.dll\) is not available/.test(
        out,
      )
    ) {
      expect(code).toBe(3);
    } else {
      expect(out).not.toMatch(ARGUMENT_ERROR);
    }
  });

  it("check_dhcp_scopes rejects a mistyped keyword (the ARGUMENT_ERROR control)", async () => {
    const warn = await queryWithCode("check_dhcp_scopes", ["warning=usedpct > 80"]);
    expect(warn.out).toMatch(/Warning expression is not valid: usedpct > 80/);
    expect(warn.out).toMatch(ARGUMENT_ERROR);
    expect(warn.code).toBe(3);
    const syntax = await query("check_dhcp_scopes", ["detail-syntax=${no_such_keyword}"]);
    expect(syntax).toMatch(ARGUMENT_ERROR);
    const option = await query("check_dhcp_scopes", ["no-such-option=1"]);
    expect(option).toMatch(ARGUMENT_ERROR);
  });

  // --- check_dhcp_server -------------------------------------------------------

  it("check_dhcp_server reports the packet rates or the missing-counters contract", async () => {
    const out = await query("check_dhcp_server", []);
    expect(out).toMatch(
      /[\d.]+ discovers\/s, [\d.]+ offers\/s, [\d.]+ requests\/s, [\d.]+ acks\/s, [\d.]+ nacks\/s, [\d.]+ declines\/s, [\d.]+ queued|DHCP Server performance counters are not available/,
    );
    expect(out).not.toMatch(ARGUMENT_ERROR);
  });

  it("check_dhcp_server is UNKNOWN when the counters are missing", async () => {
    const { out, code } = await queryWithCode("check_dhcp_server", []);
    if (/DHCP Server performance counters are not available/.test(out)) {
      expect(code).toBe(3);
    } else {
      expect(out).not.toMatch(ARGUMENT_ERROR);
    }
  });

  it("check_dhcp_server accepts every counter keyword in an expression", async () => {
    const out = await query("check_dhcp_server", [
      "warning=packets_received > 1000 or duplicates_dropped > 10 or packets_expired > 0 or informs > 100 or releases > 100",
      "critical=conflict_queue > 100 or offers > 1000 or discovers > 1000 or requests > 1000 or acks > 1000",
    ]);
    expect(out).not.toMatch(ARGUMENT_ERROR);
  });

  it("check_dhcp_server rejects a mistyped keyword (the ARGUMENT_ERROR control)", async () => {
    const { out, code } = await queryWithCode("check_dhcp_server", ["warning=nack > 10"]);
    expect(out).toMatch(/Warning expression is not valid: nack > 10/);
    expect(out).toMatch(ARGUMENT_ERROR);
    expect(code).toBe(3);
  });

  it("check_dhcp_server takes the short thresholds", async () => {
    const out = await query("check_dhcp_server", ["w=40", "c=100"]);
    expect(out).not.toMatch(ARGUMENT_ERROR);
  });
});
