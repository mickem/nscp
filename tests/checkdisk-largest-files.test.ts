/**
 * Exercises CheckDisk's check_largest_files end-to-end on both platforms.
 *
 * Each case runs a one-shot client query - `nscp client --module CheckDisk
 * --boot --query check_largest_files k=v ...` - which passes every k=v as a
 * single token, the way REST does, so the valued-boolean trap is covered too.
 * No web server is needed: the check reads the disk when it is asked, not a
 * background collector. The client path prints the raw message with no status
 * word, so the status is read from the exit code.
 *
 * The fixture tree holds a link (a junction on Windows, a symlink elsewhere)
 * to a folder with a much larger file: the link must not be followed, so that
 * file never shows up. The exact assertions use method=walk, which reads the
 * live tree. method=mft reads the NTFS Master File Table off the volume, which
 * needs an elevated agent, and NTFS writes freshly created records to disk
 * lazily, so that path is only compared once it has caught up.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import { NscpInstance, describeWithModules, onWindows } from "@fixtures/index";

jest.setTimeout(180_000);

const OK = 0;
const WARNING = 1;
const UNKNOWN = 3;

describeWithModules("CheckDisk")("check_largest_files", () => {
  let nscp: NscpInstance;
  let work: string;
  let tree: string;

  async function query(args: Record<string, string>): Promise<{ out: string; code: number }> {
    const tokens = Object.entries(args).map(([k, v]) => `${k}=${v}`);
    const r = await nscp.run(
      ["client", "--module", "CheckDisk", "--boot", "--query", "check_largest_files", ...tokens],
      { allowFailure: true },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** The result line: the message as rendered, without any log output around it. */
  function lineMatching(out: string, re: RegExp): string | undefined {
    return out.split(/\r?\n/).find((l) => re.test(l));
  }

  const ranked = { "top-syntax": "${list}", "detail-syntax": "${rank}:${name}" };

  beforeAll(() => {
    nscp = new NscpInstance();
    work = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-largest-"));
    tree = path.join(work, "tree");
    const outside = path.join(work, "outside");
    const write = (rel: string, size: number) => {
      const file = path.join(tree, rel);
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, "x".repeat(size));
    };
    write("big.bin", 30_000);
    write("a/mid.log", 20_000);
    write("a/deep/small.log", 10_000);
    write("b/tiny.txt", 100);
    fs.mkdirSync(outside, { recursive: true });
    fs.writeFileSync(path.join(outside, "huge.bin"), "x".repeat(100_000));
    // A junction needs no privilege on Windows, unlike a symbolic link.
    fs.symlinkSync(outside, path.join(tree, "link"), onWindows ? "junction" : "dir");
  });

  afterAll(() => {
    if (work) fs.rmSync(work, { recursive: true, force: true });
  });

  it("ranks the largest files and does not follow the link", async () => {
    const r = await query({ path: tree, method: "walk", top: "3", ...ranked });
    expect(r.code).toBe(OK);
    expect(lineMatching(r.out, /^1:/)?.trim()).toBe("1:big.bin, 2:mid.log, 3:small.log");
    expect(r.out).not.toContain("huge.bin");
  });

  it("the default syntax shows the path and a human-readable size", async () => {
    const r = await query({ path: tree, method: "walk", top: "1" });
    expect(r.code).toBe(OK);
    expect(r.out).toContain(`${path.join(tree, "big.bin")} (29.297KB)`);
  });

  it("pattern and min-size narrow the files that are ranked", async () => {
    const r = await query({
      path: tree,
      method: "walk",
      pattern: "*.log",
      "min-size": "15000",
      ...ranked,
    });
    expect(r.code).toBe(OK);
    expect(lineMatching(r.out, /^1:/)?.trim()).toBe("1:mid.log");
  });

  it("type=folders ranks the recursive folder totals", async () => {
    const r = await query({
      path: tree,
      method: "walk",
      type: "folders",
      "top-syntax": "${list}",
      "detail-syntax": "${name}=${file_count}",
      warning: "size > 25K",
    });
    // a = 20000 + 10000 bytes in two files; b = 100 bytes; the link is not a folder.
    expect(lineMatching(r.out, /^a=/)?.split("|")[0].trim()).toBe("a=2, b=1");
    expect(r.code).toBe(WARNING);
  });

  it("folder-depth=-1 ranks every folder beneath the root", async () => {
    const r = await query({
      path: tree,
      method: "walk",
      type: "folders",
      "folder-depth": "-1",
      "top-syntax": "${list}",
      "detail-syntax": "${name}",
    });
    expect(r.code).toBe(OK);
    expect(lineMatching(r.out, /^a, /)?.trim()).toBe("a, deep, b");
  });

  it("thresholds apply to the ranked entries and carry the size as perfdata", async () => {
    const r = await query({
      path: tree,
      method: "walk",
      warning: "size > 25K",
      critical: "size > 1M",
    });
    expect(r.code).toBe(WARNING);
    // The value is scaled to the thresholds' unit: 30000 bytes against 25K and 1M is in MB.
    expect(r.out).toMatch(/\|'[^']*big\.bin'=0\.0286\d*MB;/);
  });

  it("accepts a valued boolean as a single token", async () => {
    // The bool_switch trap: REST passes same-filesystem=true as one token.
    const r = await query({
      path: tree,
      method: "walk",
      "same-filesystem": "true",
      top: "1",
      ...ranked,
    });
    expect(r.code).toBe(OK);
    expect(lineMatching(r.out, /^1:/)?.trim()).toBe("1:big.bin");
  });

  it("nothing above min-size is OK", async () => {
    const r = await query({ path: tree, "min-size": "1G" });
    expect(r.code).toBe(OK);
    expect(r.out).toContain("No files found");
  });

  it("a missing path is UNKNOWN", async () => {
    const r = await query({ path: path.join(work, "missing") });
    expect(r.code).toBe(UNKNOWN);
    expect(r.out).toContain("Path not found");
  });

  it("an invalid min-size is UNKNOWN", async () => {
    const r = await query({ path: tree, "min-size": "1.5G" });
    expect(r.code).toBe(UNKNOWN);
    expect(r.out).toContain("Invalid min-size");
  });

  it("method=auto answers whichever way it scans", async () => {
    const r = await query({ path: tree, top: "1", ...ranked });
    expect(r.code).toBe(OK);
    expect(r.out).not.toContain("huge.bin");
  });

  it("method=mft reads the Master File Table where it can, and says why where it cannot", async () => {
    const first = await query({ path: tree, method: "mft", top: "3", ...ranked });
    if (!onWindows) {
      expect(first.code).toBe(UNKNOWN);
      expect(first.out).toContain("only available on Windows");
      return;
    }
    if (first.code === UNKNOWN) {
      // Not elevated, or not NTFS: the reason is reported, not a crash.
      expect(first.out).toContain("Unable to read the MFT");
      return;
    }
    // Elevated on NTFS: once the new records are on disk the MFT agrees with
    // the walk, the link skipped and all.
    const expectEventually = async (args: Record<string, string>, re: RegExp, want: string) => {
      let got: string | undefined;
      for (const deadline = Date.now() + 60_000; Date.now() < deadline; ) {
        got = lineMatching((await query(args)).out, re)?.trim();
        if (got === want) break;
        await new Promise((resolve) => setTimeout(resolve, 2_000));
      }
      expect(got).toBe(want);
    };
    await expectEventually(
      { path: tree, method: "mft", top: "3", ...ranked },
      /^1:/,
      "1:big.bin, 2:mid.log, 3:small.log",
    );
    await expectEventually(
      {
        path: tree,
        method: "mft",
        type: "folders",
        "top-syntax": "${list}",
        "detail-syntax": "${name}=${file_count}",
      },
      /^a=/,
      "a=2, b=1",
    );
  });
});
