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
    const r = await nscp.run(["client", "--module", "CheckDisk", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
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
});
