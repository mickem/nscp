/**
 * Regression suite for #1368: the `disable` setting in
 * [/settings/system/windows] must match whole tokens, not substrings.
 * "cpu" is a substring of "cpu_frequency", so `disable = cpu_frequency`
 * used to also stop the CPU load sampler — check_cpu then kept answering
 * from a buffer that was never updated, without any error.
 *
 * Windows-only: the setting and the collector live in modules/CheckSystem's
 * pdh_thread; CheckSystemUnix has no `disable` setting.
 */
import request from "supertest";

import { NscpInstance, OK, REST_URL, UNKNOWN, executeQuery, messageOf, perfValue, setupQueryNscp } from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

/** All log messages buffered by the WEBServer so far, joined for matching. */
async function fetchLogs(key: string): Promise<string> {
  const res = await request(REST_URL)
    .get("/api/v2/logs")
    .set("Authorization", `Bearer ${key}`)
    .query({ per_page: 500, page: 1 })
    .trustLocalhost(true)
    .expect(200);
  return (res.body as { message: string }[]).map((l) => l.message).join("\n");
}

/** Poll the REST log buffer until `needle` shows up (the collector thread
 * emits its boot warnings asynchronously). Returns the full log. */
async function waitForLog(key: string, needle: string, timeoutMs = 15_000): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  let logs = "";
  while (Date.now() < deadline) {
    logs = await fetchLogs(key);
    if (logs.includes(needle)) return logs;
    await new Promise((r) => setTimeout(r, 250));
  }
  throw new Error(`log line not seen within ${timeoutMs}ms: ${needle}\n--- logs ---\n${logs}`);
}

onWindows("CheckSystem disable=cpu_frequency (#1368)", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckSystem", {
      "/settings/system/windows": { disable: "cpu_frequency" },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("disables only the frequency collector, not CPU load sampling", async () => {
    // The collector logs one WARNING per disabled collector right after boot.
    // With the old substring matching, "cpu checking is disabled" appeared too.
    const logs = await waitForLog(key, "cpu frequency checking is disabled");
    expect(logs).not.toContain("WARNING: cpu checking is disabled");
  });

  it("leaves check_cpu answering from the collector", async () => {
    const q = await executeQuery(key, "check_cpu", {
      warning: "load > 101",
      critical: "load > 101",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).not.toMatch(/disabled/i);
    const load = perfValue(q, "total 5m");
    expect(load).toBeGreaterThanOrEqual(0);
    expect(load).toBeLessThanOrEqual(100);
  });
});

onWindows("CheckSystem disable=cpu (#1368)", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckSystem", {
      "/settings/system/windows": { disable: "cpu" },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("makes check_cpu report the disabled sampler instead of frozen values", async () => {
    const q = await executeQuery(key, "check_cpu", {});
    expect(q.result).toBe(UNKNOWN);
    expect(messageOf(q)).toMatch(/CPU load sampling is disabled/);
  });
});
