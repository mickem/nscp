/**
 * Windows CheckDisk check_files / check_single_file, ported from the legacy
 * scripts/python/test_w32_file.py (an `nscp unit` script run by
 * acceptance-tests.bat). checkdisk-unix covers the same command on Linux;
 * this is its Windows twin, and it keeps every assertion the Python script
 * made:
 *
 *   - the fixture tree matrix: size, pattern, max-depth and written-date
 *     filters over ten files in three nested folders, each case asserting
 *     the match count and the status the count thresholds imply;
 *   - the regressions #730 (max-depth=0 scans the top directory only), #613
 *     (a missing path is UNKNOWN, alone or among valid ones), #717 (the
 *     legacy CheckFiles shim treats no matches as OK) and #598 (paths
 *     outside the active code page are readable);
 *   - the check_files path-handling edge cases and the check_single_file
 *     acceptance cases.
 *
 * Queries go over REST, which hands the agent UTF-8 and so reaches the #598
 * code path the way a remote caller does - a command line would first pass
 * through the console code page. They need no collector, so one agent serves
 * the whole suite.
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
  describeOnWindows,
  executeQuery,
  messageOf,
  perfOf,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

/**
 * The legacy fixture: name, size in bytes, mtime offset from now in minutes,
 * and the folder it sits in. Three folders (001, 001/002, 001/002/003), ten
 * files of which nine are *.txt.
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

/** The status the matrix thresholds give a count: warning > 1, critical > 3, empty unknown. */
function statusForCount(count: number): number {
  if (count > 3) return CRITICAL;
  if (count > 1) return WARNING;
  if (count > 0) return OK;
  return UNKNOWN;
}

describeOnWindows("CheckDisk check_files and check_single_file (Windows)", () => {
  let nscp: NscpInstance;
  let key: string;
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
   * Re-stamp the fixture's mtimes relative to now. The date cases run this
   * right before their query, so the offsets hold whatever time the earlier
   * cases took.
   */
  function stampTree(): void {
    const now = Date.now();
    for (const [name, , offset, folder] of FIXTURE) {
      const t = new Date(now + offset * 60_000);
      fs.utimesSync(path.join(tree, folder, name), t, t);
    }
  }

  beforeAll(async () => {
    work = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-w32file-"));
    tree = path.join(work, "tree");
    for (const [name, size, , folder] of FIXTURE) {
      const dir = path.join(tree, folder);
      fs.mkdirSync(dir, { recursive: true });
      let body = "";
      for (let i = 0; i < size; i++) body += String(i % 10);
      fs.writeFileSync(path.join(dir, name), body);
    }
    stampTree();

    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckDisk");
  });

  afterAll(async () => {
    await nscp?.stop();
    if (work) fs.rmSync(work, { recursive: true, force: true });
  });

  // --- the fixture tree matrix ------------------------------------------------

  /** check_files over the fixture tree with the legacy count thresholds. */
  async function matrix(filter: string, extra: Record<string, string> = {}) {
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

  // [filter, extra arguments, expected count]
  const cases: Array<[string, Record<string, string>, number]> = [
    // Sizes. Folders have size 0, so the "lt" filters count them too.
    ["size gt 0b", {}, 10],
    ["size gt 4b", {}, 2],
    ["size lt 5b", {}, 11],
    ["size eq 4b", {}, 8],
    ["size ne 4b", {}, 5],
    ["size lt 4m", {}, 13],
    ["size eq 0b", {}, 3],
    // Depth and pattern. Without max-depth every folder is counted; a depth
    // of n reaches n levels of folders.
    ["size eq 0b", { "max-depth": "1" }, 1],
    ["size eq 0b", { "max-depth": "2" }, 2],
    ["size eq 0b", { "max-depth": "3" }, 3],
    ["size eq 0b", { "max-depth": "4" }, 3],
    ["size gt 0b", { pattern: "*.txt" }, 9],
    ["size gt 0b", { pattern: "*.foo" }, 0],
    // Written dates. `ge -6m` rather than `ge -5m`: elapsed time moves the
    // threshold forward but not the files, so a bound sitting exactly on a
    // fixture offset drops those files when stamping and query straddle a
    // second boundary. Every other bound has at least a minute of margin.
    ["written ge -6m", { pattern: "*.txt" }, 7],
    ["written le -5m", { pattern: "*.txt" }, 4],
    ["written lt -9m", { pattern: "*.txt" }, 2],
    ["written gt -9m", { pattern: "*.txt" }, 7],
    ["written lt -1m", { pattern: "*.txt" }, 4],
    ["written gt -9m and written lt -1m", { pattern: "*.txt" }, 2],
    ["written gt 0m", { pattern: "*.txt" }, 4],
  ];

  it.each(cases)("filter %s %j counts %i", async (filter, extra, expected) => {
    const q = await matrix(filter, extra);
    if (expected > 0) expect(messageOf(q)).toContain(`count=${expected}`);
    expect(q.result).toBe(statusForCount(expected));
  });

  it("reports the count as its only perfdata", async () => {
    const q = await matrix("size gt 4b");
    expect(Object.values(perfOf(q)).map((e) => e.value)).toEqual([2]);
  });

  // --- regressions ----------------------------------------------------------

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
    const q = await executeQuery(key, "check_files", { path: `${tree}\\aaa.txt` });
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

  it("#717: the legacy CheckFiles shim treats no matches as OK", async () => {
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
    const root = scratch("日本語_single");
    const file = path.join(root, "日本語.txt");
    fs.writeFileSync(file, "data");
    const q = await executeQuery(key, "check_single_file", { file, warning: "size > 1M" });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).not.toContain("File not found");
  });

  // --- check_files edge cases -------------------------------------------------

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
    const q = await executeQuery(key, "check_files", {
      path: empty,
      pattern: "*.log",
      "empty-state": "ok",
    });
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
    const root = scratch("path_is_file");
    const file = path.join(root, "lonely.txt");
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

  it("tolerates a trailing backslash", async () => {
    const root = scratch("trailing_bs");
    fs.writeFileSync(path.join(root, "a.txt"), "hello");
    fs.writeFileSync(path.join(root, "b.txt"), "hello");
    const q = await countFiles({ path: `${root}\\` });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toContain("2 files");
  });

  it("tolerates forward slashes", async () => {
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
      path: `${root}\\*.log`,
      filter: "type='file'",
      warning: "count > 100",
      critical: "count > 200",
    });
    expect(q.result).toBe(UNKNOWN);
  });

  // --- check_single_file ------------------------------------------------------

  it("check_single_file: a file under the thresholds is OK and named", async () => {
    const file = path.join(scratch("single_ok"), "small.txt");
    fs.writeFileSync(file, "a few bytes of text");
    const q = await executeQuery(key, "check_single_file", {
      file,
      warning: "size > 1M",
      critical: "size > 10M",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toContain("small.txt");
  });

  it("check_single_file: over the warning threshold is WARNING", async () => {
    const file = path.join(scratch("single_warn"), "big.txt");
    fs.writeFileSync(file, "x".repeat(200_000));
    const q = await executeQuery(key, "check_single_file", {
      file,
      warning: "size > 100k",
      critical: "size > 10M",
    });
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toContain("big.txt");
  });

  it("check_single_file: over both thresholds is CRITICAL", async () => {
    const file = path.join(scratch("single_crit"), "huge.dat");
    fs.writeFileSync(file, "x".repeat(200_000));
    const q = await executeQuery(key, "check_single_file", {
      file,
      warning: "size > 50k",
      critical: "size > 100k",
    });
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
    const q = await executeQuery(key, "check_single_file", {
      file,
      warning: "size > 1M",
      critical: "size > 10M",
    });
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
    const q = await executeQuery(key, "check_single_file", {
      file: scratch("single_dir"),
      warning: "size > 1M",
    });
    expect(q.result).toBe(UNKNOWN);
  });

  it("check_single_file: perfdata carries the size", async () => {
    const file = path.join(scratch("single_perf"), "mid.dat");
    fs.writeFileSync(file, "x".repeat(1024));
    const q = await executeQuery(key, "check_single_file", {
      file,
      warning: "size > 1M",
      critical: "size > 10M",
    });
    expect(q.result).toBe(OK);
    // The label is prefixed with the file name ('mid.dat size').
    expect(Object.keys(perfOf(q)).some((label) => /size/.test(label))).toBe(true);
  });
});
