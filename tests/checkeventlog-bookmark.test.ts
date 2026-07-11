/**
 * Exercises CheckEventLog's `bookmark` option end-to-end on Windows.
 *
 * Focus: the bookmark must advance so a second consecutive check does NOT
 * re-report events the first check already saw — including when the first scan
 * reaches the end of the log (a quiet/empty read) rather than the date
 * boundary. That "advance on empty read" path used to be a no-op
 * (CheckEventLog.cpp logged "Cannot update bookmarks for empty reads yet"), so
 * on a host whose Application log is younger than the default -24h window the
 * bookmark was never persisted and every run rescanned and re-reported the same
 * events.
 *
 * Queries run over REST against a single long-lived `nscp test` instance so the
 * in-memory bookmark store persists across the three queries below.
 *
 * Determinism: each run uses a UNIQUE event source, so the only `source=<S>`
 * events in the log are the ones this test injects (via the built-in
 * `eventcreate`, the same tool the docs use). That removes historical noise and
 * makes the matched count exact regardless of scan direction:
 *   A: after injecting one event      -> 1 match  (WARNING)
 *   B: immediate re-run, nothing new  -> 0 matches (OK)      <- the regression guard
 *   C: after injecting one more       -> 1 match  (WARNING)
 * With the pre-fix behaviour B re-reports the first event (WARNING) on a
 * short-lived log, so B===OK is what proves the bookmark advanced.
 */
import { execFileSync } from "node:child_process";

import { NscpInstance, OK, WARNING, executeQuery, messageOf, setupQueryNscp } from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

onWindows("CheckEventLog bookmark", () => {
  let nscp: NscpInstance;
  let key: string;
  // Unique per run so no events from earlier runs share the source.
  const source = `NSCP_BM_${Date.now()}`;
  const log = "Application";
  const bookmark = "it-bookmark";
  // Match only our injected events; alert purely on how many were matched, so
  // the Nagios status is a direct proxy for "how many new events this run saw".
  // `filter` overrides the default level filter so we select purely by source;
  // the injected events are INFORMATION, so the default level-based critical
  // never fires and `warning=count>0` alone drives the status.
  const args = {
    file: log,
    bookmark,
    filter: `source = '${source}'`,
    warning: "count > 0",
  };

  /**
   * Write one INFORMATION event under our unique source via eventcreate.
   * INFORMATION (not ERROR) so only our explicit `warning=count>0` decides the
   * status, not the level-based defaults. Returns false if eventcreate is
   * unavailable / not permitted, so the suite skips rather than false-fails.
   */
  function emit(id: number, message: string): boolean {
    try {
      execFileSync("eventcreate", ["/ID", String(id), "/L", log, "/T", "INFORMATION", "/SO", source, "/D", message], { stdio: "ignore" });
      return true;
    } catch {
      return false;
    }
  }

  const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckEventLog");
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("advances the bookmark so a re-run does not re-report already-seen events", async () => {
    if (!emit(500, "bookmark-e1")) {
      console.warn("eventcreate unavailable (permissions?), skipping bookmark assertions");
      return;
    }
    await sleep(750); // let the write settle in the log

    // A: fresh bookmark — sees the first event.
    const a = await executeQuery(key, "check_eventlog", args);
    expect(a.result).toBe(WARNING); // count > 0

    // B: same bookmark, nothing new injected — must resume past the first event.
    const b = await executeQuery(key, "check_eventlog", args);
    expect(b.result).toBe(OK); // 0 matches; regression guard for advance-on-empty
    expect(messageOf(b)).not.toMatch(/1 message/);

    // C: inject one more, re-run — sees only the new event, not the old one.
    expect(emit(501, "bookmark-e2")).toBe(true);
    await sleep(750);
    const c = await executeQuery(key, "check_eventlog", args);
    expect(c.result).toBe(WARNING);
  });
});
