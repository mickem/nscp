/**
 * The `nscp wmi` command line, ported from the legacy
 * scripts/python/test_w32_wmi.py (an `nscp unit` script run by
 * acceptance-tests.bat). checkwmi-access covers check_wmi and its access
 * modes; this covers the operator's browsing verbs, which nothing else runs:
 * --list-all-ns, --list-classes and --select.
 *
 * The legacy script reached them in-process through Core.simple_exec; here
 * they go through the real CLI, the way an operator types them. Every class
 * and namespace used is on every Windows install.
 */
import { NscpInstance, describeOnWindows } from "@fixtures/index";

jest.setTimeout(180_000);

describeOnWindows("CheckWMI command line", () => {
  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckWMI: "enabled" } });
  });

  /** One `nscp wmi -- <args>` run. */
  async function wmi(args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["wmi", "--", ...args], { allowFailure: true, timeout: 120_000 });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** Non-empty output lines, trimmed. */
  function linesOf(out: string): string[] {
    return out
      .split(/\r?\n/)
      .map((l) => l.trim())
      .filter((l) => l.length > 0);
  }

  it("--list-all-ns lists the namespaces under the one given", async () => {
    const { out, code } = await wmi(["--list-all-ns", "--namespace", "root\\CIMV2"]);
    expect(code).toBe(0);
    expect(out).toMatch(/CIMV2/i);
  });

  it("--list-classes lists root\\cimv2 by default", async () => {
    const { out, code } = await wmi(["--list-classes", "--simple"]);
    expect(code).toBe(0);
    const classes = linesOf(out);
    expect(classes).toContain("Win32_Processor");
    // An event consumer class lives in root\subscription, not here.
    expect(classes).not.toContain("LogFileEventConsumer");
  });

  it("--list-classes honours --namespace", async () => {
    const { out, code } = await wmi([
      "--list-classes",
      "--simple",
      "--namespace",
      "root\\subscription",
    ]);
    expect(code).toBe(0);
    const classes = linesOf(out);
    expect(classes).toContain("LogFileEventConsumer");
    expect(classes).not.toContain("Win32_Processor");
  });

  it("--select runs a query and renders its rows", async () => {
    const { out, code } = await wmi([
      "--select",
      "SELECT DeviceId, AddressWidth, Caption, Name FROM Win32_Processor",
      "--simple",
    ]);
    expect(code).toBe(0);
    // A header and at least one processor row.
    expect(linesOf(out).length).toBeGreaterThan(1);
    expect(out).toContain("CPU0");
  });

  it("--select reports a bad query as an error", async () => {
    const { out, code } = await wmi(["--select", "SELECT * FROM NoSuchClass_nscp_1f", "--simple"]);
    expect(code).not.toBe(0);
    expect(out).toMatch(/ERROR/);
  });
});
