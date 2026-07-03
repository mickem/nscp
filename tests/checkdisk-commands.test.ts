/**
 * Exercises the CheckDisk module's drive/IO commands end-to-end on BOTH
 * platforms (the module registers as "CheckDisk" on Windows and Linux; the
 * platform-specific bits live in check_*_win.cpp / check_*_unix.cpp).
 *
 * check_files / check_single_file and the deeper Linux-only drivesize cases
 * are covered by checkdisk-unix.test.ts; this suite focuses on cross-platform
 * parity for check_drivesize, check_disk_io and check_disk_health.
 *
 * Queries run over the REST API against a long-lived `nscp test` process:
 * disk I/O rates come from a background collector that needs a couple of
 * 1-second samples before it has data.
 */
import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  executeQuery,
  messageOf,
  perfOf,
  pollQuery,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32";
const ROOT_DRIVE = onWindows ? "c:" : "/";

describe("CheckDisk commands", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckDisk");
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  // --- check_drivesize -------------------------------------------------------

  it("check_drivesize reports the root drive with perf data", async () => {
    const q = await executeQuery(key, "check_drivesize", {
      drive: ROOT_DRIVE,
      warning: "used > 100%",
      critical: "used > 100%",
    });
    expect(q.result).toBe(OK);
    const perfKeys = Object.keys(perfOf(q));
    expect(perfKeys.some((k) => k.includes(ROOT_DRIVE) && /used/.test(k))).toBe(true);
  });

  it("check_drivesize trips a threshold that is always exceeded", async () => {
    const q = await executeQuery(key, "check_drivesize", {
      drive: ROOT_DRIVE,
      critical: "used > 0",
    });
    expect(q.result).toBe(CRITICAL);
  });

  // --- check_disk_io ----------------------------------------------------------

  it("check_disk_io reports per-device IO rates after collector warm-up", async () => {
    const args = {
      warning: "total_bytes_per_sec > 999999999999",
      critical: "total_bytes_per_sec > 999999999999",
    };
    const q = await pollQuery(key, "check_disk_io", args, (r) => r.result === OK);
    expect(q.result).toBe(OK);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });

  // --- check_disk_health -------------------------------------------------------

  it("check_disk_health merges space and IO data", async () => {
    const args = {
      warning: "free_pct < 0",
      critical: "free_pct < 0",
    };
    const q = await pollQuery(key, "check_disk_health", args, (r) => r.result === OK);
    expect(q.result).toBe(OK);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });

  it("check_disk_health does not evaluate space thresholds on IO-only devices", async () => {
    // Regression for the LVM false-CRITICAL: devices whose filesystems can't
    // be mapped back get IO-only rows with no space data (has_space=0). The
    // thresholds below are the DEFAULT space clauses verbatim — guarded by
    // has_space — so such rows must never trip on their unset (0) free_pct.
    // The defaults' percent_disk_time clause is dropped to keep the test
    // deterministic under CI disk load, and the filter restricts matching to
    // IO-only rows so hosts whose real filesystems are legitimately low on
    // space cannot interfere; hosts where every device maps cleanly simply
    // select zero rows, which is also OK.
    const q = await pollQuery(
      key,
      "check_disk_health",
      {
        filter: "has_space = 0",
        warning: "has_space = 1 and free_pct < 20",
        critical: "has_space = 1 and free_pct < 10",
      },
      (r) => r.result !== UNKNOWN,
    );
    expect(q.result).toBe(OK);
    expect(messageOf(q)).not.toMatch(/free_pct/); // no threshold annotation on the rows
  });
});
