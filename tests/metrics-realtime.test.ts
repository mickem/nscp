/**
 * Dedicated suite for the two "slow" (collector/scheduler-driven) subsystems,
 * exercised end-to-end on BOTH platforms against one shared nscp instance:
 *
 *   Metrics — CheckSystem publishes a system.* metrics bundle (cpu, mem,
 *   uptime, network, process history, plus hardware sections); the core
 *   metrics scheduler fetches it every `metrics interval` and hands it to
 *   the WEBServer, which serves it as flat JSON on /api/v2/metrics and in
 *   Prometheus text format on /api/v2/openmetrics.
 *
 *   Real-time checks — the collector evaluates the configured
 *   real-time/{cpu,memory,process} filters once per second; with
 *   `destination = events` a match is emitted on the event bus
 *   (system.cpu / system.memory / system.process) and lands in the
 *   WEBServer's event store, observable via GET /api/v2/events. This is
 *   also the first coverage that actually drives events INTO the events
 *   controller (rest-events.test.ts only exercises the empty buffer).
 *
 * Everything shares one `nscp test` process configured up front, so the
 * warm-up (collector samples + first metrics push) is paid only once.
 */
import request from "supertest";

import { NscpInstance, REST_URL, setupQueryNscp } from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32";
/** Our own long-lived process — the one thing guaranteed to be running. */
const SELF_EXE = onWindows ? "nscp.exe" : "nscp";
const SYSTEM_PATH = onWindows ? "/settings/system/windows" : "/settings/system/unix";

interface RestEvent {
  index: number;
  event: string;
  date: string;
  data: Record<string, string>;
}

/** GET an API path and return the JSON payload. Depending on the controller's
 * content-type superagent either pre-parses into res.body (application/json)
 * or leaves the raw text (everything else) — handle both. */
async function getJson<T>(key: string, path: string): Promise<T> {
  const res = await request(REST_URL)
    .get(path)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .expect(200);
  if (res.text && res.text.length > 0) return JSON.parse(res.text) as T;
  return res.body as T;
}

/** GET an API path as raw text, bypassing superagent's content-type-driven
 * body parsing (openmetrics serves plain text under a JSON content-type). */
async function getText(key: string, path: string): Promise<string> {
  const res = await request(REST_URL)
    .get(path)
    .set("Authorization", `Bearer ${key}`)
    .trustLocalhost(true)
    .buffer(true)
    .parse((r, callback) => {
      let data = "";
      r.on("data", (chunk: Buffer) => (data += chunk.toString()));
      r.on("end", () => callback(null, data));
    })
    .expect(200);
  return res.body as string;
}

/** Re-fetch until `until(value)` holds (scheduler/collector warm-up).
 * Returns the last value either way so asserts fail with the real payload. */
async function poll<T>(
  fetch: () => Promise<T>,
  until: (v: T) => boolean,
  timeoutMs = 60_000,
): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  let last: T;
  for (;;) {
    last = await fetch();
    if (until(last) || Date.now() >= deadline) return last;
    await new Promise((r) => setTimeout(r, 500));
  }
}

/** Flat metric map ({"system.cpu.total.user": 42, ...}) from /api/v2/metrics. */
async function getMetrics(key: string): Promise<Record<string, unknown>> {
  return getJson<Record<string, unknown>>(key, "/api/v2/metrics");
}

async function getEvents(key: string): Promise<RestEvent[]> {
  return getJson<RestEvent[]>(key, "/api/v2/events");
}

describe("metrics and real-time checks", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckSystem", {
      "/modules": {
        // The full replacement module list: CheckSystem + WEBServer are what
        // setupQueryNscp would enable, CheckDisk joins as the second metrics
        // producer (disk.io.* / disk.free.*). CheckDisk has no real-time mode
        // on either platform, so it only participates in the metrics tests.
        CheckSystem: "enabled",
        CheckDisk: "enabled",
        WEBServer: "enabled",
      },
      "/settings/core": {
        // Push metrics every second so the suite doesn't wait the 10s default.
        "metrics interval": "1s",
      },
      [SYSTEM_PATH]: {
        // Both platforms gate process-history collection behind this flag.
        "process history": true,
      },
      // Three always-matching real-time filters, one per subsystem, all routed
      // to the event bus. The section value is the filter expression; the
      // dedicated child section carries the destination.
      [`${SYSTEM_PATH}/real-time/cpu`]: { rt_cpu: "load >= 0" },
      [`${SYSTEM_PATH}/real-time/cpu/rt_cpu`]: { destination: "events" },
      [`${SYSTEM_PATH}/real-time/memory`]: { rt_mem: "used >= 0" },
      [`${SYSTEM_PATH}/real-time/memory/rt_mem`]: { destination: "events" },
      [`${SYSTEM_PATH}/real-time/process`]: { rt_proc: "exe like 'nscp'" },
      [`${SYSTEM_PATH}/real-time/process/rt_proc`]: {
        destination: "events",
        process: SELF_EXE,
      },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  // --- metrics --------------------------------------------------------------

  it("publishes core system metrics (cpu, mem, uptime) to /api/v2/metrics", async () => {
    const metrics = await poll(
      () => getMetrics(key),
      (m) => Object.keys(m).some((k) => k.startsWith("system.cpu.")),
    );
    const keys = Object.keys(metrics);
    expect(keys.some((k) => k.startsWith("system.cpu."))).toBe(true);
    expect(keys.some((k) => k.startsWith("system.mem."))).toBe(true);
    expect(keys.some((k) => k.startsWith("system.uptime."))).toBe(true);
  });

  it("publishes CheckDisk io and free-space metrics", async () => {
    // disk.io.* comes from CheckDisk's own 1 Hz sampler, disk.free.* from the
    // mounted-filesystem walk; both need a sample before they show up.
    const metrics = await poll(
      () => getMetrics(key),
      (m) =>
        Object.keys(m).some((k) => k.startsWith("disk.io.")) &&
        Object.keys(m).some((k) => k.startsWith("disk.free.")),
    );
    const keys = Object.keys(metrics);
    expect(keys.some((k) => k.startsWith("disk.io.") && k.endsWith(".read_bytes_per_sec"))).toBe(
      true,
    );
    expect(keys.some((k) => k.startsWith("disk.free.") && k.endsWith(".free_pct"))).toBe(true);
  });

  it("publishes collector-backed network metrics after warm-up", async () => {
    const metrics = await poll(
      () => getMetrics(key),
      (m) => Object.keys(m).some((k) => k.startsWith("system.network.")),
    );
    expect(Object.keys(metrics).some((k) => k.startsWith("system.network."))).toBe(true);
  });

  it("publishes process-history metrics including our own process", async () => {
    const metrics = await poll(
      () => getMetrics(key),
      (m) => Object.keys(m).some((k) => /^system\.process_history\..*nscp.*\.times_seen$/i.test(k)),
    );
    const keys = Object.keys(metrics);
    expect(keys.some((k) => /^system\.process_history\..*nscp.*\.times_seen$/i.test(k))).toBe(true);
    // At least our own process is running right now.
    expect(metrics["system.process_history.running"] as number).toBeGreaterThanOrEqual(1);
    expect(metrics["system.process_history.count"] as number).toBeGreaterThanOrEqual(1);
  });

  it("exposes the same metrics in Prometheus format on /api/v2/openmetrics", async () => {
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics"),
      (t) => /^system_cpu_/m.test(t),
    );
    // One "name value" sample per line, dots flattened to underscores.
    expect(text).toMatch(/^system_cpu_\S+ [\d.]+$/m);
    expect(text).toMatch(/^system_mem_\S+ [\d.]+$/m);
  });

  // --- real-time checks -----------------------------------------------------

  it("real-time cpu filter emits system.cpu events", async () => {
    const events = await poll(
      () => getEvents(key),
      (list) => list.some((e) => e.event.startsWith("system.cpu")),
    );
    const cpu = events.find((e) => e.event.startsWith("system.cpu"));
    expect(cpu).toBeDefined();
    // The event payload is the filter row rendered as a hash of keywords.
    expect(Object.keys(cpu!.data).length).toBeGreaterThan(0);
  });

  it("real-time memory filter emits system.memory events", async () => {
    const events = await poll(
      () => getEvents(key),
      (list) => list.some((e) => e.event.startsWith("system.memory")),
    );
    const mem = events.find((e) => e.event.startsWith("system.memory"));
    expect(mem).toBeDefined();
    expect(Object.keys(mem!.data).length).toBeGreaterThan(0);
  });

  it("real-time process filter emits system.process events for our process", async () => {
    const events = await poll(
      () => getEvents(key),
      (list) => list.some((e) => e.event.startsWith("system.process")),
    );
    const proc = events.find((e) => e.event.startsWith("system.process"));
    expect(proc).toBeDefined();
    expect(JSON.stringify(proc!.data)).toMatch(/nscp/i);
  });

  it("DELETE /api/v2/events drains the buffered real-time events", async () => {
    // By now the filters have been firing once per second; the drain must
    // return a non-empty batch (this is the get-and-clear contract).
    const res = await request(REST_URL)
      .delete("/api/v2/events")
      .set("Authorization", `Bearer ${key}`)
      .trustLocalhost(true)
      .expect(200);
    const drained = JSON.parse(res.text) as RestEvent[];
    expect(drained.length).toBeGreaterThan(0);
  });
});
