/**
 * Exercises the CheckSystem module's check commands end-to-end on BOTH
 * platforms (modules/CheckSystem on Windows, modules/CheckSystemUnix — which
 * also registers as "CheckSystem" — on Linux). The point is command parity:
 * the same query with the same arguments must work on either OS.
 *
 * Queries run over the REST API against a long-lived `nscp test` process
 * because several checks (cpu, memory, network, process history) read from a
 * 1 Hz background collector and would return empty/zero data in one-shot
 * `nscp client --boot` mode.
 *
 * Hardware-dependent checks (battery, temperature, cpu frequency) assert the
 * documented no-hardware contract (OK "no battery" / UNKNOWN "no sensors")
 * when the machine lacks the hardware, and real values when it has it.
 */
import {
  CRITICAL,
  NscpInstance,
  OK,
  UNKNOWN,
  executeQuery,
  messageOf,
  perfOf,
  perfValue,
  pollQuery,
  setupQueryNscp,
} from "@fixtures/index";

jest.setTimeout(300_000);

const onWindows = process.platform === "win32";
/** Our own long-lived process — the one thing guaranteed to be running. */
const SELF_EXE = onWindows ? "nscp.exe" : "nscp";
const SELF_RE = /nscp(\.exe)?/i;
const SYSTEM_PATH = onWindows ? "/settings/system/windows" : "/settings/system/unix";

describe("CheckSystem commands", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckSystem", {
      // Both platforms gate check_process_history behind this collector flag.
      [SYSTEM_PATH]: { "process history": true },
    });
    // Warm the collector: memory totals come from its 1 Hz buffer on Linux.
    await pollQuery(key, "check_memory", {}, (q) => (perfValue(q, "physical") ?? 0) > 0);
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  // --- check_uptime / check_os_version --------------------------------------

  it("check_uptime reports a positive uptime", async () => {
    // Explicit thresholds: the defaults (warn < 2d) would trip on a
    // freshly-booted CI VM.
    const q = await executeQuery(key, "check_uptime", {
      warning: "uptime < 1s",
      critical: "uptime < 1s",
    });
    expect(q.result).toBe(OK);
    expect(perfValue(q, "uptime")).toBeGreaterThan(0);
  });

  it("check_os_version identifies the platform", async () => {
    const q = await executeQuery(key, "check_os_version", {});
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(onWindows ? /windows/i : /linux/i);
  });

  // --- check_cpu / check_memory / check_pagefile -----------------------------

  it("check_cpu reports collector-backed load per time window", async () => {
    const q = await executeQuery(key, "check_cpu", {
      warning: "load > 101",
      critical: "load > 101",
    });
    expect(q.result).toBe(OK);
    const load = perfValue(q, "total 5m");
    expect(load).toBeGreaterThanOrEqual(0);
    expect(load).toBeLessThanOrEqual(100);
  });

  it("check_memory reports a real physical total", async () => {
    const q = await executeQuery(key, "check_memory", {
      warning: "used > 100%",
      critical: "used > 100%",
    });
    expect(q.result).toBe(OK);
    expect(perfValue(q, "physical")).toBeGreaterThan(0);
  });

  it("check_pagefile executes and emits perf data", async () => {
    // No thresholds pinned: machines without swap legitimately report 0.
    const q = await executeQuery(key, "check_pagefile", {});
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });

  // --- check_process ---------------------------------------------------------

  it("check_process finds our own process running", async () => {
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "${exe}: ${state}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(new RegExp(`${SELF_RE.source}: started`, "i"));
  });

  it("check_process exposes page_fault, peak sizes and creation time", async () => {
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax":
        "pf=${page_fault} peak_ws=${peak_working_set} peak_virt=${peak_virtual} created=${creation}",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    expect(msg).toMatch(/pf=\d+/);
    // A running process has touched memory: peaks must be strictly positive.
    expect(msg).toMatch(/peak_ws=[1-9]\d*/);
    expect(msg).toMatch(/peak_virt=[1-9]\d*/);
    expect(msg).toMatch(/created=\S+/);
    expect(msg).not.toMatch(/created=0(\s|$)/);
  });

  it("check_process delta=true reports CPU as a percentage", async () => {
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      delta: "true",
      "top-syntax": "${list}",
      "detail-syntax": "cpu=${time} user=${user} kernel=${kernel}",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    const cpu = Number(/cpu=(\d+)/.exec(msg)?.[1]);
    expect(cpu).toBeGreaterThanOrEqual(0);
    expect(cpu).toBeLessThanOrEqual(100);
    expect(msg).toMatch(/user=\d+ kernel=\d+/);
  });

  // --- check_process_history -------------------------------------------------

  it("check_process_history tracks our process with times_seen=1", async () => {
    // The collector samples /proc (or the PDH snapshot) once per second;
    // poll until our own exe shows up in the history.
    const args = {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "${exe} seen=${times_seen} running=${running}",
    };
    const q = await pollQuery(
      key,
      "check_process_history",
      args,
      (r) => r.result === OK && new RegExp(`${SELF_RE.source} seen=`, "i").test(messageOf(r)),
    );
    expect(q.result).toBe(OK);
    // times_seen counts starts, not samples: a continuously-running process
    // must stay at exactly 1 no matter how long the collector has run.
    expect(messageOf(q)).toMatch(new RegExp(`${SELF_RE.source} seen=1 running=true`, "i"));
  });

  it("check_process_history_new sees our recently-started process", async () => {
    const q = await pollQuery(
      key,
      "check_process_history_new",
      { time: "1h", "top-syntax": "${list}", "detail-syntax": "${exe}" },
      (r) => new RegExp(SELF_RE.source, "i").test(messageOf(r)),
    );
    // nscp itself started seconds ago, so it must appear in the 1h window.
    expect(messageOf(q)).toMatch(SELF_RE);
    expect(q.result).toBe(OK);
  });

  // --- hardware-dependent checks ---------------------------------------------

  it("check_battery reports charge or the documented no-battery contract", async () => {
    // No battery flows through the filter's empty-state on both platforms
    // (default "warning"); pin it to OK so battery-less CI hosts are
    // deterministic AND the user-configurable empty-state is exercised.
    const q = await executeQuery(key, "check_battery", { "empty-state": "ok" });
    const charge = Object.entries(perfOf(q)).find(([k]) => /charge/i.test(k));
    if (!charge) {
      // No (usable) battery on this machine (typical CI/VM): zero rows match,
      // so the pinned empty-state decides the result and the empty-syntax
      // renders the informative message.
      expect(q.result).toBe(OK);
      expect(messageOf(q)).toMatch(/no battery found/i);
      return;
    }
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    expect(charge[1].value).toBeGreaterThanOrEqual(0);
    expect(charge[1].value).toBeLessThanOrEqual(100);
  });

  it("check_temperature reports sensors or UNKNOWN without hardware", async () => {
    const q = await executeQuery(key, "check_temperature", {
      warning: "temperature > 1000",
      critical: "temperature > 1000",
    });
    if (q.result === UNKNOWN) {
      expect(messageOf(q)).toMatch(/no temperature sensors/i);
      return; // Linux without sensors (typical VM/WSL) — contract holds.
    }
    expect(q.result).toBe(OK);
    const temps = Object.values(perfOf(q)).map((p) => p.value as number);
    if (onWindows && temps.length === 0) {
      return; // Windows without WMI thermal data reports OK with zero zones.
    }
    expect(temps.length).toBeGreaterThan(0);
    for (const t of temps) {
      expect(t).toBeGreaterThan(-60);
      expect(t).toBeLessThan(150);
    }
  });

  it("check_cpu_frequency reports clocks or UNKNOWN without cpufreq", async () => {
    const q = await executeQuery(key, "check_cpu_frequency", {});
    if (q.result === UNKNOWN) {
      expect(messageOf(q)).toMatch(/no cpu frequency/i);
      return; // No cpufreq/WMI clock data (typical VM/WSL) — contract holds.
    }
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    const mhz = Object.entries(perfOf(q)).find(([k]) => /mhz|frequency/i.test(k));
    expect(mhz).toBeDefined();
  });

  // --- check_network ----------------------------------------------------------

  it("check_network lists at least one interface with throughput perf", async () => {
    const args = {
      warning: "total > 999999999999",
      critical: "total > 999999999999",
    };
    // Both platforms serve this from the background collector: Linux returns
    // UNKNOWN before its first sample while Windows renders an empty OK, so
    // poll until interface perf data actually appears.
    const q = await pollQuery(
      key,
      "check_network",
      args,
      (r) => r.result !== UNKNOWN && Object.keys(perfOf(r)).length > 0,
    );
    expect(q.result).toBe(OK);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });
});
