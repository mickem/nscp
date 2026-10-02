/**
 * Exercises the CheckDisk module's drive/IO commands end-to-end on BOTH
 * platforms (the module registers as "CheckDisk" on Windows and Linux; the
 * platform-specific bits live in check_*_win.cpp / check_*_unix.cpp).
 *
 * It focuses on cross-platform parity for check_drivesize, check_disk_io,
 * check_disk_health and check_files / check_single_file (the last two ported
 * from the legacy test_w32_file.py, in their own block at the end). The
 * deeper Linux-only drivesize cases are in checkdisk-unix.test.ts.
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
  onWindows,
  describeWithModules,
} from "@fixtures/index";

jest.setTimeout(300_000);

const ROOT_DRIVE = onWindows ? "c:" : "/";

// Skipped where the build has no CheckDisk (macOS, until it is ported).
describeWithModules("CheckDisk")("CheckDisk commands", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    // 1s trend cadence so the drivesize trend keywords (full_in/rate) can
    // accumulate a valid trend (>= 3 samples spanning >= 3x the interval)
    // within the suite's runtime; samples arrive on the collector's tick,
    // which the collection interval below puts at 1s.
    key = await setupQueryNscp(nscp, "CheckDisk", {
      "/settings/disk": {
        "trend interval": "1s",
        // Both keys are new in #1392; setting them here also pins that the
        // module boots with them configured. A 1s cadence makes the collector
        // reach its second sample (needed for every rate and latency) quickly.
        "collection interval": "1s",
        "max collection errors": "3",
      },
    });
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

  // --- check_drivesize trend keywords (full_in / rate) ------------------------

  it("check_drivesize accepts a duration threshold on full_in over REST", async () => {
    // `full_in < 1s` arrives as the single token `warning=full_in < 1s`
    // (duration literal through the k=v path). It can only fire if the drive
    // would fill within one second, so the verdict is deterministically OK.
    const q = await executeQuery(key, "check_drivesize", {
      drive: ROOT_DRIVE,
      warning: "full_in < 1s",
      critical: "full_in < 1s",
    });
    expect(q.result).toBe(OK);
  });

  it("check_drivesize trend keywords follow the sample count, live or not", async () => {
    // Two outcomes are correct here, and which one you get depends on whether
    // the collector tracks this drive at all:
    //
    //  - it does (a real filesystem): with trend interval=1s a valid trend
    //    exists once the samples span three seconds, and rate/full_in go live;
    //  - it does not: `/` inside a container is an overlay mount, which the
    //    collector skips as a pseudo filesystem, so no history is ever
    //    recorded for it and the keywords must report the documented no-data
    //    contract.
    //
    // Asserting "live trend" unconditionally is therefore wrong on
    // containerised CI. What must hold either way is the contract tying the
    // keywords to the history behind them - never a sentinel, and never a half
    // state such as a real rate with too little history behind it.
    const args = {
      drive: ROOT_DRIVE,
      "detail-syntax": "samples=%(trend_samples);span=%(trend_span);rate=%(rate);full_in=%(full_in)",
      "top-syntax": "${list}",
      warning: "used > 100%",
      critical: "used > 100%",
    };
    // trend_buffer trusts a slope only with >= 3 samples spanning >= 3x the
    // trend interval (min_span), which is 3s with the 1s interval configured
    // above. The sample count alone is not that condition: at a 1s collection
    // cadence three samples land inside two seconds, which is a legitimate
    // "not yet", not a live trend.
    const TREND_MIN_SPAN_SECONDS = 3;
    const trendIsLive = (m: string) =>
      Number(/samples=(\d+)/.exec(m)?.[1] ?? 0) >= 3 && Number(/span=(\d+)/.exec(m)?.[1] ?? 0) >= TREND_MIN_SPAN_SECONDS;

    const q = await pollQuery(key, "check_drivesize", args, (r) => trendIsLive(messageOf(r)), 90_000);
    const msg = messageOf(q);
    expect(q.result).toBe(OK);

    const samples = Number(/samples=(\d+)/.exec(msg)?.[1]);
    expect(Number.isFinite(samples)).toBe(true);
    // full_in is a duration or 'never' in both branches - never a number of
    // seconds, and never the empty string.
    expect(msg).toMatch(/full_in=(never|[\dwd: hms]+)/);

    if (trendIsLive(msg)) {
      // Enough history for a regression: the trend is live.
      expect(msg).toMatch(/rate=-?[\d.]+\s?[KMGTP]?i?B\/day/);
      expect(msg).not.toMatch(/rate=unknown/);
      expect(Number(/span=(\d+)/.exec(msg)?.[1])).toBeGreaterThanOrEqual(TREND_MIN_SPAN_SECONDS);
    } else {
      // Below the minimum history (too few samples, too short a span, or no
      // history at all): the optional numbers have no value and say so,
      // rather than reporting a sentinel.
      expect(msg).toMatch(/rate=unknown/);
      expect(msg).toMatch(/full_in=never/);
    }
  });

  it("check_drivesize accepts trend-window and rejects garbage", async () => {
    const ok = await executeQuery(key, "check_drivesize", {
      drive: ROOT_DRIVE,
      "trend-window": "6h",
      warning: "used > 100%",
      critical: "used > 100%",
    });
    expect(ok.result).toBe(OK);

    const bad = await executeQuery(key, "check_drivesize", {
      drive: ROOT_DRIVE,
      "trend-window": "bogus",
    });
    expect(bad.result).toBe(UNKNOWN);
    expect(messageOf(bad)).toMatch(/Invalid trend-window/i);
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

  it("check_disk_io exposes average I/O latency in milliseconds", async () => {
    // Latency needs two collector samples (it is a delta over the interval);
    // thresholds reference the fields so they are emitted as perf data.
    const args = {
      warning: "read_latency > 999999 or write_latency > 999999",
      critical: "total_latency > 999999",
    };
    const q = await pollQuery(key, "check_disk_io", args, (r) => r.result === OK);
    expect(q.result).toBe(OK);
    const perfKeys = Object.keys(perfOf(q));
    expect(perfKeys.some((k) => /read_latency/.test(k))).toBe(true);
    expect(perfKeys.some((k) => /write_latency/.test(k))).toBe(true);
    expect(perfKeys.some((k) => /total_latency/.test(k))).toBe(true);
  });

  it("check_disk_io gives every secondary keyword its own perf label", async () => {
    // Both generators used to be registered without a suffix, so queue_length
    // and percent_disk_time were emitted under the bare ${name} alias and a
    // store that keys by label kept only one of the two. percent_disk_time is
    // the primary metric and keeps the bare device name.
    const q = await pollQuery(
      key,
      "check_disk_io",
      {
        warning: "queue_length > 999999",
        critical: "percent_disk_time > 999999",
      },
      (r) => r.result === OK,
    );
    expect(q.result).toBe(OK);
    const perfKeys = Object.keys(perfOf(q));
    expect(perfKeys.some((k) => k.endsWith("_queue_length"))).toBe(true);
    // One label per metric: the two never share one.
    expect(perfKeys.some((k) => !k.endsWith("_queue_length"))).toBe(true);
    expect(new Set(perfKeys).size).toBe(perfKeys.length);
  });

  it("check_disk_io formats byte rates through format_bytes", async () => {
    // The filter language has no arithmetic, so without these functions a
    // template can only print the raw byte count.
    const q = await pollQuery(
      key,
      "check_disk_io",
      {
        filter: "none",
        "detail-syntax": "%(name)=%(format_bytes(total_bytes_per_sec,'KB'))KB",
        warning: "iops > 999999",
        critical: "iops > 999999",
      },
      (r) => r.result === OK,
    );
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/=[0-9.]+KB/);
  });

  it("check_disk_io compares byte rates in a chosen unit", async () => {
    // convert_bytes returns a number, so the comparison must be numeric: a
    // string comparison would order any two-digit value above 999999.
    const q = await pollQuery(
      key,
      "check_disk_io",
      {
        warning: "convert_bytes(total_bytes_per_sec,'MB') > 999999",
        critical: "convert_bytes(total_bytes_per_sec,'MB') > 999999",
      },
      (r) => r.result !== UNKNOWN,
    );
    expect(q.result).toBe(OK);
  });

  // --- check_disk_health -------------------------------------------------------

  it("check_disk_health gives space and load keywords distinct perf labels", async () => {
    // The default thresholds reference free_pct and percent_disk_time, so the
    // plain command emitted two entries under one label on every row. free_pct
    // is the primary metric and keeps the bare drive name.
    const q = await pollQuery(
      key,
      "check_disk_health",
      {
        filter: "has_space = 1",
        warning: "free_pct < 0",
        critical: "percent_disk_time > 999999",
      },
      (r) => r.result === OK,
    );
    expect(q.result).toBe(OK);
    const perfKeys = Object.keys(perfOf(q));
    expect(perfKeys.some((k) => k.endsWith("_percent_disk_time"))).toBe(true);
    expect(perfKeys.some((k) => !k.endsWith("_percent_disk_time"))).toBe(true);
    expect(new Set(perfKeys).size).toBe(perfKeys.length);
  });

  it("check_disk_health reports no space rather than 0% on IO-only rows", async () => {
    // Rows with no filesystem behind them used to render "0% free" and record
    // a flat 0% free_pct series. Hosts where every device maps to a filesystem
    // select zero rows, which is also a pass.
    const q = await pollQuery(
      key,
      "check_disk_health",
      {
        filter: "has_space = 0",
        "detail-syntax": "${name}=${free_pct}",
        warning: "percent_disk_time > 999999",
        critical: "percent_disk_time > 999999",
      },
      (r) => r.result !== UNKNOWN,
    );
    expect(q.result).toBe(OK);
    const message = messageOf(q);
    expect(message).not.toMatch(/=0%/);
  });

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

  it("check_disk_health size keyword and its deprecated total alias resolve to disk size", async () => {
    // `total` was renamed to `size` (the old name clashes with the generic
    // `total` summary keyword). Both must resolve to the row's disk size in
    // bytes: any real filesystem is larger than 1MB, so the thresholds below
    // trip WARNING. The generic summary `total` (the row count) never would.
    const args = {
      filter: "has_space = 1",
      warning: "size > 1048576",
      critical: "size < 0",
    };
    const q = await pollQuery(key, "check_disk_health", args, (r) => r.result === WARNING);
    expect(q.result).toBe(WARNING);

    const alias = await pollQuery(
      key,
      "check_disk_health",
      { ...args, warning: "total > 1048576" },
      (r) => r.result === WARNING,
    );
    expect(alias.result).toBe(WARNING);
  });

  // --- check_disk_write ------------------------------------------------------
  //
  // Platform-neutral by design (plain file I/O + fsync/_commit), so every case
  // runs on both Windows and Linux. Thresholds are pinned so the result never
  // depends on how fast the CI disk happens to be.

  describe("check_disk_write", () => {
    let scratch: string;

    beforeAll(() => {
      scratch = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-disk-write-"));
    });

    afterAll(() => {
      fs.rmSync(scratch, { recursive: true, force: true });
    });

    it("performs a write/read/delete round-trip and reports OK with perf data", async () => {
      const file = path.join(scratch, "probe.dat");
      const q = await executeQuery(key, "check_disk_write", {
        file,
        size: "64k",
        warning: "total_time > 999999",
        critical: "total_time > 999999",
      });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toMatch(/wrote and read back 65536 bytes in \d+ms/);
      // The test file must be cleaned up again.
      expect(fs.existsSync(file)).toBe(false);
      // Thresholding on total_time emits it as perf data.
      const perfKeys = Object.keys(perfOf(q));
      expect(perfKeys.some((k) => k.includes("total_time"))).toBe(true);
    });

    it("accepts a size with byte units (REST k=v token path)", async () => {
      const q = await executeQuery(key, "check_disk_write", {
        file: path.join(scratch, "probe-units.dat"),
        size: "4k",
        "detail-syntax": "%(size)",
        "top-syntax": "${list}",
      });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toBe("4096");
    });

    it("rejects a size above the 1M maximum", async () => {
      const q = await executeQuery(key, "check_disk_write", {
        file: path.join(scratch, "too-big.dat"),
        size: "2M",
      });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toMatch(/Size too large/);
    });

    it("refuses to overwrite an existing file and goes CRITICAL", async () => {
      const existing = path.join(scratch, "precious.dat");
      fs.writeFileSync(existing, "do not touch");
      const q = await executeQuery(key, "check_disk_write", { file: existing });
      expect(q.result).toBe(CRITICAL);
      expect(messageOf(q)).toMatch(/already exists/);
      // The pre-existing file is left intact.
      expect(fs.readFileSync(existing, "utf8")).toBe("do not touch");
    });

    it("goes CRITICAL when the target cannot be created", async () => {
      const q = await executeQuery(key, "check_disk_write", {
        file: path.join(scratch, "no", "such", "dir", "probe.dat"),
      });
      expect(q.result).toBe(CRITICAL);
      expect(messageOf(q)).toMatch(/failed to create file/);
    });

    it("rejects a missing file argument with a clear message", async () => {
      const q = await executeQuery(key, "check_disk_write", {});
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toMatch(/No file specified/);
    });
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

  it("check_shadowcopy accepts the copies keyword and its deprecated count alias (Windows)", async () => {
    if (!onWindows) return;
    // `count` was renamed to `copies` (the old name clashes with the generic
    // `count` summary keyword). Both must parse in threshold expressions — an
    // unknown keyword would make the whole check UNKNOWN. No volume has that
    // many copies and the empty state is OK, so the result is deterministic.
    const q = await executeQuery(key, "check_shadowcopy", {
      warning: "copies > 999999",
      critical: "count > 999999",
      "empty-state": "ok",
    });
    expect(q.result).toBe(OK);
  });

  // --- check_share (§3.6, Windows) ------------------------------------------

  it("check_share lists shares without failing (Windows)", async () => {
    if (!onWindows) return; // SMB shares are a Windows feature.
    // List mode: a host may have any number of shares (admin shares like IPC$
    // are usually present); either way it must return a valid status, not error.
    const q = await executeQuery(key, "check_share", {});
    expect(q.result).not.toBe(UNKNOWN);
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
  });

  it("check_share goes CRITICAL when a required share is missing (Windows)", async () => {
    if (!onWindows) return;
    // A share name that cannot exist -> exists=0 -> default crit=not exists.
    const q = await executeQuery(key, "check_share", { share: "NSCP_NoSuchShare_zzz" });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/NSCP_NoSuchShare_zzz/);
    expect(messageOf(q)).toMatch(/exists=0/);
  });

  it("check_share confirms an always-present admin share exists (Windows)", async () => {
    if (!onWindows) return;
    // IPC$ is published by the Server service on essentially every Windows host.
    // If this host has it, the required-share check passes; if the Server service
    // is somehow absent it reports missing — accept either, but never UNKNOWN.
    const q = await executeQuery(key, "check_share", { share: "IPC$" });
    expect(q.result).not.toBe(UNKNOWN);
    expect([OK, CRITICAL]).toContain(q.result);
  });

  it("check_share with a missing share but no crit stays OK (Windows)", async () => {
    if (!onWindows) return;
    // Disabling the default crit proves the option parses and exists=0 alone
    // does not fail the check.
    const q = await executeQuery(key, "check_share", { share: "NSCP_NoSuchShare_zzz", critical: "none" });
    expect(q.result).toBe(OK);
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

  // --- empty result sets ------------------------------------------------------

  // A filter that matches nothing must land on each check's documented empty
  // contract rather than an error (#1499 was check_service dying on exactly
  // this path). The disk checks disagree on what an empty set means, so each
  // case pins its own default.

  it("check_drivesize with a filter that matches nothing returns UNKNOWN", async () => {
    const q = await executeQuery(key, "check_drivesize", {
      filter: "name = 'nosuchdrive-1499'",
    });
    expect(q.result).toBe(UNKNOWN);
    expect(messageOf(q)).toMatch(/No drives found/i);
  });

  it("check_disk_io with a filter that matches nothing takes the empty state", async () => {
    // The default empty-state is critical: a device list that comes back empty
    // is an alert, not a quiet OK. The option still relaxes it.
    const args = { filter: "name = 'nosuchdevice-1499'" };
    const q = await executeQuery(key, "check_disk_io", args);
    expect(q.result).toBe(CRITICAL);
    const relaxed = await executeQuery(key, "check_disk_io", { ...args, "empty-state": "ok" });
    expect(relaxed.result).toBe(OK);
  });

  it("check_disk_health with a filter that matches nothing takes the empty state", async () => {
    const args = { filter: "name = 'nosuchdevice-1499'" };
    const q = await executeQuery(key, "check_disk_health", args);
    expect(q.result).toBe(CRITICAL);
    const relaxed = await executeQuery(key, "check_disk_health", { ...args, "empty-state": "ok" });
    expect(relaxed.result).toBe(OK);
  });

  it("check_files with a filter that matches nothing returns UNKNOWN", async () => {
    // The directory exists and has a file, so this is the filter (not a bad
    // path) producing the empty set.
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-empty-"));
    fs.writeFileSync(path.join(dir, "present.txt"), "x");
    try {
      const q = await executeQuery(key, "check_files", {
        path: dir,
        filter: "filename = 'nosuchfile-1499'",
      });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toMatch(/No files found/i);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  it("check_share with a filter that matches nothing is OK (Windows)", async () => {
    if (!onWindows) return; // SMB shares are a Windows feature.
    // The default crit is `not exists`, an object-bound keyword: with no share
    // in the context it must not fire.
    const q = await executeQuery(key, "check_share", { filter: "name = 'nosuchshare-1499'" });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/No shares found/i);
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

  // --- check_files / check_single_file ---------------------------------------
  //
  // Ported from the legacy scripts/python/test_w32_file.py (an `nscp unit`
  // script run by acceptance-tests.bat), and run on both platforms: the
  // fixture-tree matrix (size, pattern, max-depth, written), the regressions
  // #730 (max-depth=0 scans the top directory only), #613 (a missing path is
  // UNKNOWN, alone or among valid ones), #717 (the legacy CheckFiles shim
  // treats no matches as OK) and #598 (paths outside the active code page are
  // readable), and the path edge cases. Over REST the paths arrive as UTF-8,
  // which is how #598 reaches the agent from a remote caller.
  describe("check_files and check_single_file", () => {
    /**
     * The legacy fixture: name, size in bytes, mtime offset from now in
     * minutes, and the folder it sits in. Three nested folders (001, 001/002,
     * 001/002/003), ten files of which nine are *.txt; the files in the
     * folders sit at depth 3, 2 and 1.
     */
    const FIXTURE: Array<[name: string, size: number, offsetMin: number, folder: string]> = [
      ["test.001", 4, -5, ""],
      ["test-001.txt", 4, -5, ""],
      ["test-002.txt", 12, -5, ""],
      ["test-003.txt", 32, -10, ""],
      ["test-004.txt", 4, -10, ""],
      ["test-005.txt", 4, 0, ""],
      ["test-006.txt", 4, 5, ""],
      ["test-007.txt", 4, 5, "001/002/003"],
      ["test-008.txt", 4, 5, "001/002"],
      ["test-009.txt", 4, 5, "001"],
    ];

    let work: string;
    let tree: string;

    /** A fresh, empty folder for one case. */
    function scratch(name: string): string {
      const dir = path.join(work, name);
      fs.rmSync(dir, { recursive: true, force: true });
      fs.mkdirSync(dir, { recursive: true });
      return dir;
    }

    /**
     * Re-stamp the fixture's mtimes relative to now. The matrix runs this
     * right before every query, so the offsets hold however long the earlier
     * cases took.
     */
    function stampTree(): void {
      const now = Date.now();
      for (const [name, , offset, folder] of FIXTURE) {
        const t = new Date(now + offset * 60_000);
        fs.utimesSync(path.join(tree, folder, name), t, t);
      }
    }

    beforeAll(() => {
      work = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-files-"));
      tree = path.join(work, "tree");
      for (const [name, size, , folder] of FIXTURE) {
        const dir = path.join(tree, folder);
        fs.mkdirSync(dir, { recursive: true });
        let body = "";
        for (let i = 0; i < size; i++) body += String(i % 10);
        fs.writeFileSync(path.join(dir, name), body);
      }
    });

    afterAll(() => {
      if (work) fs.rmSync(work, { recursive: true, force: true });
    });

    /** The status the matrix thresholds give a count: warning > 1, critical > 3, empty unknown. */
    function statusForCount(count: number): number {
      if (count > 3) return CRITICAL;
      if (count > 1) return WARNING;
      if (count > 0) return OK;
      return UNKNOWN;
    }

    /** check_files over the fixture tree with the legacy count thresholds. */
    function matrix(filter: string, extra: Record<string, string> = {}) {
      stampTree();
      return executeQuery(key, "check_files", {
        path: tree,
        filter,
        "detail-syntax": "%(filename): %(size) %(written)",
        "top-syntax": "count=${count}",
        warning: "count > 1",
        critical: "count > 3",
        "empty-state": "unknown",
        ...extra,
      });
    }

    // [filter, extra arguments, count on Windows, count on Linux]
    //
    // The two differ only where folders would be counted. On Windows a folder
    // is a match of its own, with size 0; Linux descends into folders without
    // reporting them, so there it counts the ten files alone.
    const cases: Array<[string, Record<string, string>, number, number]> = [
      ["size gt 0b", {}, 10, 10],
      ["size gt 4b", {}, 2, 2],
      ["size eq 4b", {}, 8, 8],
      ["size lt 5b", {}, 11, 8], // + the three folders on Windows
      ["size ne 4b", {}, 5, 2], // + the three folders on Windows
      ["size lt 4m", {}, 13, 10], // + the three folders on Windows
      ["size eq 0b", {}, 3, 0], // the folders alone
      // max-depth n reaches n levels: the folders on Windows...
      ["size eq 0b", { "max-depth": "1" }, 1, 0],
      ["size eq 0b", { "max-depth": "2" }, 2, 0],
      ["size eq 0b", { "max-depth": "3" }, 3, 0],
      ["size eq 0b", { "max-depth": "4" }, 3, 0],
      // ...and the files under them, which reads the same on both.
      ["size gt 0b", { "max-depth": "1" }, 7, 7],
      ["size gt 0b", { "max-depth": "2" }, 8, 8],
      ["size gt 0b", { "max-depth": "3" }, 9, 9],
      ["size gt 0b", { "max-depth": "4" }, 10, 10],
      ["size gt 0b", { pattern: "*.txt" }, 9, 9],
      ["size gt 0b", { pattern: "*.foo" }, 0, 0],
      // Written dates. `ge -6m` rather than `ge -5m`: elapsed time moves the
      // threshold forward but not the files, so a bound sitting exactly on a
      // fixture offset drops those files when stamping and query straddle a
      // second boundary. Every other bound has at least a minute of margin.
      ["written ge -6m", { pattern: "*.txt" }, 7, 7],
      ["written le -5m", { pattern: "*.txt" }, 4, 4],
      ["written lt -9m", { pattern: "*.txt" }, 2, 2],
      ["written gt -9m", { pattern: "*.txt" }, 7, 7],
      ["written lt -1m", { pattern: "*.txt" }, 4, 4],
      ["written gt -9m and written lt -1m", { pattern: "*.txt" }, 2, 2],
      ["written gt 0m", { pattern: "*.txt" }, 4, 4],
    ];

    it.each(cases)("filter %s %j counts %i on Windows, %i on Linux", async (filter, extra, onWin, onLinux) => {
      const expected = onWindows ? onWin : onLinux;
      const q = await matrix(filter, extra);
      if (expected > 0) expect(messageOf(q)).toContain(`count=${expected}`);
      expect(q.result).toBe(statusForCount(expected));
    });

    it("reports the count as its only perfdata", async () => {
      const q = await matrix("size gt 4b");
      expect(Object.values(perfOf(q)).map((e) => e.value)).toEqual([2]);
    });

    // --- regressions --------------------------------------------------------

    it("#730: max-depth=0 scans the top directory only", async () => {
      const root = scratch("depth0");
      fs.writeFileSync(path.join(root, "top1.txt"), "hello");
      fs.writeFileSync(path.join(root, "top2.txt"), "hello");
      fs.mkdirSync(path.join(root, "sub"));
      fs.writeFileSync(path.join(root, "sub", "nested.txt"), "hello");

      const q = await executeQuery(key, "check_files", {
        path: root,
        "max-depth": "0",
        filter: "type='file'",
        warning: "count > 5",
        critical: "count > 10",
        "top-syntax": "%(count) files",
      });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("2 files");
      expect(messageOf(q)).not.toContain("nested.txt");
    });

    it("#613: a missing path is UNKNOWN and named", async () => {
      const q = await executeQuery(key, "check_files", {
        path: path.join(work, "does_not_exist_47b1f0e5"),
        filter: "type='file'",
        warning: "count > 5",
        critical: "count > 10",
      });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toContain("Path was not found");
    });

    it("#613: a missing file under an existing folder is UNKNOWN", async () => {
      const q = await executeQuery(key, "check_files", { path: path.join(tree, "aaa.txt") });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toContain("Path was not found");
    });

    it("#613: a missing path among valid ones is still UNKNOWN", async () => {
      const good = scratch("mixed_paths");
      fs.writeFileSync(path.join(good, "a.txt"), "hello");
      const q = await executeQuery(key, "check_files", {
        path: [good, path.join(work, "does_not_exist_47b1f0e5")],
        filter: "type='file'",
        warning: "count > 0",
        critical: "count > 100000",
      });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toContain("Path was not found");
    });

    it("#717: the legacy CheckFiles shim treats no matches as OK (Windows)", async () => {
      if (!onWindows) return; // The legacy command is not implemented elsewhere.
      const empty = scratch("empty_legacy");
      const q = await executeQuery(key, "CheckFiles", {
        path: empty,
        pattern: "*.log",
        MaxWarn: "10",
        MaxCrit: "20",
      });
      expect(q.result).toBe(OK);
    });

    it("#598: check_files reads a path outside the active code page", async () => {
      const root = scratch("日本語_files");
      fs.writeFileSync(path.join(root, "file.txt"), "data");
      const q = await executeQuery(key, "check_files", {
        path: root,
        filter: "type='file'",
        warning: "count > 5",
        critical: "count > 10",
        "top-syntax": "%(count) files",
      });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("1 files");
    });

    it("#598: check_single_file reads a file name outside the active code page", async () => {
      const file = path.join(scratch("日本語_single"), "日本語.txt");
      fs.writeFileSync(file, "data");
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 1M" });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).not.toContain("File not found");
    });

    // --- check_files edge cases ---------------------------------------------

    /** check_files over `args` with thresholds that cannot fire and a count in the message. */
    function countFiles(args: Record<string, string | string[]>) {
      return executeQuery(key, "check_files", {
        filter: "type='file'",
        warning: "count > 100",
        critical: "count > 200",
        "top-syntax": "%(count) files",
        ...args,
      });
    }

    it("empty-state=ok turns an empty folder OK", async () => {
      const empty = scratch("empty_modern");
      const q = await executeQuery(key, "check_files", { path: empty, pattern: "*.log", "empty-state": "ok" });
      expect(q.result).toBe(OK);
    });

    it("paths=A,B scans both folders", async () => {
      const a = scratch("paths_a");
      const b = scratch("paths_b");
      fs.writeFileSync(path.join(a, "one.txt"), "hello");
      fs.writeFileSync(path.join(b, "two.txt"), "hello");
      fs.writeFileSync(path.join(b, "three.txt"), "hello");
      const q = await countFiles({ paths: `${a},${b}` });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("3 files");
    });

    it("file= is an alias for path=", async () => {
      const root = scratch("file_alias");
      fs.writeFileSync(path.join(root, "a.txt"), "hello");
      const q = await countFiles({ file: root });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("1 files");
    });

    it("a file given as path= counts as one match", async () => {
      const file = path.join(scratch("path_is_file"), "lonely.txt");
      fs.writeFileSync(file, "hello");
      const q = await countFiles({ path: file });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("1 files");
    });

    it("a malformed filter is UNKNOWN", async () => {
      const root = scratch("bad_filter");
      fs.writeFileSync(path.join(root, "a.txt"), "hello");
      const q = await executeQuery(key, "check_files", {
        path: root,
        filter: "size >>> 5b",
        warning: "count > 100",
        critical: "count > 200",
      });
      expect(q.result).toBe(UNKNOWN);
    });

    it("tolerates a trailing separator", async () => {
      const root = scratch("trailing_sep");
      fs.writeFileSync(path.join(root, "a.txt"), "hello");
      fs.writeFileSync(path.join(root, "b.txt"), "hello");
      const q = await countFiles({ path: `${root}${path.sep}` });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("2 files");
    });

    it("tolerates forward slashes (Windows)", async () => {
      if (!onWindows) return; // They are the only separator elsewhere.
      const root = scratch("forward_slashes");
      fs.writeFileSync(path.join(root, "a.txt"), "hello");
      fs.writeFileSync(path.join(root, "b.txt"), "hello");
      const q = await countFiles({ path: root.replace(/\\/g, "/") });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("2 files");
    });

    it("show-all keeps an all-OK scan OK", async () => {
      const root = scratch("show_all");
      for (const n of ["a.txt", "b.txt", "c.txt"]) fs.writeFileSync(path.join(root, n), "hello");
      const q = await executeQuery(key, "check_files", {
        path: root,
        filter: "type='file'",
        warning: "count > 100",
        critical: "count > 200",
        "show-all": "true",
      });
      expect(q.result).toBe(OK);
    });

    it("a glob in path= is a literal path, so it is UNKNOWN", async () => {
      const root = scratch("glob_in_path");
      fs.writeFileSync(path.join(root, "a.log"), "hello");
      fs.writeFileSync(path.join(root, "b.log"), "hello");
      const q = await executeQuery(key, "check_files", {
        path: path.join(root, "*.log"),
        filter: "type='file'",
        warning: "count > 100",
        critical: "count > 200",
      });
      expect(q.result).toBe(UNKNOWN);
    });

    // --- check_single_file --------------------------------------------------

    it("check_single_file: a file under the thresholds is OK and named", async () => {
      const file = path.join(scratch("single_ok"), "small.txt");
      fs.writeFileSync(file, "a few bytes of text");
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 1M", critical: "size > 10M" });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toContain("small.txt");
    });

    it("check_single_file: over the warning threshold is WARNING", async () => {
      const file = path.join(scratch("single_warn"), "big.txt");
      fs.writeFileSync(file, "x".repeat(200_000));
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 100k", critical: "size > 10M" });
      expect(q.result).toBe(WARNING);
      expect(messageOf(q)).toContain("big.txt");
    });

    it("check_single_file: over both thresholds is CRITICAL", async () => {
      const file = path.join(scratch("single_crit"), "huge.dat");
      fs.writeFileSync(file, "x".repeat(200_000));
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 50k", critical: "size > 100k" });
      expect(q.result).toBe(CRITICAL);
    });

    it("check_single_file: an age threshold fires on a two-day-old file", async () => {
      const file = path.join(scratch("single_age"), "old.txt");
      fs.writeFileSync(file, "old");
      const twoDaysAgo = new Date(Date.now() - 2 * 24 * 3600 * 1000);
      fs.utimesSync(file, twoDaysAgo, twoDaysAgo);
      const q = await executeQuery(key, "check_single_file", { file, critical: "written < -1d" });
      expect(q.result).toBe(CRITICAL);
    });

    it("check_single_file: an empty file is found, not missing", async () => {
      const file = path.join(scratch("single_empty"), "empty.dat");
      fs.writeFileSync(file, "");
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 1M", critical: "size > 10M" });
      expect(q.result).toBe(OK);
      expect(messageOf(q)).not.toContain("File not found");
    });

    it("check_single_file: a missing file is UNKNOWN", async () => {
      const file = path.join(work, "single_missing", "nope.txt");
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 1M" });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toContain("File not found");
    });

    it("check_single_file: no file argument is UNKNOWN", async () => {
      const q = await executeQuery(key, "check_single_file", { warning: "size > 1M" });
      expect(q.result).toBe(UNKNOWN);
      expect(messageOf(q)).toContain("No file specified");
    });

    it("check_single_file: a folder is UNKNOWN", async () => {
      const q = await executeQuery(key, "check_single_file", { file: scratch("single_dir"), warning: "size > 1M" });
      expect(q.result).toBe(UNKNOWN);
    });

    it("check_single_file: perfdata carries the size", async () => {
      const file = path.join(scratch("single_perf"), "mid.dat");
      fs.writeFileSync(file, "x".repeat(1024));
      const q = await executeQuery(key, "check_single_file", { file, warning: "size > 1M", critical: "size > 10M" });
      expect(q.result).toBe(OK);
      // The label is prefixed with the file name ('mid.dat size').
      expect(Object.keys(perfOf(q)).some((label) => /size/.test(label))).toBe(true);
    });
  });
});
