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

/** GET an API path as raw text plus its Content-Type, bypassing superagent's
 * content-type-driven body parsing (the exposition is not JSON). `accept` is
 * sent verbatim when given, so the negotiation can be exercised. */
async function getTextWithType(
  key: string,
  path: string,
  accept?: string,
): Promise<{ text: string; contentType: string }> {
  let req = request(REST_URL).get(path).set("Authorization", `Bearer ${key}`);
  if (accept !== undefined) req = req.set("Accept", accept);
  const res = await req
    .trustLocalhost(true)
    .buffer(true)
    .parse((r, callback) => {
      let data = "";
      r.on("data", (chunk: Buffer) => (data += chunk.toString()));
      r.on("end", () => callback(null, data));
    })
    .expect(200);
  return { text: res.body as string, contentType: String(res.headers["content-type"] ?? "") };
}

async function getText(key: string, path: string, accept?: string): Promise<string> {
  return (await getTextWithType(key, path, accept)).text;
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

/** The sample names a family of this type may write.
 *
 * This is where the two expositions differ: OpenMetrics 1.0 names the *family*
 * on its metadata lines, so `# TYPE foo counter` governs the sample `foo_total`
 * and `# TYPE foo info` governs `foo_info`; the Prometheus text format has no
 * families, so its metadata lines name the sample itself and family == sample.
 * `info` does not exist there at all — it is a gauge valued 1. */
function samplesOf(family: string, type: string, openmetrics: boolean): string[] {
  if (!openmetrics) {
    if (type === "summary") return [family, `${family}_sum`, `${family}_count`];
    if (type === "histogram") return [`${family}_bucket`, `${family}_sum`, `${family}_count`];
    return [family];
  }
  switch (type) {
    case "counter":
      return [`${family}_total`];
    case "info":
      return [`${family}_info`];
    case "summary":
      return [family, `${family}_sum`, `${family}_count`];
    case "histogram":
      return [`${family}_bucket`, `${family}_sum`, `${family}_count`];
    default:
      return [family];
  }
}

/** Walk a whole exposition and assert its grammar: one `# TYPE` per family,
 * every sample belonging to a family declared before it, no duplicate series,
 * a `# UNIT` only on a family whose name ends in that unit, and the `# EOF`
 * terminator. Returns the families it saw, so a caller can then assert about
 * particular ones. */
function checkExposition(text: string, openmetrics: boolean): Map<string, string> {
  expect(text.endsWith("# EOF\n")).toBe(true);

  const lines = text.split("\n").slice(0, -1);
  const families = new Map<string, string>();
  const sampleNames = new Map<string, string>();
  const seenSeries = new Set<string>();
  const helped = new Set<string>();
  const united = new Set<string>();
  let samples = 0;
  for (const line of lines) {
    if (line.startsWith("#")) {
      const type = /^# TYPE (\S+) (\S+)$/.exec(line);
      if (type) {
        // Exactly one `# TYPE` per family, or the document is ambiguous.
        expect(families.has(type[1])).toBe(false);
        expect(
          openmetrics
            ? ["gauge", "counter", "unknown", "info", "summary", "histogram"]
            : ["gauge", "counter", "untyped", "summary", "histogram"],
        ).toContain(type[2]);
        families.set(type[1], type[2]);
        for (const name of samplesOf(type[1], type[2], openmetrics)) sampleNames.set(name, type[1]);
        continue;
      }
      const help = /^# HELP (\S+) (.*)$/.exec(line);
      if (help) {
        // One `# HELP` per family, naming a family, and never empty.
        expect(helped.has(help[1])).toBe(false);
        helped.add(help[1]);
        expect(help[2].length).toBeGreaterThan(0);
        // No raw newline survived into the text.
        expect(help[2]).not.toMatch(/[\r\n]/);
        continue;
      }
      const unit = /^# UNIT (\S+) (\S+)$/.exec(line);
      if (unit) {
        expect(united.has(unit[1])).toBe(false);
        united.add(unit[1]);
        // The spec requires the family name to end with its unit.
        expect(unit[1].endsWith(`_${unit[2]}`)).toBe(true);
        continue;
      }
      expect(line).toBe("# EOF");
      continue;
    }
    const sample =
      /^([a-zA-Z_][a-zA-Z0-9_]*)(\{.*\})? (-?(?:[0-9.]+(?:[eE][+-]?[0-9]+)?|Inf|NaN)|\+Inf)$/.exec(
        line,
      );
    expect(sample).not.toBeNull();
    if (sample![2] !== undefined) {
      // Every label name is legal and every value is quoted. An empty label set
      // (`name{}`) is not emitted at all - it reads as a different series from
      // the bare name to some tooling.
      const labels = sample![2].slice(1, -1);
      expect(labels).not.toBe("");
      for (const pair of labels.split(/,(?=[a-zA-Z_])/)) {
        expect(pair).toMatch(/^[a-zA-Z_][a-zA-Z0-9_]*="(?:[^"\\]|\\.)*"$/);
      }
    }
    const series = `${sample![1]}${sample![2] ?? ""}`;
    // The same series twice in one body is a duplicate a scraper rejects.
    expect(seenSeries.has(series)).toBe(false);
    seenSeries.add(series);
    // Every sample belongs to a family that was declared before it, spelled
    // the way that family's type says it must be.
    expect(sampleNames.get(sample![1])).toBeDefined();
    samples++;
  }
  expect(samples).toBeGreaterThan(0);
  // Metadata only ever describes a family that exists.
  for (const name of helped) expect(families.has(name)).toBe(true);
  for (const name of united) expect(families.has(name)).toBe(true);
  return families;
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
    expect(text).toMatch(/^system_cpu_\S+ -?[\d.]+$/m);
    expect(text).toMatch(/^system_mem_\S+ -?[\d.]+$/m);
    // `%` reads as the word, which is the name the documentation has always
    // shown and the code did not produce until the renderer landed.
    expect(text).toMatch(/^system_mem_\S*_percent -?[\d.]+$/m);
  });

  it("serves an exposition a strict OpenMetrics parser accepts", async () => {
    // A grammar walk rather than a spot check: this is the test that fails on
    // a future regression nobody predicted, not just on the ones we know about.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics", "application/openmetrics-text;version=1.0.0"),
      (t) => /^system_cpu_/m.test(t),
    );

    const families = checkExposition(text, true);
    // The snapshot really did carry the metadata the producers declare, rather
    // than being well-formed by carrying none of it.
    expect([...families.values()]).toContain("counter");
    expect(text).toMatch(/^# HELP system_mem_/m);
    expect(text).toMatch(/^# UNIT \S+ bytes$/m);
  });

  it("serves the older text format a body its parser accepts too", async () => {
    // Everything that does not negotiate reads this one, and it is not the
    // same document: `# TYPE foo counter` there has to name the sample
    // `foo_total`, and `info` is a type its parser would reject outright.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics", "*/*"),
      (t) => /^system_cpu_/m.test(t),
    );

    const families = checkExposition(text, false);
    expect([...families.keys()].some((name) => name.endsWith("_total"))).toBe(true);
    expect([...families.values()]).not.toContain("info");
  });

  it("renders per-instance metrics as one labelled family", async () => {
    // Before labels every core was its own family (`system_cpu_core_0_idle`),
    // so `sum by (core)` had nothing to sum over, a Grafana variable had no
    // label to bind to, and the family names differed between two hosts with
    // different core counts. Now it is one family with a `core` label, and the
    // all-cores aggregate rides along as `core="total"`.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics"),
      (t) => /^system_cpu_idle_percent\{/m.test(t),
    );

    expect(text).toMatch(/^# TYPE system_cpu_idle_percent gauge$/m);
    expect(text).toMatch(/^system_cpu_idle_percent\{core="total"\} -?[\d.]+$/m);
    // A machine with one core still reports `core 0` alongside the aggregate.
    expect(text).toMatch(/^system_cpu_idle_percent\{core="0"\} -?[\d.]+$/m);

    // One `# TYPE` for however many cores this runner has, and every sample of
    // the family under it.
    expect(text.match(/^# TYPE system_cpu_idle_percent /gm) ?? []).toHaveLength(1);
    const samples = text.match(/^system_cpu_idle_percent\{core="[^"]*"\} /gm) ?? [];
    expect(samples.length).toBeGreaterThanOrEqual(2);

    // The label never carries the key's per-platform spelling: Linux publishes
    // `core_0` as the JSON key and Windows `core 0`, and neither may reach the
    // label, where it would make one core look like two across a fleet.
    expect(text).not.toMatch(/^system_cpu_idle_percent\{core="core/m);
  });

  it("labels the other per-instance producers too", async () => {
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics"),
      (t) => /^disk_free_total_bytes\{drive="/m.test(t) && /^system_network_/m.test(t),
    );

    // CheckDisk: one `disk_free_total_bytes` family with a `drive` label per
    // filesystem, rather than one family per drive letter or mount point.
    expect(text).toMatch(/^# TYPE disk_free_total_bytes gauge$/m);
    expect(text).toMatch(/^disk_free_total_bytes\{drive="[^"]+"\} \d+$/m);
    // And the network counters, whose family name used to embed a WMI adapter
    // description or a Linux interface name.
    const received = onWindows ? "system_network_BytesReceivedPersec" : "system_network_received";
    expect(text).toMatch(new RegExp(`^# TYPE ${received} gauge$`, "m"));
    expect(text).toMatch(new RegExp(`^${received}\\{nic="[^"]+"\\} -?[\\d.]+$`, "m"));
  });

  it("gives each instance its own info series", async () => {
    // A string metric becomes a label of its bundle's `_info` family, and which
    // series of that family it lands on is decided by the instance labels. So
    // one NIC's link state (and, on Windows, its MAC address and speed) share a
    // line, and the next NIC's get their own - rather than every NIC's strings
    // piling onto one series under label names like `eth0_status`.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics", "application/openmetrics-text;version=1.0.0"),
      (t) => /^system_network_info\{/m.test(t),
    );

    const series = text.match(/^system_network_info\{[^}]*\} 1$/gm) ?? [];
    expect(series.length).toBeGreaterThanOrEqual(1);
    for (const line of series) expect(line).toMatch(/\bnic="[^"]+"/);
    // The instance is a label, never part of a label's name.
    expect(text).not.toMatch(/^system_network_info\{[^}]*[a-z0-9]_status=/m);
  });

  it("keeps the flat JSON keys exactly where they were", async () => {
    // The contract that makes the label sweep safe to ship: labels are
    // additive, and `key` stays authoritative for everyone who reads keys - the
    // flat and nested JSON endpoints, the web UI dashboard, Graphite's carbon
    // path, collectd and Python. A label that moved a key would move a
    // dashboard on every upgraded host, silently.
    const metrics = await poll(
      () => getMetrics(key),
      (m) => Object.keys(m).some((k) => k.startsWith("system.cpu.")),
    );
    const keys = Object.keys(metrics);

    // The per-core CPU keys keep their platform spelling, instance and all.
    const core = onWindows ? "system.cpu.core 0." : "system.cpu.core_0.";
    expect(keys.some((k) => k.startsWith(core))).toBe(true);
    expect(keys).toContain("system.cpu.total.idle");
    // Per-drive and per-NIC keys keep the instance in the middle of the key.
    expect(keys.some((k) => /^disk\.free\..+\.free_pct$/.test(k))).toBe(true);
    expect(keys.some((k) => /^system\.network\..+\./.test(k))).toBe(true);
    // Nothing leaked the label syntax, or a unit suffix, into a key.
    expect(keys.filter((k) => k.includes("{") || k.includes("}"))).toEqual([]);
    expect(keys).not.toContain("system.mem.physical.total_bytes");
  });

  it("keeps full precision, so a sample equals the JSON value for the same key", async () => {
    // `str::xtos` truncated to six significant digits, which turned a 16 GB
    // memory reading into 1.6554e+10 on the scrape while the JSON endpoint
    // reported every byte.
    //
    // The pinned key is deliberate: the JSON and the text body are two
    // requests, so a metric that moves (anything `avail`, `used` or a
    // percentage) would differ whenever a metrics tick lands between them.
    // Installed physical memory is published on both platforms and does not
    // change while the test runs.
    const STABLE_KEY = "system.mem.physical.total";
    // The metric declares `bytes`, and OpenMetrics requires the name of a
    // family that declares a unit to end with it.
    const STABLE_NAME = "system_mem_physical_total_bytes";
    const metrics = await poll(
      () => getMetrics(key),
      (m) => typeof m[STABLE_KEY] === "number",
    );
    const value = metrics[STABLE_KEY] as number;
    expect(typeof value).toBe("number");
    expect(value).toBeGreaterThan(1e7);

    const text = await getText(key, "/api/v2/openmetrics");
    const sample = new RegExp(`^${STABLE_NAME} (\\S+)$`, "m").exec(text);
    expect(sample).not.toBeNull();
    expect(Number(sample![1])).toBe(value);
    // And it is written out in full rather than in scientific notation.
    expect(sample![1]).not.toMatch(/e/i);
  });

  it("negotiates the OpenMetrics content type", async () => {
    const om = await getTextWithType(
      key,
      "/api/v2/openmetrics",
      "application/openmetrics-text;version=1.0.0;q=0.75,text/plain;version=0.0.4;q=0.5",
    );
    expect(om.contentType).toBe("application/openmetrics-text; version=1.0.0; charset=utf-8");

    // Anything else gets the versioned Prometheus text type - not the bare
    // `text/plain` the endpoint used to fall back to, which tells a scraper
    // nothing about what it is reading.
    const plain = await getTextWithType(key, "/api/v2/openmetrics", "*/*");
    expect(plain.contentType).toBe("text/plain; version=0.0.4; charset=utf-8");

    // A gauge reads the same in both, so it is the part that can be compared
    // directly — asserted on its shape, not byte for byte, since these are two
    // round trips against a one-second metrics interval.
    for (const body of [om.text, plain.text]) {
      expect(body.endsWith("# EOF\n")).toBe(true);
      expect(body).toMatch(/^# TYPE system_mem_physical_total_bytes gauge$/m);
      expect(body).toMatch(/^system_mem_physical_total_bytes \d+$/m);
    }

    // A counter does not, and that is the whole reason the endpoint renders
    // two bodies: serving one of them to the other reader would leave every
    // counter untyped.
    expect(om.text).toMatch(/^# TYPE workers_jobs counter$/m);
    expect(plain.text).toMatch(/^# TYPE workers_jobs_total counter$/m);
    for (const body of [om.text, plain.text]) {
      expect(body).toMatch(/^workers_jobs_total \d+$/m);
    }
  });

  it("describes every built-in metric it exposes", async () => {
    // Before the metadata sweep a scrape was a list of names and numbers: no
    // `# HELP`, no `# UNIT`, and everything typed as a gauge whatever it was.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics", "application/openmetrics-text;version=1.0.0"),
      (t) => /^system_mem_/m.test(t),
    );

    // A byte count says so, and the family name ends in the unit it declares.
    expect(text).toMatch(/^# HELP system_mem_physical_used_bytes \S.*$/m);
    expect(text).toMatch(/^# TYPE system_mem_physical_used_bytes gauge$/m);
    expect(text).toMatch(/^# UNIT system_mem_physical_used_bytes bytes$/m);

    // A percentage keeps the name it had - the key already ended in `%`, which
    // sanitises to `_percent` - and now says what it is.
    expect(text).toMatch(/^# UNIT system_mem_physical_percent percent$/m);

    // Every family carries help text. The bundle's description is the fallback
    // for the per-core and per-NIC metrics that declare none of their own, so
    // there should be no family without one.
    const declared = text.split("\n").filter((l) => l.startsWith("# TYPE"));
    const helped = new Set(
      text
        .split("\n")
        .filter((l) => l.startsWith("# HELP"))
        .map((l) => l.split(" ")[2]),
    );
    const undescribed = declared
      .map((l) => l.split(" ")[2])
      // A PDH counter or a Python script names its own metrics and may
      // describe none of them; everything built in is expected to.
      .filter((name) => !name.startsWith("system_metrics_pdh_"))
      .filter((name) => !helped.has(name));
    expect(undescribed).toEqual([]);
  });

  it("types the agent's own monotonic counts as counters", async () => {
    // The core's `workers` bundle is always published, so this is the one
    // counter every platform and module set has. Typed as a gauge it cannot be
    // rate()d safely across a restart.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics", "application/openmetrics-text;version=1.0.0"),
      (t) => /^# TYPE workers_jobs counter$/m.test(t),
    );
    expect(text).toMatch(/^# TYPE workers_jobs counter$/m);
    expect(text).toMatch(/^# TYPE workers_submitted counter$/m);
    expect(text).toMatch(/^# TYPE workers_errors counter$/m);
    // The count itself is still a gauge: threads come and go.
    expect(text).toMatch(/^# TYPE workers_threads gauge$/m);

    // And the JSON view still carries them under the keys it always did - a
    // metric that changed type must not disappear from a dashboard.
    const metrics = await getMetrics(key);
    expect(typeof metrics["workers.jobs"]).toBe("number");
    expect(typeof metrics["workers.submitted"]).toBe("number");
    expect(typeof metrics["workers.errors"]).toBe("number");
  });

  it("exposes the string-valued metrics it used to drop, as an info family", async () => {
    // Uptime and boot time have no numeric sample, so the endpoint skipped
    // them entirely and a Prometheus user could not see them at all. They are
    // labels of an always-1 series now, the `node_uname_info` shape.
    const text = await poll(
      () => getText(key, "/api/v2/openmetrics", "application/openmetrics-text;version=1.0.0"),
      (t) => /^system_uptime_info\{/m.test(t),
    );
    expect(text).toMatch(/^# TYPE system_uptime info$/m);
    const sample = /^system_uptime_info\{(.*)\} 1$/m.exec(text);
    expect(sample).not.toBeNull();
    expect(sample![1]).toMatch(/uptime="[^"]+"/);
    expect(sample![1]).toMatch(/boot="[^"]+"/);

    // The same strings are still in the JSON view under their own keys.
    const metrics = await getMetrics(key);
    expect(typeof metrics["system.uptime.uptime"]).toBe("string");
    expect(typeof metrics["system.uptime.boot"]).toBe("string");
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
