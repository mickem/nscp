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
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onLinux = process.platform === "linux" ? describe : describe.skip;

onLinux("CheckDisk (Unix)", () => {
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
});
