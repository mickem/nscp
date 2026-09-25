/**
 * Exercises the CheckHyperV module end-to-end (Windows only) via one-shot
 * client queries: `nscp client --module CheckHyperV --boot --query <cmd> ...`.
 * No server/port needed, and `k=v` arguments travel as single tokens,
 * exercising the same REST-style argument parsing as the web API.
 *
 * The machines running this suite are not Hyper-V hosts, so every check
 * asserts the documented no-data contract (a clean "role not installed"
 * message) — with an escape hatch that still validates the real output shape
 * if the suite ever runs on a Hyper-V host. Client-query output is the raw
 * Nagios message with no status-word prefix.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32";

const NO_COUNTERS =
  /Hyper-V counters \(.*\) not available - is the Hyper-V role installed and the hypervisor running on this host\?/;
// No role (the namespace is missing), or a stopped management service.
const NO_NAMESPACE =
  /Hyper-V virtual machine information not available: (the Hyper-V role is not installed on this host \(root\\virtualization\\v2 missing\)|the Hyper-V management classes are missing)/;
// An unelevated run on a Hyper-V host: WMI hides every VM from the caller
// while the health summary counters still count them.
const HIDDEN_VMS =
  /Hyper-V reports \d+ virtual machine\(s\) on this host but none are visible to this account|cannot tell that there are none/;

(onWindows ? describe : describe.skip)("CheckHyperV commands", () => {
  let nscp: NscpInstance;

  /** Run a CheckHyperV query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(
      ["client", "--module", "CheckHyperV", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
      },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  // --- check_hyperv_host ----------------------------------------------------

  it("check_hyperv_host reports the health summary or the documented no-counters contract", async () => {
    const out = await query("check_hyperv_host");
    if (NO_COUNTERS.test(out)) return;
    expect(out).toMatch(/\d+ VMs ok, \d+ critical, \d+ partitions on \d+ logical processors/);
    expect(out).toMatch(/'vms_health_ok'=\d+/);
    expect(out).toMatch(/'vms_partitions'=\d+/);
  });

  it("check_hyperv_host accepts pinned REST-style thresholds as single tokens", async () => {
    // health_critical < 0 is always false: OK on a Hyper-V host regardless of
    // its VMs, the documented message elsewhere. Also proves `warning=...`/
    // `critical=...` parse as single k=v tokens (the REST transport's shape).
    const out = await query("check_hyperv_host", [
      "warning=health_critical < 0",
      "critical=health_critical < 0",
    ]);
    expect(out).toMatch(/not available|OK/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  // --- check_hyperv_cpu -----------------------------------------------------

  it("check_hyperv_cpu reports the aggregate load or the documented no-counters contract", async () => {
    const out = await query("check_hyperv_cpu", ["averages=false"]);
    if (NO_COUNTERS.test(out)) return;
    // One decimal in the detail line; the perf label is the perf-syntax (the
    // processor) joined to the keyword, and the perf value is rendered by the
    // float perfdata formatter (which may spell a rounded 0.6 as 0.59999).
    expect(out).toMatch(/total: \d+(\.\d)?% total \(\d+(\.\d)?% guest, \d+(\.\d)?% hypervisor\)/);
    expect(out).toMatch(/'total_total_run_time'=\d+(\.\d+)?%?;80;90/);
  });

  it("check_hyperv_cpu accepts averages=false as a valued boolean and per-processor filters", async () => {
    // A bool_switch would reject the valued form with "does not take any
    // arguments" before touching any counters.
    const out = await query("check_hyperv_cpu", [
      "averages=false",
      "filter=processor != 'total'",
      "warning=total_run_time < 0",
      "critical=total_run_time < 0",
    ]);
    expect(out).not.toMatch(/does not take any arguments/);
    if (NO_COUNTERS.test(out)) return;
    expect(out).toMatch(/Hv LP 0/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  // --- check_hyperv_vms -----------------------------------------------------

  it("check_hyperv_vms lists virtual machines or reports the documented no-role contract", async () => {
    const out = await query("check_hyperv_vms");
    if (NO_NAMESPACE.test(out) || HIDDEN_VMS.test(out)) return;
    expect(out).toMatch(
      /all \d+ virtual machine\(s\) ok|heartbeat \w+, health \w+|No virtual machines found/,
    );
  });

  it("check_hyperv_vms accepts pinned thresholds and filters on the VM keywords", async () => {
    // An impossible filter must not error out even without the role; with it,
    // the empty set renders the empty syntax given here in the empty state.
    const out = await query("check_hyperv_vms", [
      "filter=state = 'no such state'",
      "empty-state=ok",
      "empty-syntax=no VMs matched",
      "warning=uptime < 0",
    ]);
    expect(out).toMatch(
      /information not available|none are visible to this account|cannot tell that there are none|no VMs matched/,
    );
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });
});
