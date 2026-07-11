/**
 * Exercises CheckEventLog's core (non-bookmark) query behaviour end-to-end on
 * Windows: the level→status mapping that drives the default thresholds, level /
 * id filtering, `unique` de-duplication, count thresholds and multi-file
 * aggregation.
 *
 * Events are injected via .NET's EventLog.WriteEntry (see src/eventlog.ts) so no
 * elevation is required. They carry no queryable message text, so every filter
 * here keys off `id`, `source` and `level`. Each test isolates its own events
 * with a per-run id range plus (where it matters) a specific id, so real system
 * events never interfere. The suite FAILS in beforeAll (never silently skips) if
 * no writable event source is available.
 */
import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  WARNING,
  type EventLevel,
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

onWindows("CheckEventLog commands", () => {
  let nscp: NscpInstance;
  let key: string;
  let source: string;
  const log = "Application";
  // Band 50000+, distinct from the checkeventlog-bookmark suite's band (which
  // stays below 50000), so the two suites don't match each other's events in the
  // shared Windows event log.
  const ids = eventIdAllocator(50000);

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
   * Inject one event with a specific level and wait until it is visible to a
   * query on its (unique) id, so the assertion below sees a settled log without
   * a flaky fixed sleep. Returns the id.
   */
  async function inject(level: EventLevel, message = "nscp test"): Promise<number> {
    const id = ids.next();
    if (!writeEventLogEntry(source, id, level, message)) throw new Error(`failed to inject event to '${source}'`);
    // Visibility poll: critical=none so an Error event (which would otherwise trip
    // the default CRITICAL) still resolves to WARNING here — we only care that the
    // event is now visible, not its severity.
    await pollQuery(
      key,
      "check_eventlog",
      { file: log, filter: `id = ${id} and source = '${source}'`, warning: "count > 0", critical: "none", "top-syntax": "${count}" },
      (r) => r.result === WARNING,
      20_000,
    );
    return id;
  }

  /** A query scoped to a single injected event by id (no bookmark). */
  const byId = (id: number, extra: Record<string, string> = {}) => ({
    file: log,
    filter: `id = ${id} and source = '${source}'`,
    "top-syntax": "${count}",
    ...extra,
  });

  // --- level -> status mapping (drives the default thresholds) ----------------

  it("maps an error event to CRITICAL via the default level thresholds", async () => {
    const id = await inject("Error");
    // No warn/crit override: the built-in crit is level in ('error','critical').
    const q = await executeQuery(key, "check_eventlog", byId(id));
    expect(q.result).toBe(CRITICAL);
  });

  it("maps a warning event to WARNING via the default level thresholds", async () => {
    const id = await inject("Warning");
    const q = await executeQuery(key, "check_eventlog", byId(id));
    expect(q.result).toBe(WARNING);
  });

  it("leaves an information event OK (below the default thresholds)", async () => {
    const id = await inject("Information");
    // The event matches the id filter, but info trips neither the default warn
    // (level='warning') nor crit (level in ('error','critical')).
    const q = await executeQuery(key, "check_eventlog", byId(id));
    expect(q.result).toBe(OK);
  });

  it("exposes the level keyword as a queryable string", async () => {
    const id = await inject("Error");
    // critical=none so the error resolves to WARNING; render the row's level.
    const q = await executeQuery(key, "check_eventlog", {
      file: log,
      filter: `id = ${id} and source = '${source}'`,
      warning: "count > 0",
      critical: "none",
      "top-syntax": "${list}",
      "detail-syntax": "${level}",
    });
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toBe("error");
  });

  // --- explicit level filtering ----------------------------------------------

  it("an explicit level filter selects only the matching severity", async () => {
    const err = await inject("Error");
    const warn = await inject("Warning");
    // Filter to our two events, then narrow to errors only.
    const q = await executeQuery(key, "check_eventlog", {
      file: log,
      filter: `source = '${source}' and (id = ${err} or id = ${warn}) and level = 'error'`,
      warning: "count > 0",
      critical: "none", // isolate the count assertion from the default level crit
      "top-syntax": "${count}",
    });
    expect(q.result).toBe(WARNING); // warning=count>0 fired
    expect(messageOf(q)).toBe("1"); // exactly the error, not the warning
  });

  // --- count thresholds -------------------------------------------------------

  it("thresholds on the matched count", async () => {
    // Two events sharing one id; select both by that id.
    const id = ids.next();
    for (let i = 0; i < 2; i++) {
      if (!writeEventLogEntry(source, id, "Information", `count-${i}`)) throw new Error("inject failed");
    }
    await pollQuery(
      key,
      "check_eventlog",
      { file: log, filter: `id = ${id} and source = '${source}'`, warning: "count > 1", "top-syntax": "${count}" },
      (r) => (messageOf(r) === "2" ? true : false),
      20_000,
    );
    // count is 2: a >1 warning trips, a >5 warning does not.
    expect((await executeQuery(key, "check_eventlog", byId(id, { warning: "count > 1" }))).result).toBe(WARNING);
    expect((await executeQuery(key, "check_eventlog", byId(id, { warning: "count > 5" }))).result).toBe(OK);
  });

  // --- unique / index de-duplication -----------------------------------------

  it("unique collapses duplicate log/source/id rows in the list", async () => {
    const id = ids.next();
    for (let i = 0; i < 3; i++) {
      if (!writeEventLogEntry(source, id, "Information", `dup-${i}`)) throw new Error("inject failed");
    }
    // Render the matched rows as a comma-separated list of ids so we can count
    // list entries. `unique` dedups the LIST (not the `count` keyword, which
    // stays at the total number of matched records).
    const listArgs = (extra: Record<string, string> = {}) => ({
      file: log,
      filter: `id = ${id} and source = '${source}'`,
      warning: "count > 0",
      critical: "none",
      "top-syntax": "${list}",
      "detail-syntax": "${id}",
      ...extra,
    });
    await pollQuery(key, "check_eventlog", listArgs(), (r) => messageOf(r).split(", ").length === 3, 20_000);
    // Without unique: three list rows.
    expect(messageOf(await executeQuery(key, "check_eventlog", listArgs())).split(", ")).toHaveLength(3);
    // With unique (index ${log}-${source}-${id}): the three collapse to one row.
    const uniq = await executeQuery(key, "check_eventlog", listArgs({ unique: "true" }));
    expect(messageOf(uniq).split(", ")).toHaveLength(1);
    expect(messageOf(uniq)).toBe(`${id}`);
  });

  // --- multi-file aggregation & empty state ----------------------------------

  it("aggregates across multiple files (file=Application file=System)", async () => {
    const id = await inject("Information");
    // Our event is in Application; querying both logs (repeated file=) must still
    // find it in the aggregate set.
    const q = await executeQuery(key, "check_eventlog", {
      file: ["Application", "System"],
      filter: `id = ${id} and source = '${source}'`,
      warning: "count > 0",
      critical: "none",
      "top-syntax": "${count}",
    });
    expect(q.result).toBe(WARNING);
    expect(messageOf(q)).toBe("1");
  });

  it("reports OK for a filter that matches nothing", async () => {
    // An id we never inject: no rows match, so the check is OK (empty state).
    const q = await executeQuery(key, "check_eventlog", {
      file: log,
      filter: `id = ${ids.base - 1} and source = '${source}'`,
      warning: "count > 0",
    });
    expect(q.result).toBe(OK);
    expect(q.result).not.toBe(UNKNOWN);
  });
});
