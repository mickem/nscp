/**
 * Exercises CheckEventLog's `bookmark` option end-to-end on Windows. A bookmark
 * must (a) NOT re-report events an earlier check already saw, and (b) still pick
 * up events that arrive AFTER the bookmark. (b) is the regression guard for the
 * reverse-scan bug where the tracking bookmark parked on the OLDEST event read,
 * so a resume could only ever seek to still-older events and never surfaced
 * anything new.
 *
 * Events are injected via .NET's EventLog.WriteEntry (through powershell), NOT
 * `eventcreate`: WriteEntry works WITHOUT elevation as long as the source is
 * already registered, whereas eventcreate needs Administrator to register a
 * fresh source. Consequences of using a shared, pre-existing source:
 *   - We disambiguate our events by a per-run id range (well above real system
 *     event ids), not by a unique source.
 *   - `${message}` is not queryable for these events (the source's message DLL
 *     has no template for our ids), so filters key off `id` + `source` only.
 *   - Each test PRIMES its bookmark first (one query that advances it past all
 *     pre-existing events) so tests are independent of one another and of prior
 *     runs.
 *
 * If no writable source can be found (locked-down host, no admin, no .NET), the
 * suite FAILS in beforeAll instead of silently skipping. A skipped assertion
 * that still reports "pass" is exactly how the original bug survived green local
 * runs, so this test refuses to no-op into a false pass.
 */
import {
  NscpInstance,
  OK,
  WARNING,
  eventIdAllocator,
  executeQuery,
  messageOf,
  pollQuery,
  resolveEventLogSource,
  setupQueryNscp,
  writeEventLogEntry,
} from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

onWindows("CheckEventLog bookmark", () => {
  let nscp: NscpInstance;
  let key: string;
  let source: string;
  const log = "Application";
  // Per-run id band [40000, ~49000), well above real system event ids and BELOW
  // the checkeventlog-commands suite's band (50000+), so the two suites — which
  // share the one Windows event log — never match each other's events.
  const ids = eventIdAllocator(40000);
  // A bookmarked query that matches only this run's injected events. The upper
  // bound keeps the (source-shared) filter inside our band. These tests assert on
  // count transitions, not severity, so critical=none disables the default
  // level threshold — otherwise a stray Error event left in the shared log by an
  // earlier run would flip the result to CRITICAL.
  const ours = (bookmark: string) => ({
    file: log,
    bookmark,
    filter: `id >= ${ids.base} and id <= ${ids.base + 999} and source = '${source}'`,
    warning: "count > 0",
    critical: "none",
    "top-syntax": "${count}",
  });

  beforeAll(async () => {
    // Throws (failing the suite) if injection is impossible — never a silent skip.
    source = resolveEventLogSource();
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckEventLog");
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  /**
   * Inject one event (unique id) and wait until it is visible to a fresh,
   * NON-bookmark query, so the next bookmarked query is guaranteed to observe it
   * without a flaky fixed sleep. The visibility poll deliberately uses a
   * bookmark-less query on the event's unique id, so it never advances any test
   * bookmark.
   */
  async function inject(message: string): Promise<number> {
    const id = ids.next();
    if (!writeEventLogEntry(source, id, "Information", message)) throw new Error(`failed to inject event to '${source}'`);
    await pollQuery(
      key,
      "check_eventlog",
      { file: log, filter: `id = ${id} and source = '${source}'`, warning: "count > 0", "top-syntax": "${count}" },
      (r) => r.result === WARNING,
      20_000,
    );
    return id;
  }

  /**
   * Advance a bookmark past every currently-existing event, so the test only
   * sees events it injects AFTER this call — independent of other tests and of
   * prior runs. (The scan reads and bookmarks every event in the window, not
   * just filter matches, so this works even when nothing matches yet.)
   */
  async function prime(bookmark: string): Promise<void> {
    await executeQuery(key, "check_eventlog", ours(bookmark));
  }

  // --- fresh-bookmark path: seed on first run, then resume ---------------------

  it("does not re-report seen events but does pick up new ones (fresh bookmark)", async () => {
    const bm = "bm-fresh";
    await inject("e1");
    // Fresh bookmark: first check sees the event, exercising the no-stored-bookmark
    // (reverse-window) path that must seed the bookmark at the NEWEST event read.
    expect((await executeQuery(key, "check_eventlog", ours(bm))).result).toBe(WARNING);
    // Immediate re-run: bookmark advanced, nothing new -> OK (no re-report).
    expect((await executeQuery(key, "check_eventlog", ours(bm))).result).toBe(OK);
    // A new event AFTER the bookmark must be picked up — this is the bug fix.
    await inject("e2");
    expect((await executeQuery(key, "check_eventlog", ours(bm))).result).toBe(WARNING);
  });

  // --- resume path: repeated forward advancement ------------------------------

  it("keeps advancing across several consecutive new events (forward resume)", async () => {
    const bm = "bm-consecutive";
    await prime(bm);
    for (let i = 0; i < 4; i++) {
      await inject(`round-${i}`);
      // Each newly-arrived event is picked up...
      expect((await executeQuery(key, "check_eventlog", ours(bm))).result).toBe(WARNING);
      // ...and not re-reported on the following run.
      expect((await executeQuery(key, "check_eventlog", ours(bm))).result).toBe(OK);
    }
  });

  it("reports every event that arrived since the last check, not just one", async () => {
    const bm = "bm-batch";
    await prime(bm);
    await inject("b1");
    await inject("b2");
    await inject("b3");
    const q = await executeQuery(key, "check_eventlog", ours(bm));
    expect(q.result).toBe(WARNING);
    // All three new events surface in the single resumed read (top-syntax=${count}).
    expect(messageOf(q)).toBe("3");
    // Nothing left to report afterwards.
    expect((await executeQuery(key, "check_eventlog", ours(bm))).result).toBe(OK);
  });

  it("tracks distinct bookmark names independently", async () => {
    const bmA = "bm-indep-a";
    const bmB = "bm-indep-b";
    await prime(bmA);
    await prime(bmB);
    await inject("shared");
    // bmA sees the event and advances past it.
    expect((await executeQuery(key, "check_eventlog", ours(bmA))).result).toBe(WARNING);
    expect((await executeQuery(key, "check_eventlog", ours(bmA))).result).toBe(OK);
    // bmB keeps its own position, so it still sees the same event.
    expect((await executeQuery(key, "check_eventlog", ours(bmB))).result).toBe(WARNING);
  });

  // --- control: the non-bookmark path is unaffected by the fix ----------------

  it("without a bookmark, re-reports the same event on every run", async () => {
    const id = await inject("control");
    const args = { file: log, filter: `id = ${id} and source = '${source}'`, warning: "count > 0", "top-syntax": "${count}" };
    // No bookmark => no "since last check" position => found on every run.
    expect((await executeQuery(key, "check_eventlog", args)).result).toBe(WARNING);
    expect((await executeQuery(key, "check_eventlog", args)).result).toBe(WARNING);
  });
});
