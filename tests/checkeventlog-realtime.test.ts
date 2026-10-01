/**
 * CheckEventLog real-time filters, ported from the legacy
 * scripts/python/test_eventlog.py (an `nscp unit` script run by
 * acceptance-tests.bat). checkeventlog-commands covers the active
 * check_eventlog; nothing else covers a real-time filter matching a live
 * event and submitting it.
 *
 * Events are injected with `nscp eventlog insert`, the verb the legacy script
 * used (in-process, through Core.simple_exec): unlike EventLog.WriteEntry it
 * sets the category, which is what the legacy filters select on. It runs from
 * a second, empty configuration so the injecting process does not start real-
 * time filters of its own.
 *
 * Where the Python script subscribed a handler to the filters' channel, the
 * filters here submit to the web server's passive result cache, keyed by the
 * submitting command - which for a real-time filter is its alias - and the
 * test reads each filter's result back from GET /api/v2/results/<alias>.
 * `maximum age` is off everywhere, so nothing but a matching event (or the
 * `run on startup` sentinel below) can put an entry there.
 *
 * The same events then serve the legacy CheckEventLog command's filters,
 * thresholds and %-syntax, which the Python script also asserted.
 */
import request from "supertest";

import {
  CRITICAL,
  NscpInstance,
  OK,
  REST_URL,
  UNKNOWN,
  WARNING,
  describeOnWindows,
  eventIdAllocator,
  executeQuery,
  messageOf,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

/** The channel the filters submit to and the web server caches from. */
const CHANNEL = "evlog_rt";
const FILTERS = "/settings/eventlog/real-time/filters";

/**
 * The legacy event set: one per category, with the category deciding which
 * filter picks it up. `level` is the event type the agent reports, the
 * severity and facility are folded into the raw id and must not change the
 * id the filters see.
 */
const EVENTS = [
  { category: 0, level: "error", severity: "success", facility: 0 },
  { category: 1, level: "warning", severity: "informational", facility: 5 },
  { category: 2, level: "success", severity: "warning", facility: 5 },
  { category: 3, level: "info", severity: "error", facility: 5 },
];

const SOURCE = "Application Error";

interface CachedResult {
  key: string;
  command: string;
  status: number;
  message: string;
  channel: string;
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

describeOnWindows("CheckEventLog real-time filters", () => {
  // One id for the whole run, well away from the other eventlog suites' bands
  // (40000+ and 50000+); the categories tell the events apart.
  const id = eventIdAllocator(30000).base;
  const base = `id = ${id} and source = '${SOURCE}'`;
  let nscp: NscpInstance;
  let injector: NscpInstance;
  let key: string;

  /** The cached result for one filter alias, or undefined if none arrived. */
  async function cached(alias: string): Promise<CachedResult | undefined> {
    const res = await request(REST_URL)
      .get(`/api/v2/results/${encodeURIComponent(alias)}`)
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true);
    if (res.status === 404) return undefined;
    expect(res.status).toBe(200);
    return res.body as CachedResult;
  }

  async function waitFor(alias: string, timeoutMs = 30_000): Promise<CachedResult | undefined> {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const r = await cached(alias);
      if (r || Date.now() >= deadline) return r;
      await sleep(250);
    }
  }

  beforeAll(async () => {
    injector = new NscpInstance();
    await injector.configure({ "/modules": { CheckEventLog: "enabled" } });

    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckEventLog", {
      "/settings/WEB/server/results": {
        enabled: "true",
        channel: CHANNEL,
        "primary index": "${command}",
        "clear on poll": "false",
      },
      "/settings/eventlog/real-time": { enabled: "true" },
      // The template every filter below inherits from: the log, the
      // destination, a filter and a severity. A filter that sets none of
      // them (rt_inherit) is the test of that inheritance.
      [`${FILTERS}/default`]: {
        log: "application",
        destination: CHANNEL,
        filter: `${base} and category = 0`,
        severity: "OK",
        "maximum age": "false",
        "detail syntax": "${category} ${level}",
      },
      [`${FILTERS}/rt_warning`]: {
        filter: `${base} and category = 1`,
        severity: "WARNING",
        "detail syntax": "X1 ${id},${category} ${source}",
      },
      [`${FILTERS}/rt_critical`]: {
        filter: `${base} and category = 2`,
        severity: "CRITICAL",
        "detail syntax": "X2 ${id},${category} ${source}",
      },
      [`${FILTERS}/rt_unknown`]: {
        filter: `${base} and category = 3`,
        severity: "UNKNOWN",
        "detail syntax": "X3 ${id},${category} ${source}",
      },
      [`${FILTERS}/rt_inherit`]: {
        "detail syntax": "X4 ${id},${category} ${source}",
      },
      // Matches none of the injected events. `run on startup` makes it
      // submit once the log subscription is in place, which is the signal
      // that events injected from now on are seen.
      [`${FILTERS}/rt_ready`]: {
        filter: `${base} and category = 9`,
        "run on startup": "true",
        "empty message": "subscribed",
      },
    });

    const ready = await waitFor("rt_ready", 60_000);
    if (!ready)
      throw new Error(
        "the real-time filters never reported in: rt_ready is not in the result cache",
      );

    for (const e of EVENTS) {
      const r = await injector.run(
        [
          "client",
          "--module",
          "CheckEventLog",
          "--exec",
          "insert",
          "--",
          "--source",
          SOURCE,
          "--id",
          String(id),
          "--level",
          e.level,
          "--severity",
          e.severity,
          "--category",
          String(e.category),
          "--facility",
          String(e.facility),
          ...Array(13).flatMap(() => ["--argument", "a"]),
        ],
        { allowFailure: true },
      );
      expect({ code: r.exitCode, out: r.stdout }).toEqual({
        code: 0,
        out: expect.stringContaining("Message reported successfully"),
      });
    }
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it.each([
    ["rt_warning", WARNING, "X1", 1],
    ["rt_critical", CRITICAL, "X2", 2],
    ["rt_unknown", UNKNOWN, "X3", 3],
  ])(
    "%s submits its event with its own severity and syntax",
    async (alias, status, prefix, category) => {
      const got = await waitFor(alias);
      expect(got).toBeDefined();
      expect(got?.channel).toBe(CHANNEL);
      expect(got?.status).toBe(status);
      expect(got?.message).toContain(`${prefix} ${id},${category} ${SOURCE}`);
    },
  );

  it("a filter inherits the filter, destination and severity of the default", async () => {
    const got = await waitFor("rt_inherit");
    expect(got).toBeDefined();
    expect(got?.status).toBe(OK);
    expect(got?.message).toContain(`X4 ${id},0 ${SOURCE}`);
  });

  it("a filter that matched nothing submits nothing more than its startup message", async () => {
    // Every other filter has fired by now (the cases above wait for them).
    const got = await cached("rt_ready");
    expect(got?.status).toBe(OK);
    expect(got?.message).toBe("subscribed");
  });

  // --- the legacy CheckEventLog over the same events --------------------------

  /** Legacy CheckEventLog over the last two minutes of the Application log. */
  function legacy(args: Record<string, string>) {
    return executeQuery(key, "CheckEventLog", {
      file: "Application",
      "scan-range": "-2m",
      ...args,
    });
  }

  it.each([
    [`${base} and generated gt -1m`, "%generated%", 4],
    [`${base} and generated gt -1m and category = 1`, "%category%", 1],
    [`${base} and generated gt -1m and category = 0`, "%category%", 1],
    [`${base} and generated gt -1m and level = 'error'`, "%level%", 1],
    [`${base} and generated gt -1m and level = 'warning'`, "%level%", 1],
  ])("CheckEventLog filter %s with syntax %s counts %i", async (filter, syntax, count) => {
    const common = { filter, syntax, "top-syntax": `\${status} \${count}==${count}: \${list}` };
    const below = await legacy({ ...common, warn: `gt:${count}`, crit: `gt:${count}` });
    expect({ result: below.result, message: messageOf(below) }).toEqual({
      result: OK,
      message: expect.any(String),
    });
    const warn = await legacy({ ...common, warn: `eq:${count}`, crit: `gt:${count}` });
    expect({ result: warn.result, message: messageOf(warn) }).toEqual({
      result: WARNING,
      message: expect.any(String),
    });
    const crit = await legacy({ ...common, warn: `eq:${count}`, crit: `eq:${count}` });
    expect({ result: crit.result, message: messageOf(crit) }).toEqual({
      result: CRITICAL,
      message: expect.any(String),
    });
  });

  it.each([
    [0, "Application Error - error - 0"],
    [1, "Application Error - warning - 1"],
    [3, "Application Error - information - 3"],
  ])("CheckEventLog renders the %%-syntax of the category %i event", async (category, expected) => {
    const q = await legacy({
      filter: `${base} and generated gt -2m and category = ${category}`,
      syntax: "%source% - %type% - %category%",
      warn: "ne:1",
    });
    expect(messageOf(q)).toBe(expected);
  });
});
