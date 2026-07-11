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
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  WARNING,
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

  // --- check_disk_health physical-disk device rows (Windows) ----------------

  it("check_disk_health exposes physical-disk device state (Windows)", async () => {
    if (!onWindows) return; // device rows come from MSFT_PhysicalDisk / MSFT_Disk.
    // has_device rows carry health_status; scope to them and don't trip on
    // healthy disks. Hosts without the Storage provider select zero rows -> OK.
    const q = await pollQuery(
      key,
      "check_disk_health",
      {
        filter: "has_device = 1",
        warning: "health_status = 'Warning'",
        critical: "health_status = 'Unhealthy' or is_offline = 1",
        "detail-syntax": "${friendly_name}: ${health_status} ${media_type}",
      },
      (r) => r.result !== UNKNOWN,
    );
    expect(q.result).toBe(OK);
  });

  // --- check_drivesize required-drives (§4.10) ------------------------------

  it("check_drivesize require= goes CRITICAL when a mandatory drive is missing (Windows)", async () => {
    if (!onWindows) return;
    // Q: is almost never present; require it while scanning all drives.
    const q = await executeQuery(key, "check_drivesize", {
      drive: "*",
      require: "Q:",
      warning: "used_pct > 999",
      critical: "used_pct > 999",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/Required drive/i);
  });

  it("check_drivesize require= for a present drive does not trip (Windows)", async () => {
    if (!onWindows) return;
    const q = await executeQuery(key, "check_drivesize", {
      drive: "*",
      require: "C:",
      warning: "used_pct > 999",
      critical: "used_pct > 999",
    });
    expect(q.result).toBe(OK);
  });

  // --- check_storagepool (§3.2, Windows) ------------------------------------

  it("check_storagepool runs and does not fail without Storage Spaces (Windows)", async () => {
    if (!onWindows) return; // Storage Spaces is a Windows feature.
    // A host with no pools returns OK (empty_state=ok); a host with healthy
    // pools also returns OK. Either way it must not be UNKNOWN/error.
    const q = await executeQuery(key, "check_storagepool", {});
    expect(q.result).not.toBe(UNKNOWN);
  });

  // --- check_shadowcopy (§3.3, Windows) -------------------------------------

  it("check_shadowcopy runs and reports a valid status (Windows)", async () => {
    if (!onWindows) return; // VSS shadow copies are a Windows feature.
    // With thresholds disabled the result is deterministic: a host with no shadow
    // copies takes empty-state=ok; a host with copies has no threshold to trip.
    const q = await executeQuery(key, "check_shadowcopy", {
      warning: "none",
      critical: "none",
      "empty-state": "ok",
    });
    expect(q.result).toBe(OK);
    // Either "No shadow copies found" (typical CI) or per-volume "<n> copies" rows.
    expect(messageOf(q)).toMatch(/shadow|copies/i);
  });

  it("check_shadowcopy empty-state=critical is honoured (Windows)", async () => {
    if (!onWindows) return;
    // A host with no shadow copies goes CRITICAL on the empty state; a host WITH
    // copies (thresholds disabled) stays OK. Never UNKNOWN/error either way.
    const q = await executeQuery(key, "check_shadowcopy", {
      warning: "none",
      critical: "none",
      "empty-state": "critical",
    });
    expect([OK, CRITICAL]).toContain(q.result);
  });

  // --- check_uncpath (§3.1, Windows) ----------------------------------------

  it("check_uncpath without a path is reported as an error (Windows)", async () => {
    if (!onWindows) return;
    const q = await executeQuery(key, "check_uncpath", {});
    expect(q.result).toBe(UNKNOWN);
    expect(messageOf(q)).toMatch(/No path specified/i);
  });

  // --- inodes (check_drivesize, Linux) --------------------------------------

  it("check_drivesize exposes inode counts (Linux)", async () => {
    if (onWindows) return; // inode keywords are a Unix statvfs concept.
    const q = await executeQuery(key, "check_drivesize", {
      drive: ROOT_DRIVE,
      warning: "inodes_used_pct > 100",
      critical: "inodes_used_pct > 100",
      "top-syntax": "${list}",
      "detail-syntax":
        "inodes ${inodes_used}/${inodes_total} (${inodes_used_pct}% used, ${inodes_free} free)",
    });
    expect(q.result).toBe(OK);
    const m = messageOf(q);
    const total = Number(/\/(\d+) /.exec(m)?.[1]);
    expect(total).toBeGreaterThan(0); // a real filesystem has inodes
    expect(m).toMatch(/inodes \d+\/\d+ \(\d+% used, \d+ free\)/);
  });

  // --- checksums (check_files, both platforms) ------------------------------

  it("check_files computes file content checksums", async () => {
    // A file with known content -> known digests (independent of platform).
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-hash-"));
    fs.writeFileSync(path.join(dir, "hash.txt"), "hello world");
    try {
      const q = await executeQuery(key, "check_files", {
        path: dir,
        pattern: "hash.txt",
        "top-syntax": "${list}",
        "detail-syntax": "md5=${md5_checksum} sha256=${sha256_checksum}",
      });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("md5=5eb63bbbe01eeed093cb22bb8f5acdc3");
      expect(messageOf(q)).toContain(
        "sha256=b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9",
      );
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("check_files can alert on an unexpected checksum", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-hash-"));
    fs.writeFileSync(path.join(dir, "hash.txt"), "hello world");
    try {
      const q = await executeQuery(key, "check_files", {
        path: dir,
        pattern: "hash.txt",
        critical: "sha256_checksum != 'deadbeef'",
      });
      expect(q.result).toBe(CRITICAL);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  // --- check_mount (Linux) ---------------------------------------------------

  it("check_mount confirms the root filesystem is mounted (Linux)", async () => {
    if (onWindows) return; // check_mount is implemented on Unix.
    const q = await executeQuery(key, "check_mount", { mount: ROOT_DRIVE });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/mounts are as expected/i);
  });

  it("check_mount reports a missing mount as critical (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_mount", { mount: "/no/such/mount/point" });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/not mounted/i);
  });

  it("check_mount detects an unexpected fstype (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_mount", { mount: ROOT_DRIVE, fstype: "no_such_fs" });
    // fstype mismatch on a specific mount -> issue -> warning by default.
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toMatch(/fstype differs/i);
  });
});
