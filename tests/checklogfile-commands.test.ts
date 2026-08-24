/**
 * Exercises CheckLogFile's check_logfile end-to-end, with the emphasis on the
 * `bookmark` option (#561): a bookmarked check must report each line ONCE
 * instead of re-reporting the whole file on every run - and on `max-lines` /
 * `newest` (#583), which pick the newest N lines out of whichever end of the
 * file they are written to.
 *
 * Queries run over the REST API against a long-lived `nscp test` process
 * because that is what the feature needs: the read position lives in the
 * module instance, so the state under test only exists while one process
 * serves several consecutive queries. REST also exercises the `k=v`
 * single-token argument path, which is where a boolean/implicit-value option
 * like `bookmark` (used bare) can break without the CLI noticing.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import {
  NscpInstance,
  OK,
  WARNING,
  executeQuery,
  messageOf,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

describe("CheckLogFile check_logfile", () => {
  let nscp: NscpInstance;
  let key: string;
  let scratch: string;
  let counter = 0;

  /** A fresh log file per test so bookmarks never collide across cases. */
  function newLog(contents = ""): string {
    const file = path.join(scratch, `test-${counter++}.log`);
    fs.writeFileSync(file, contents);
    return file;
  }

  function append(file: string, contents: string): void {
    fs.appendFileSync(file, contents);
  }

  /** Count the lines matching `filter` in `file`, without a bookmark. */
  function check(file: string, extra: Record<string, string> = {}) {
    return executeQuery(key, "check_logfile", {
      file,
      filter: "column1 like 'ERROR'",
      warning: "count > 0",
      critical: "count > 5",
      "empty-state": "ok",
      ...extra,
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    scratch = fs.mkdtempSync(path.join(os.tmpdir(), "checklogfile-it-"));
    key = await setupQueryNscp(nscp, "CheckLogFile");
  });

  afterAll(async () => {
    await nscp?.stop();
    fs.rmSync(scratch, { recursive: true, force: true });
  });

  // --- without a bookmark (unchanged behaviour) -----------------------------

  it("scans the whole file on every run when no bookmark is used", async () => {
    const file = newLog("ERROR one\nINFO two\nERROR three\n");

    const first = await check(file);
    expect(first.result).toBe(WARNING);
    expect(messageOf(first)).toMatch(/2\/3/);

    // Same file, same answer: the entire file is re-read every time.
    const second = await check(file);
    expect(messageOf(second)).toMatch(/2\/3/);
  });

  it("compares columns against decimal thresholds numerically", async () => {
    // Tab-separated columns (the default column-split). column2 is a dual
    // string/number keyword: the decimal literal re-types it to float and
    // the value is served from the numeric accessor — `column2 > 2.5` used
    // to fail to evaluate ("no accessor"), and before that the whole class
    // of string-vs-number comparisons ordered lexically.
    const file = newLog("low\t2\nhigh\t3\nhighest\t10\n");

    // 3 and 10 exceed 2.5; a lexical compare would also (wrongly) drop "10"
    // since the text "10" sorts below "2.5".
    const matching = await check(file, { filter: "column2 > 2.5" });
    expect(matching.result).toBe(WARNING);
    expect(messageOf(matching)).toMatch(/2\/3/);

    // Nothing exceeds 10.5 — and the non-match must be a clean OK.
    const none = await check(file, { filter: "column2 > 10.5" });
    expect(none.result).toBe(OK);
  });

  it("compares a column against a bare number through the numeric accessor", async () => {
    // Ordering discriminator: 100 > 90 as numbers, while the text "100"
    // sorts below "90" — a lexical compare would report 0 matches.
    const file = newLog("a\t100\nb\t9\n");

    const gt = await check(file, { filter: "column2 > 90" });
    expect(gt.result).toBe(WARNING);
    expect(messageOf(gt)).toMatch(/1\/2/);

    const none = await check(file, { filter: "column2 > 200" });
    expect(none.result).toBe(OK);
  });

  // --- bookmarks (#561) ----------------------------------------------------

  it("only reports lines added since the previous bookmarked check", async () => {
    const file = newLog("ERROR one\nINFO two\n");

    // First check of a file reads it in full.
    const first = await check(file, { bookmark: "it-basic" });
    expect(messageOf(first)).toMatch(/1\/2/);
    expect(first.result).toBe(WARNING);

    // Nothing was written: nothing to report.
    const second = await check(file, { bookmark: "it-basic" });
    expect(second.result).toBe(OK);
    expect(messageOf(second)).toMatch(/Nothing found/i);

    // Only the new lines are considered.
    append(file, "INFO three\nERROR four\n");
    const third = await check(file, { bookmark: "it-basic" });
    expect(messageOf(third)).toMatch(/1\/2/);
    expect(third.result).toBe(WARNING);

    const fourth = await check(file, { bookmark: "it-basic" });
    expect(fourth.result).toBe(OK);
  });

  it("holds back a line which has not been terminated yet", async () => {
    const file = newLog("ERROR one\n");

    expect(messageOf(await check(file, { bookmark: "it-partial" }))).toMatch(/1\/1/);

    // A half-written line must not be reported (it would be reported again,
    // in full, once the terminator arrives).
    append(file, "ERROR two-in-prog");
    const partial = await check(file, { bookmark: "it-partial" });
    expect(partial.result).toBe(OK);
    expect(messageOf(partial)).toMatch(/Nothing found/i);

    // Once complete it is reported exactly once, in full.
    append(file, "ress\n");
    const complete = await check(file, { bookmark: "it-partial" });
    expect(messageOf(complete)).toMatch(/1\/1/);
    expect(messageOf(complete)).toContain("ERROR two-in-progress");
  });

  it("re-reads from the start when the file is truncated", async () => {
    const file = newLog("ERROR one\nERROR two\n");
    expect(messageOf(await check(file, { bookmark: "it-truncate" }))).toMatch(/2\/2/);

    // Truncate + rewrite with less content than we had already read: without
    // the shrink check the new lines would be skipped entirely.
    fs.writeFileSync(file, "ERROR fresh\n");
    expect(messageOf(await check(file, { bookmark: "it-truncate" }))).toMatch(/1\/1/);
  });

  it("re-reads from the start when the file is replaced by a bigger one", async () => {
    const file = newLog("ERROR one\n");
    expect(messageOf(await check(file, { bookmark: "it-rotate" }))).toMatch(/1\/1/);

    // Rotation: a brand-new file under the same name which is LARGER than the
    // offset we stored. Only the content fingerprint catches this.
    fs.writeFileSync(file, "ERROR a\nERROR b\nERROR c\n");
    const after = await check(file, { bookmark: "it-rotate" });
    expect(messageOf(after)).toMatch(/3\/3/);
  });

  it("tracks each bookmark name separately", async () => {
    const file = newLog("ERROR one\n");

    expect(messageOf(await check(file, { bookmark: "it-name-a" }))).toMatch(/1\/1/);
    // A different bookmark has never seen this file, so it starts over.
    expect(messageOf(await check(file, { bookmark: "it-name-b" }))).toMatch(/1\/1/);
    // ... and the first one is still where it left off.
    expect((await check(file, { bookmark: "it-name-a" })).result).toBe(OK);
  });

  it("derives an automatic bookmark name from the file and the filters", async () => {
    const file = newLog("ERROR one\n");

    // `bookmark` with no value: the REST path passes it as a bare token, which
    // the implicit value has to accept.
    expect(messageOf(await check(file, { bookmark: "" }))).toMatch(/1\/1/);
    expect((await check(file, { bookmark: "" })).result).toBe(OK);

    // A check with a different filter gets its own position rather than
    // consuming lines the other check has not seen.
    const other = await check(file, { bookmark: "", filter: "column1 like 'one'" });
    expect(messageOf(other)).toMatch(/1\/1/);
  });

  it("does not move any position when the check fails", async () => {
    const file = newLog("ERROR one\n");
    const missing = path.join(scratch, "gone.log");

    // The second file cannot be opened, so the check reports an error - and
    // must not have consumed the first file's lines on the way there.
    const failed = await executeQuery(key, "check_logfile", {
      file: [file, missing],
      filter: "column1 like 'ERROR'",
      warning: "count > 0",
      "empty-state": "ok",
      bookmark: "it-failed",
    });
    expect(messageOf(failed)).toMatch(/Failed to open file/i);

    expect(messageOf(await check(file, { bookmark: "it-failed" }))).toMatch(/1\/1/);
  });

  it("does not consume lines when the file is checked without a bookmark", async () => {
    const file = newLog("ERROR one\n");

    expect(messageOf(await check(file, { bookmark: "it-mixed" }))).toMatch(/1\/1/);
    // Unbookmarked checks neither read nor update any stored position.
    expect(messageOf(await check(file))).toMatch(/1\/1/);
    expect((await check(file, { bookmark: "it-mixed" })).result).toBe(OK);
  });

  // A separate instance: the position has to survive a full stop/start cycle
  // (it is written to ${data-path}/nsclient.db on shutdown), which the shared
  // long-lived REST process above cannot show. One-shot `nscp client --boot`
  // runs are exactly that cycle, twice.
  it("remembers the position across a restart", async () => {
    const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "checklogfile-restart-"));
    const dataDir = path.join(workDir, "data");
    fs.mkdirSync(dataDir, { recursive: true });
    const file = path.join(workDir, "restart.log");
    fs.writeFileSync(file, "ERROR one\nINFO two\n");

    const restarted = new NscpInstance({ workDir, pathOverrides: { "data-path": dataDir } });
    const run = async () =>
      (
        await restarted.run(
          [
            "client",
            "--module",
            "CheckLogFile",
            "--boot",
            "--query",
            "check_logfile",
            `file=${file}`,
            "filter=column1 like 'ERROR'",
            "warning=count > 0",
            "empty-state=ok",
            "bookmark=it-restart",
          ],
          { allowFailure: true },
        )
      ).all ?? "";

    expect(await run()).toMatch(/1\/2 \(ERROR one\)/);
    // Second process, same bookmark: the line must not come back.
    expect(await run()).toMatch(/Nothing found/i);
    // ... and a line written while nothing was running is still picked up.
    fs.appendFileSync(file, "ERROR three\n");
    expect(await run()).toMatch(/1\/1 \(ERROR three\)/);

    fs.rmSync(workDir, { recursive: true, force: true });
  });

  // The REST API turns a valueless query parameter into a bare token, so it
  // cannot produce `bookmark=` at all. The client-query path passes `k=v`
  // verbatim, which is where an explicitly empty value has to be recognised as
  // "no name given" rather than as "no bookmark" - the mirror image of the
  // bare-token case above.
  it("treats an explicitly empty bookmark value as an automatic name", async () => {
    const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "checklogfile-empty-"));
    const dataDir = path.join(workDir, "data");
    fs.mkdirSync(dataDir, { recursive: true });
    const file = path.join(workDir, "empty-value.log");
    fs.writeFileSync(file, "ERROR one\nINFO two\n");

    const instance = new NscpInstance({ workDir, pathOverrides: { "data-path": dataDir } });
    const run = async () =>
      (
        await instance.run(
          [
            "client",
            "--module",
            "CheckLogFile",
            "--boot",
            "--query",
            "check_logfile",
            `file=${file}`,
            "filter=column1 like 'ERROR'",
            "warning=count > 0",
            "empty-state=ok",
            "bookmark=",
          ],
          { allowFailure: true },
        )
      ).all ?? "";

    expect(await run()).toMatch(/1\/2 \(ERROR one\)/);
    // Incremental, not a full re-scan: the empty value picked the automatic
    // name instead of silently disabling the bookmark.
    expect(await run()).toMatch(/Nothing found/i);

    fs.rmSync(workDir, { recursive: true, force: true });
  });

  // --- max-lines / newest (#583) -------------------------------------------

  it("reports only the newest lines when max-lines is set", async () => {
    const file = newLog("ERROR one\nERROR two\nERROR three\nERROR four\n");

    const res = await check(file, { "max-lines": "2" });
    expect(res.result).toBe(WARNING);
    // Both the count and the total shrink: only two lines were examined.
    expect(messageOf(res)).toMatch(/2\/2/);
    expect(messageOf(res)).toContain("ERROR three");
    expect(messageOf(res)).toContain("ERROR four");
    expect(messageOf(res)).not.toContain("ERROR one");
  });

  it("keeps the file order of the lines it reports", async () => {
    const file = newLog("ERROR one\nERROR two\nERROR three\n");
    expect(messageOf(await check(file, { "max-lines": "2" }))).toContain("ERROR two, ERROR three");
  });

  it("counts a final line which has no terminator", async () => {
    const file = newLog("ERROR one\nERROR two\nERROR three");
    const res = await check(file, { "max-lines": "2" });
    expect(messageOf(res)).toMatch(/2\/2/);
    expect(messageOf(res)).toContain("ERROR three");
    expect(messageOf(res)).not.toContain("ERROR one");
  });

  it("reads every line when max-lines exceeds the file", async () => {
    const file = newLog("ERROR one\nERROR two\n");
    expect(messageOf(await check(file, { "max-lines": "10" }))).toMatch(/2\/2/);
  });

  it("takes the newest lines from the top with newest=first", async () => {
    const file = newLog("ERROR one\nERROR two\nERROR three\nERROR four\n");

    const res = await check(file, { "max-lines": "2", newest: "first" });
    expect(messageOf(res)).toMatch(/2\/2/);
    expect(messageOf(res)).toContain("ERROR one");
    expect(messageOf(res)).toContain("ERROR two");
    expect(messageOf(res)).not.toContain("ERROR four");
  });

  it("finds the newest lines of a large file", async () => {
    // Big enough that the backwards scan has to cross several chunks.
    const lines: string[] = [];
    for (let i = 0; i < 100_000; i++) lines.push(`ERROR line ${i}`);
    const file = newLog(lines.join("\n") + "\n");

    const res = await check(file, { "max-lines": "3" });
    expect(messageOf(res)).toMatch(/3\/3/);
    expect(messageOf(res)).toContain("ERROR line 99999");
    expect(messageOf(res)).toContain("ERROR line 99997");
    expect(messageOf(res)).not.toContain("ERROR line 99996");
  });

  it("still consumes the lines it drops when bookmarked", async () => {
    const file = newLog("ERROR one\nERROR two\nERROR three\n");

    // A burst of lines arrived at once but only the newest two are wanted.
    const first = await check(file, { bookmark: "it-max-lines", "max-lines": "2" });
    expect(messageOf(first)).toMatch(/2\/2/);
    expect(messageOf(first)).not.toContain("ERROR one");

    // The dropped line must not come back as "new" on the next check.
    const second = await check(file, { bookmark: "it-max-lines", "max-lines": "2" });
    expect(second.result).toBe(OK);
    expect(messageOf(second)).toMatch(/Nothing found/i);

    append(file, "ERROR four\n");
    expect(messageOf(await check(file, { bookmark: "it-max-lines", "max-lines": "2" }))).toMatch(/1\/1/);
  });

  it("refuses newest=first together with a bookmark", async () => {
    const file = newLog("ERROR one\n");
    const res = await check(file, { newest: "first", bookmark: "it-newest-first" });
    expect(messageOf(res)).toMatch(/cannot be combined with bookmark/i);
  });

  it("rejects an unknown newest value", async () => {
    const file = newLog("ERROR one\n");
    expect(messageOf(await check(file, { newest: "middle" }))).toMatch(/Invalid newest/i);
  });

  it("fails cleanly for a missing file", async () => {
    const missing = path.join(scratch, "does-not-exist.log");
    const res = await check(missing, { bookmark: "it-missing" });
    expect(messageOf(res)).toMatch(/Failed to open file/i);
  });
});
