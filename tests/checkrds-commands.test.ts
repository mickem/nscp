/**
 * Exercises the CheckRDS module end-to-end (Windows only — CheckRDS is a
 * Windows module) via one-shot client queries: `nscp client --module CheckRDS
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

(onWindows ? describe : describe.skip)("CheckRDS commands", () => {
  let nscp: NscpInstance;

  /** Run a CheckRDS query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    const r = await nscp.run(["client", "--module", "CheckRDS", "--boot", "--query", command, ...args], {
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
    const out = await query("check_rds_licenses", ["warning=total < 0", "critical=total < 0"]);
    expect(out).toMatch(/licensing role is not installed|OK/);
    expect(out).not.toMatch(/(^|\s)(WARNING|CRITICAL)\b/);
  });

  it("check_rds_licenses accepts filter expressions on the licensing keywords", async () => {
    // An impossible filter must not error out even without the role installed;
    // with the role it yields the empty-set state (unknown by default).
    const out = await query("check_rds_licenses", ["filter=keypack_type = 999", "empty-state=ok", "top-syntax=no packs matched"]);
    expect(out).toMatch(/licensing role is not installed|no packs matched/);
  });
});
