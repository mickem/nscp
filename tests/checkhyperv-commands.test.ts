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
const NO_NAMESPACE =
  /Hyper-V virtual machine information not available: the Hyper-V role is not installed on this host \(root\\virtualization\\v2 missing\)/;

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
    expect(out).toMatch(/'vms_ok'=\d+/);
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
    expect(out).toMatch(/total: \d+% total \(\d+% guest, \d+% hypervisor\)/);
    expect(out).toMatch(/'total'=\d+%/);
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
    if (NO_NAMESPACE.test(out)) return;
    expect(out).toMatch(
      /all \d+ virtual machine\(s\) ok|heartbeat \w+, health \w+|No virtual machines found/,
    );
  });

  it("check_hyperv_vms accepts pinned thresholds and filters on the VM keywords", async () => {
    // An impossible filter must not error out even without the role; with it,
    // the empty set renders the top syntax given here.
    const out = await query("check_hyperv_vms", [
      "filter=state = 'no such state'",
      "empty-state=ok",
      "top-syntax=no VMs matched",
      "warning=uptime < 0",
    ]);
    expect(out).toMatch(/role is not installed|no VMs matched/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });
});
