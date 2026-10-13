/**
 * A predefined counter that does not exist must not take the other predefined
 * counters down with it (#634).
 *
 * The collector used to add every counter in [/settings/system/windows/counters]
 * to one PDH query, so a single counter that was not registered - typically one
 * a service registers when it starts, after NSClient++ did - failed the whole
 * query and every predefined counter answered "Failed to get value" until the
 * next restart. Each counter is now added on its own and a missing one is
 * retried in the background.
 *
 * Driven over REST against a long-lived agent, so `reload=true` also goes
 * through as the single `reload=true` token REST passes. PDH is Windows-only,
 * so the suite is skipped elsewhere.
 */
import {
  NscpInstance,
  OK,
  UNKNOWN,
  describeOnWindows,
  executeQuery,
  messageOf,
  pollQuery,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(180_000);

/** Present on every Windows install. */
const THREADS = "\\System\\Threads";
/** An object no Windows install registers. */
const MISSING = "\\NSClient Missing Object 634\\Nothing";

describeOnWindows("CheckSystem predefined counters with one missing", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckSystem", {
      "/settings/system/windows/counters": { threads: THREADS, missing: MISSING },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("still serves the counters that exist, with a sampled value", async () => {
    // Until the collector has added and sampled the counter it answers
    // UNKNOWN ("Failed to get value"). The first answer that is not must be a
    // real value: every Windows host runs threads, so a value below 1 means a
    // counter was read before it was sampled - CRITICAL, not OK.
    const q = await pollQuery(
      key,
      "check_pdh",
      { counter: "threads", reload: "true", warning: "value < 0", critical: "value < 1" },
      (r) => r.result !== UNKNOWN,
    );
    expect(messageOf(q)).not.toMatch(/Failed to get value/);
    expect(messageOf(q)).not.toMatch(/does not take any arguments/);
    expect(q.result).toBe(OK);
  });

  it("reports the missing counter as missing", async () => {
    const q = await executeQuery(key, "check_pdh", {
      counter: "missing",
      reload: "true",
      warning: "value < 0",
      critical: "value < 0",
    });
    expect(q.result).toBe(UNKNOWN);
    expect(messageOf(q)).toMatch(/Failed to get value/);
  });
});
