/**
 * Exercises the Unix CheckDisk module (modules/CheckDiskUnix, which builds as
 * the "CheckDisk" module on Linux) end-to-end against the real nscp binary.
 *
 * Each case runs a one-shot client query — `nscp client --module CheckDisk
 * --boot --query <cmd> ...` — which loads the module, runs the check, and
 * prints the Nagios-style result line. No server/port/docker needed.
 *
 * The module is Linux-only (the Windows build ships its own CheckDisk), so the
 * whole suite is skipped off Linux.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import { NscpInstance, describeOnLinux } from "@fixtures/index";

jest.setTimeout(120_000);

describeOnLinux("CheckDisk (Unix)", () => {
  let nscp: NscpInstance;
  let scratch: string;

  /** Run a CheckDisk query and return the combined output. */
  async function query(command: string, args: string[] = []): Promise<string> {
    return (await queryWithCode(command, args)).out;
  }

  /**
   * As query(), but also returns the process exit code, which is the Nagios
   * status (0 OK / 1 WARNING / 2 CRITICAL / 3 UNKNOWN). The client-query path
   * prints the raw message with no status word, so for checks whose syntax
   * does not embed %(status) this is the only way to assert the verdict.
   */
  async function queryWithCode(command: string, args: string[] = []): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", "--module", "CheckDisk", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  beforeAll(() => {
    nscp = new NscpInstance();
    scratch = fs.mkdtempSync(path.join(os.tmpdir(), "checkdisk-it-"));
    fs.writeFileSync(path.join(scratch, "small.log"), "one\ntwo\nthree\n"); // 14 bytes, 3 lines
    fs.writeFileSync(path.join(scratch, "big.log"), "x".repeat(5000)); // 5000 bytes, 1 line
    fs.writeFileSync(path.join(scratch, "note.txt"), "ignore me\n");
  });

  afterAll(() => {
    fs.rmSync(scratch, { recursive: true, force: true });
  });

  // --- check_drivesize -----------------------------------------------------

  it("reports OK and perfdata for the root filesystem", async () => {
    const out = await query("check_drivesize", ["drive=/", "warning=used>99%", "critical=used>99%"]);
    expect(out).toMatch(/OK.*drive/i);
    // Carbon-free perfdata: "'/ used'=<value> ..." and a percentage metric.
    expect(out).toMatch(/'\/ used'=/);
    expect(out).toMatch(/'\/ used %'=\d+%/);
  });

  it("honours warn/crit thresholds", async () => {
    // Anything is more than 0% used, so this must trip CRITICAL.
    const out = await query("check_drivesize", ["drive=/", "critical=used>0"]);
    expect(out).toMatch(/^CRITICAL/m);
  });

  it("reports the filesystem type and a type filter works", async () => {
    const out = await query("check_drivesize", [
      "drive=/",
      "detail-syntax=%(drive) fs=%(fs) type=%(type)",
      "top-syntax=${list}",
      "filter=type = 'fixed'",
    ]);
    // The root fs is a real disk → classified "fixed", and its fs type shown.
    expect(out).toMatch(/\/ fs=\S+ type=fixed/);
  });

  it("rejects a non-existent drive", async () => {
    const out = await query("check_drivesize", ["drive=/no/such/mount/point"]);
    expect(out).toMatch(/not be found|not found|was not found/i);
  });

  it("ignore-missing turns a non-existent drive into OK", async () => {
    const out = await query("check_drivesize", ["drive=/no/such/mount/point", "ignore-missing=true"]);
    expect(out).not.toMatch(/not found/i);
    expect(out).toMatch(/^OK/m);
  });

  it("ignore-missing still checks the drives that do exist", async () => {
    // The missing one is dropped; the real one is checked normally, so this
    // must not silently degrade into "nothing was checked".
    const out = await query("check_drivesize", [
      "drive=/",
      "drive=/no/such/mount/point",
      "ignore-missing=true",
      "warning=used>99%",
      "critical=used>99%",
    ]);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/'\/ used'=/);
  });

  it("ignore-missing does not override an explicit empty-state", async () => {
    // ignore-missing implies empty-state=ok, but only as a default: asking for
    // something else must still win.
    const out = await query("check_drivesize", [
      "drive=/no/such/mount/point",
      "ignore-missing=true",
      "empty-state=warning",
    ]);
    expect(out).toMatch(/WARNING/);
  });

  it("require= is Windows-only, so its interaction with ignore-missing is not reachable here", async () => {
    // On Windows require= asserts a drive IS present and stays CRITICAL when
    // it is not, even under ignore-missing. Linux has no such option at all,
    // which is pinned here so this is revisited if it is ever added.
    const out = await query("check_drivesize", ["drive=/", "ignore-missing=true", "require=/no/such/mount/point"]);
    expect(out).toMatch(/unrecognised option 'require/i);
  });

  // Regression: `total` is a boolean option passed as the token `total=true`
  // (same as REST). It must be po::value<bool>()->implicit_value(true), not
  // po::bool_switch, which rejects a value with "option '--total' does not take
  // any arguments". See docs/design/icinga-windows-parity.md §4.2.
  it("accepts total=true and emits a total aggregate row", async () => {
    const out = await query("check_drivesize", ["drive=/", "warning=used>99%", "critical=used>99%", "total=true"]);
    expect(out).not.toMatch(/does not take any arguments/);
    expect(out).toMatch(/^OK/m);
    expect(out).toMatch(/'total used'=/);
  });

  // --- check_drivesize trend keywords (full_in / rate) ----------------------
  //
  // In the one-shot client-query path the collector has (at most) a single
  // fresh sample by the time the check runs — deterministically NOT a valid
  // trend (that needs >= 3 samples spanning >= 3x the sampling interval).
  // These cases pin the documented no-data contract: never/unknown/0-span,
  // thresholds that cannot fire, and no perfdata poisoning.

  it("trend keywords report the no-data contract before a trend exists", async () => {
    const out = await query("check_drivesize", [
      "drive=/",
      "detail-syntax=%(drive)|full_in=%(full_in)|rate=%(rate)|span=%(trend_span)|samples=%(trend_samples)",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/\/\|full_in=never\|rate=unknown\|span=0\|samples=[01]\b/);
  });

  it("a full_in duration threshold parses over the k=v token path and cannot fire on no data", async () => {
    const r = await queryWithCode("check_drivesize", [
      "drive=/",
      "warning=full_in < 12h",
      "critical=full_in < 1h",
    ]);
    expect(r.out).not.toMatch(/does not take any arguments|unrecognised|Invalid syntax/i);
    expect(r.code).toBe(0); // a missing value satisfies no numeric predicate
  });

  it("a missing full_in emits no perfdata even when thresholded", async () => {
    const out = await query("check_drivesize", ["drive=/", "warning=full_in < 12h"]);
    expect(out).not.toMatch(/full_in'?=/);
  });

  it("full_in = 'never' is the presence test for a missing trend", async () => {
    // No trend -> the presence test matches the drive (count=1, rendering the
    // all-ok syntax); had it not matched, empty-state=critical would have
    // produced "CRITICAL: No drives found" instead.
    const r = await queryWithCode("check_drivesize", [
      "drive=/",
      "filter=full_in = 'never'",
      "empty-state=critical",
    ]);
    expect(r.out).toMatch(/All 1 drive\(s\) are ok/);
    expect(r.code).toBe(0);
  });

  it("rejects an invalid trend-window", async () => {
    const r = await queryWithCode("check_drivesize", ["drive=/", "trend-window=bogus"]);
    expect(r.out).toMatch(/Invalid trend-window/i);
    expect(r.code).toBe(3);
  });

  it("accepts a trend-window duration", async () => {
    const r = await queryWithCode("check_drivesize", [
      "drive=/",
      "trend-window=6h",
      "warning=used>99%",
      "critical=used>99%",
    ]);
    expect(r.code).toBe(0);
  });

  // --- check_files ---------------------------------------------------------

  it("scans a directory and exposes size / line_count / age", async () => {
    const out = await query("check_files", [
      `path=${scratch}`,
      "pattern=*.log",
      "detail-syntax=%(filename)|size=%(size)|lines=%(line_count)|age=%(age)",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/small\.log\|size=14\|lines=3\|age=\d+/);
    // line_count counts newline terminators (shared cross-platform semantics);
    // big.log has 5000 chars but no newline, so 0 lines.
    expect(out).toMatch(/big\.log\|size=5000\|lines=0\|age=\d+/);
    // note.txt does not match *.log and must be absent.
    expect(out).not.toMatch(/note\.txt/);
  });

  it("applies a size filter across matched files", async () => {
    const out = await query("check_files", [
      `path=${scratch}`,
      "pattern=*.log",
      "filter=size > 1k",
      "detail-syntax=%(filename)",
      "top-syntax=${count}:${list}",
    ]);
    // Only big.log (5000 bytes) exceeds 1k.
    expect(out).toMatch(/1:big\.log/);
  });

  // --- list-separator (issue #1370) ----------------------------------------
  //
  // A common option of every modern_filter check, exercised here because
  // *.log matches exactly two files, so the joined list is deterministic.

  it("joins list items with ', ' by default", async () => {
    const out = await query("check_files", [`path=${scratch}`, "pattern=*.log", "detail-syntax=%(filename)", "top-syntax=${list}"]);
    // Directory order is not guaranteed, so accept either order.
    expect(out).toMatch(/(small\.log, big\.log|big\.log, small\.log)/);
  });

  it("joins list items with a custom separator", async () => {
    const out = await query("check_files", [
      `path=${scratch}`,
      "pattern=*.log",
      "detail-syntax=%(filename)",
      "top-syntax=${list}",
      "list-separator= | ",
    ]);
    expect(out).toMatch(/\.log \| \S+\.log/);
  });

  it("renders one item per line via list-separator=\\n and %(sep) in the top-syntax", async () => {
    const out = await query("check_files", [
      `path=${scratch}`,
      "pattern=*.log",
      "detail-syntax=%(filename)",
      // Both halves are needed: %(sep) (the decoded separator) breaks before
      // the first item, the separator itself breaks between the rest.
      "top-syntax=%(count) file(s):%(sep)%(list)",
      "list-separator=\\n",
    ]);
    expect(out).toMatch(/^2 file\(s\):$/m);
    expect(out).toMatch(/^small\.log$/m);
    expect(out).toMatch(/^big\.log$/m);
    // And no item is left glued to another.
    expect(out).not.toMatch(/\.log.*\.log/);
  });

  it("never escape-decodes the templates themselves", async () => {
    // Existing configurations contain literal backslashes (Windows paths such
    // as C:\temp\new, regexes); decoding \t or \n inside a template would
    // silently corrupt them on upgrade. Only list-separator is decoded.
    // %(list) keeps the top-syntax in play (without a list token the OK path
    // renders the ok-syntax instead).
    const out = await query("check_files", [
      `path=${scratch}`,
      "pattern=*.log",
      "detail-syntax=%(filename)",
      "top-syntax=backup to C:\\temp\\new: %(count) file(s): %(list)",
    ]);
    expect(out).toContain("backup to C:\\temp\\new: 2 file(s):");
    expect(out).not.toMatch(/\t/);
  });

  // --- valued booleans over the k=v token path ------------------------------
  //
  // REST (and this client-query path) pass each flag as the single token
  // `x=true`; bool_switch rejects that with "does not take any arguments" and
  // dumps the usage text instead of running the check.

  it("accepts show-all=true and escape-html=true as valued booleans", async () => {
    const out = await query("check_files", [
      `path=${scratch}`,
      "pattern=*.log",
      "detail-syntax=<%(filename)>",
      "show-all=true",
      "escape-html=true",
    ]);
    expect(out).not.toMatch(/does not take any arguments/i);
    // show-all renders the detail list; escape-html turns its <> into entities.
    expect(out).toContain("&lt;small.log&gt;");
    expect(out).not.toContain("<small.log>");
  });

  it("check_files fails on a missing path by default", async () => {
    const out = await query("check_files", [`path=${path.join(scratch, "no-such-dir")}`]);
    expect(out).toMatch(/Path was not found/i);
  });

  it("check_files ignore-missing turns a missing path into OK", async () => {
    const missing = path.join(scratch, "no-such-dir");
    const bad = await queryWithCode("check_files", [`path=${missing}`]);
    expect(bad.code).toBe(3); // UNKNOWN

    const ok = await queryWithCode("check_files", [`path=${missing}`, "ignore-missing=true"]);
    expect(ok.code).toBe(0); // OK
    expect(ok.out).not.toMatch(/Path was not found/i);
  });

  it("check_files ignore-missing still scans the paths that do exist", async () => {
    const out = await query("check_files", [
      `path=${scratch}`,
      `path=${path.join(scratch, "no-such-dir")}`,
      "ignore-missing=true",
      "pattern=*.log",
      "top-syntax=%(status): %(count) files",
    ]);
    // small.log + big.log are there; the missing directory contributes nothing
    // rather than wiping out the result.
    expect(out).toMatch(/All 2 files are ok/);
  });

  it("check_files ignore-missing does not log the skipped path as an error", async () => {
    // An intentionally-absent path is an expected condition, so it must not
    // show up as an ERROR line in the log.
    const out = await query("check_files", [
      `path=${path.join(scratch, "no-such-dir")}`,
      "ignore-missing=true",
    ]);
    expect(out).not.toMatch(/Invalid file specified/i);
  });

  // --- -w / -c short thresholds ----------------------------------------------
  //
  // The client-query path hands the tokens over as they are typed, so these
  // are the real "-w 80 -c 90" a plugin user writes, mixed with key=value.

  it("check_drivesize -w/-c bound free space as a percentage", async () => {
    // No drive has less than 0% free, every drive has less than 101% free.
    const ok = await queryWithCode("check_drivesize", ["drive=/", "-w", "0", "-c", "0"]);
    expect(ok.code).toBe(0);
    const warn = await queryWithCode("check_drivesize", ["drive=/", "-w", "101", "-c", "0"]);
    expect(warn.code).toBe(1);
    const crit = await queryWithCode("check_drivesize", ["drive=/", "-w", "0", "-c", "101%"]);
    expect(crit.code).toBe(2);
  });

  it("check_drivesize accepts the short thresholds in front and attached", async () => {
    const crit = await queryWithCode("check_drivesize", ["-w0", "-c=101", "drive=/"]);
    expect(crit.code).toBe(2);
    // A range says its side itself: "~:0" is "more than 0% free".
    const range = await queryWithCode("check_drivesize", ["-c", "~:0", "drive=/"]);
    expect(range.code).toBe(2);
  });

  it("check_drivesize -c only replaces the critical default", async () => {
    // Read off the thresholds of the performance data, which are those of the
    // expressions that ran: the default warning (used > 80%) is still there,
    // and the critical is the "free < 0%" that -c asked for instead of the
    // default on used. (show-default would not do: it is answered before -c is
    // applied, and prints the same line whatever -c says.)
    const r = await queryWithCode("check_drivesize", ["drive=/", "-c", "0"]);
    expect(r.out).toMatch(/'\/ used %'=\d+%;80;;/);
    expect(r.out).toMatch(/'\/ free %'=\d+%;;0;/);
    // And the other way around: -w leaves the default critical (used > 90%).
    const w = await queryWithCode("check_drivesize", ["drive=/", "-w", "0"]);
    expect(w.out).toMatch(/'\/ used %'=\d+%;;90;/);
    expect(w.out).toMatch(/'\/ free %'=\d+%;0;;/);
  });

  it("check_drivesize refuses a reversed range", async () => {
    // "20:10" would be "free < 20% or free > 10%": always true.
    const r = await queryWithCode("check_drivesize", ["drive=/", "-w", "20:10"]);
    expect(r.code).toBe(3);
    expect(r.out).toMatch(/Invalid threshold: 20:10/);
  });

  it("a -w without a value says so instead of taking the next option", async () => {
    for (const args of [
      ["drive=/", "-w", "-c", "0"],
      ["--drive", "/", "-w", "-c", "0"],
      ["drive=/", "-c", "0", "-w"],
      // An unset macro, "-w $ARG1$ drive=/": the drive is not the threshold.
      ["-w", "drive=/"],
      ["-w", "drive=/", "-c", "0"],
    ]) {
      const r = await queryWithCode("check_drivesize", args);
      expect(r.code).toBe(3);
      expect(r.out).toMatch(/Missing value for -w \(use none to remove the threshold\)/);
    }
  });

  it("an attached negative short threshold is read as one", async () => {
    const r = await queryWithCode("check_drivesize", ["drive=/", "-w0", "-c-5:"]);
    expect(r.out).not.toMatch(/unrecognised|unknown option|Invalid command line|Failed to parse/i);
    expect(r.code).toBe(0);
  });

  it("check_drivesize -w takes an expression as it is", async () => {
    const r = await queryWithCode("check_drivesize", ["drive=/", "-w", "used >= 0", "-c", "0"]);
    expect(r.code).toBe(1);
  });

  it("a negative short threshold survives next to dashed arguments", async () => {
    // "--drive / -c -5:" must not have "-5:" read as an option of its own.
    const r = await queryWithCode("check_drivesize", ["--drive", "/", "-w", "0", "-c", "-5:"]);
    expect(r.out).not.toMatch(/unrecognised|unknown option|Invalid command line/i);
    expect(r.code).toBe(0);
  });

  it("show-default stays a line that can be edited and passed back", async () => {
    // With "w=20" in it, editing only warning= would leave the short form
    // adding the old bound back; the resolved query is show-query's job.
    const long = await query("check_drivesize", ["show-default"]);
    expect(long).toMatch(/"warning=used > 80%"/);
    expect(long).toMatch(/"critical=used > 90%"/);
    expect(long).not.toMatch(/"w=/);
    expect(long).not.toMatch(/"c=/);
    expect(long).not.toMatch(/show-query/);
  });

  it("show-query prints the query that runs, -w and -c resolved", async () => {
    const r = await queryWithCode("check_drivesize", ["drive=/", "-w", "25", "-c", "10", "show-query"]);
    expect(r.code).toBe(0);
    // One token per expression; -w / -c are folded into warning / critical.
    expect(r.out).toMatch(/"warning=free < 25%" /);
    expect(r.out).toMatch(/"critical=free < 10%" /);
    expect(r.out).not.toMatch(/"w=|"c=|"show-query=|"warn=|"crit=/);
    // The check's own options are part of the line, so it can be pasted back.
    expect(r.out).toMatch(/"drive=\/" /);
    expect(r.out).toMatch(/"top-syntax=[^"]+" /);
    expect(r.out).toMatch(/"detail-syntax=[^"]+" /);
    // Nothing was checked: no perfdata, no status.
    expect(r.out).not.toMatch(/'\/ used'=/);
  });

  it("show-query with no other arguments is the defaults, resolved", async () => {
    const r = await queryWithCode("check_drivesize", ["show-query"]);
    expect(r.code).toBe(0);
    expect(r.out).toMatch(/"warning=used > 80%" "critical=used > 90%" /);
  });

  it("show-query takes the valued form REST passes", async () => {
    const on = await queryWithCode("check_drivesize", ["drive=/", "show-query=true"]);
    expect(on.code).toBe(0);
    expect(on.out).toMatch(/"warning=used > 80%"/);
    // Switched off it is an ordinary run.
    const off = await queryWithCode("check_drivesize", ["drive=/", "show-query=false", "warning=used > 101%", "critical=used > 101%"]);
    expect(off.code).toBe(0);
    expect(off.out).toMatch(/'\/ used'=/);
    expect(off.out).not.toMatch(/"warning=/);
  });

  it("show-query still refuses an invalid expression", async () => {
    const r = await queryWithCode("check_drivesize", ["drive=/", "warning=no_such_keyword > 1", "show-query"]);
    expect(r.code).toBe(3);
    expect(r.out).not.toMatch(/"warning=/);
  });

  it("show-query is there on a check that bounds count", async () => {
    const r = await queryWithCode("check_files", [`path=${scratch}`, "-c", "5", "show-query"]);
    expect(r.code).toBe(0);
    expect(r.out).toMatch(/"critical=count > 5" /);
    expect(r.out).toContain(`"path=${scratch}" `);
  });

  it("an empty or mis-united short threshold is refused, not guessed at", async () => {
    const empty = await queryWithCode("check_drivesize", ["drive=/", "w="]);
    expect(empty.code).toBe(3);
    expect(empty.out).toMatch(/Missing value for -w/);
    const unit = await queryWithCode("check_drivesize", ["drive=/", "-w", "10ms"]);
    expect(unit.code).toBe(3);
    expect(unit.out).toMatch(/Invalid unit for free: 10ms/);
  });

  it("check_single_file -w/-c bound the age in seconds, with units", async () => {
    const file = `file=${path.join(scratch, "big.log")}`;
    // Written moments ago: not older than an hour, but inside 0s..1h.
    const ok = await queryWithCode("check_single_file", [file, "-w", "30m", "-c", "1h"]);
    expect(ok.code).toBe(0);
    const warn = await queryWithCode("check_single_file", [file, "-w", "@0:1h", "-c", "1h"]);
    expect(warn.code).toBe(1);
    const bad = await queryWithCode("check_single_file", [file, "-w", "5x"]);
    expect(bad.code).toBe(3);
    expect(bad.out).toMatch(/Invalid duration for age: 5x/);
  });

  it("check_files -w/-c bound the number of matched files", async () => {
    // Three files in the scratch folder.
    const dir = `path=${scratch}`;
    const ok = await queryWithCode("check_files", [dir, "-w", "3", "-c", "5"]);
    expect(ok.code).toBe(0);
    const warn = await queryWithCode("check_files", [dir, "-w", "2", "-c", "5"]);
    expect(warn.code).toBe(1);
    // "5:" is "fewer than 5".
    const crit = await queryWithCode("check_files", [dir, "-w", "2", "-c", "5:"]);
    expect(crit.code).toBe(2);
  });

  // --- check_single_file ---------------------------------------------------

  it("inspects a single file", async () => {
    const out = await query("check_single_file", [`file=${path.join(scratch, "big.log")}`]);
    expect(out).toMatch(/OK/);
    expect(out).toMatch(/size=5000/);
  });

  it("fails clearly on a missing single file", async () => {
    const out = await query("check_single_file", [`file=${path.join(scratch, "missing.log")}`]);
    expect(out).toMatch(/File not found/i);
  });

  it("ignore-missing makes a missing single file OK", async () => {
    const out = await query("check_single_file", [
      `file=${path.join(scratch, "missing.log")}`,
      "ignore-missing=true",
    ]);
    expect(out).toMatch(/ignored/i);
    // The path is named so the result is not mistaken for "the file was fine".
    expect(out).toMatch(/missing\.log/);
  });

  it("ignore-missing does not change a file that is present", async () => {
    const out = await query("check_single_file", [
      `file=${path.join(scratch, "big.log")}`,
      "ignore-missing=true",
    ]);
    expect(out).toMatch(/size=5000/);
    expect(out).not.toMatch(/ignored/i);
  });

  // --- check_disk_write ------------------------------------------------------
  //
  // The client-query path passes k=v as single tokens (same as REST), so this
  // exercises the same argument parsing the REST API uses. Output here is the
  // raw Nagios "message|perfdata" line without a status-word prefix.

  it("check_disk_write performs a write/read/delete round-trip", async () => {
    const file = path.join(scratch, "write-probe.dat");
    const out = await query("check_disk_write", [
      `file=${file}`,
      "size=64k",
      "warning=total_time > 999999",
      "critical=total_time > 999999",
    ]);
    expect(out).toMatch(/wrote and read back 65536 bytes in \d+ms/);
    // Thresholding on total_time emits it as perf data, and the probe file is
    // cleaned up again.
    expect(out).toMatch(/total_time'?=\d+ms/);
    expect(fs.existsSync(file)).toBe(false);
  });

  it("check_disk_write accepts a size with byte units", async () => {
    const out = await query("check_disk_write", [
      `file=${path.join(scratch, "write-probe-units.dat")}`,
      "size=4k",
      "detail-syntax=%(size)",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/^4096/m);
  });

  it("check_disk_write rejects a size above the 1M maximum", async () => {
    const out = await query("check_disk_write", [`file=${path.join(scratch, "too-big.dat")}`, "size=2M"]);
    expect(out).toMatch(/Size too large/);
  });

  it("check_disk_write refuses to overwrite an existing file", async () => {
    const existing = path.join(scratch, "precious.dat");
    fs.writeFileSync(existing, "do not touch");
    const out = await query("check_disk_write", [`file=${existing}`]);
    expect(out).toMatch(/already exists/);
    expect(fs.readFileSync(existing, "utf8")).toBe("do not touch");
  });

  it("check_disk_write goes critical on an unwritable target", async () => {
    const out = await query("check_disk_write", [`file=${path.join(scratch, "no", "such", "dir", "probe.dat")}`]);
    expect(out).toMatch(/failed to create file/);
  });

  it("check_disk_write requires a file argument", async () => {
    const out = await query("check_disk_write", []);
    expect(out).toMatch(/No file specified/);
  });

  // --- number formatting (issue #1428) -------------------------------------
  //
  // These go over the one-shot client-query path, which passes each option as
  // the single token `key=value` exactly as REST does.

  const formatArgs = (extra: string[]) => [
    "drive=/",
    "warning=used>99%",
    "critical=used>99%",
    "show-all=true",
    "detail-syntax=%(used)/%(size)",
    "top-syntax=${list}",
    ...extra,
  ];

  it("renders byte values with up to three decimals by default", async () => {
    const out = await query("check_drivesize", formatArgs([]));
    expect(out).toMatch(/^-?[\d.]+(B|KB|MB|GB|TB)\/-?[\d.]+(B|KB|MB|GB|TB)/m);
  });

  it("honours decimals", async () => {
    const out = await query("check_drivesize", formatArgs(["decimals=1"]));
    // Exactly one decimal on both values, trailing zero kept.
    expect(out).toMatch(/^\d+\.\d(B|KB|MB|GB|TB)\/\d+\.\d(B|KB|MB|GB|TB)/m);
  });

  it("honours byte-unit, so both values land in the same unit", async () => {
    const out = await query("check_drivesize", formatArgs(["decimals=2", "byte-unit=GB"]));
    expect(out).toMatch(/^\d+\.\d\dGB\/\d+\.\d\dGB/m);
  });

  it("honours the decimal and thousands separators", async () => {
    const out = await query("check_drivesize", formatArgs(["decimals=2", "byte-unit=MB", "decimal-separator=,", "thousands-separator=."]));
    expect(out).toMatch(/^\d{1,3}(\.\d{3})*,\d\dMB/m);
  });

  // The whole point of keeping the number format on the message side: whatever
  // the operator does to the message, the metrics stay machine readable.
  it("leaves performance data locale neutral", async () => {
    const out = await query("check_drivesize", [
      "drive=/",
      "warning=used>99%",
      "critical=used>99%",
      "decimals=2",
      "byte-unit=GB",
      "decimal-separator=,",
      "thousands-separator=.",
    ]);
    const perf = out.slice(out.indexOf("|") + 1);
    expect(perf).toMatch(/'\/ used'=[\d.]+GB/);
    expect(perf).not.toMatch(/,/);
  });

  it("rejects a byte-unit it does not know", async () => {
    const out = await query("check_drivesize", ["drive=/", "byte-unit=ZB"]);
    expect(out).toMatch(/Invalid byte-unit: ZB/);
  });

  it("exposes format_bytes and format_number to check_drivesize templates", async () => {
    const out = await query("check_drivesize", [
      "drive=/",
      "warning=used>99%",
      "critical=used>99%",
      "show-all=true",
      "detail-syntax=%(format_bytes(used,'GB',1)) of %(format_bytes(size,'GB',1)) GB (%(format_number(used_pct,1))%)",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/^\d+\.\d of \d+\.\d GB \(\d+\.\d%\)/m);
  });

  it("reports an unknown unit in format_bytes rather than rendering nonsense", async () => {
    const out = await query("check_drivesize", [
      "drive=/",
      "show-all=true",
      "detail-syntax=%(format_bytes(used,'ZB'))",
      "top-syntax=${list}",
    ]);
    expect(out).toMatch(/Unknown byte unit: ZB/);
  });
});
