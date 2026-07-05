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
    // Default detail-syntax is "${os} (kernel ${kernel_release})" on Linux —
    // the distro pretty name plus the kernel release; on Windows it names the OS.
    expect(messageOf(q)).toMatch(onWindows ? /windows/i : /kernel/i);
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

  // Regression: `total` is a boolean option passed over REST as the token
  // `total=true`. It must be declared po::value<bool>()->implicit_value(true),
  // not po::bool_switch — the latter rejects a value with "option '--total'
  // does not take any arguments". See docs/design/icinga-windows-parity.md §4.2.
  it("check_process total=true accepts a valued boolean over REST", async () => {
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      total: "true",
      "top-syntax": "${total}",
      "detail-syntax": "${exe}",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments/);
    expect(q.result).toBe(OK);
  });

  // --- check_process_history -------------------------------------------------

  it("check_process_history tracks our process with times_seen=1", async () => {
    // Both platforms synthesise a seen=0 placeholder row for a requested but
    // not-yet-sampled process, so wait for a real sighting (seen >= 1). The
    // Linux collector samples /proc once per second; the Windows one only
    // refreshes process history every ~12s.
    const args = {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "${exe} seen=${times_seen} running=${running}",
    };
    const q = await pollQuery(
      key,
      "check_process_history",
      args,
      (r) => r.result === OK && new RegExp(`${SELF_RE.source} seen=[1-9]`, "i").test(messageOf(r)),
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
    // Windows serves this from the collector cache and the first WMI sweep
    // can take several seconds, during which the check reports the same
    // UNKNOWN as a machine without sensors — poll the warm-up out so a final
    // UNKNOWN is a definitive no-hardware answer. Linux reads /sys directly,
    // so the first (and only) query already settles it.
    const args = {
      warning: "temperature > 1000",
      critical: "temperature > 1000",
    };
    const q = await pollQuery(
      key,
      "check_temperature",
      args,
      (r) => r.result !== UNKNOWN,
      onWindows ? 20_000 : 1,
    );
    if (q.result === UNKNOWN) {
      expect(messageOf(q)).toMatch(/no temperature sensors/i);
      return; // No sensors (typical VM/WSL) — contract holds.
    }
    expect(q.result).toBe(OK);
    const temps = Object.values(perfOf(q)).map((p) => p.value as number);
    expect(temps.length).toBeGreaterThan(0);
    for (const t of temps) {
      expect(t).toBeGreaterThan(-60);
      expect(t).toBeLessThan(150);
    }
  });

  it("check_cpu_frequency reports clocks or UNKNOWN without cpufreq", async () => {
    // Perf entries are only generated for variables referenced by thresholds
    // and are keyed by the record's ${name}, so pin the thresholds to
    // current_mhz and find its perf entry via the MHz unit. Same Windows
    // collector warm-up dance as check_temperature above.
    const args = {
      warning: "current_mhz > 999999",
      critical: "current_mhz > 999999",
    };
    const q = await pollQuery(
      key,
      "check_cpu_frequency",
      args,
      (r) => r.result !== UNKNOWN,
      onWindows ? 20_000 : 1,
    );
    if (q.result === UNKNOWN) {
      expect(messageOf(q)).toMatch(/no cpu frequency/i);
      return; // No cpufreq/WMI clock data (typical VM/WSL) — contract holds.
    }
    expect(q.result).toBe(OK);
    const mhz = Object.values(perfOf(q)).find((p) => /mhz/i.test(String(p.unit ?? "")));
    expect(mhz).toBeDefined();
    expect(mhz!.value as number).toBeGreaterThan(0);
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

  // --- Linux-only checks (no Windows CheckSystem equivalent) ------------------

  it("check_load reports the three load averages (Linux)", async () => {
    if (onWindows) return; // check_load is CheckSystemUnix-only.
    const q = await executeQuery(key, "check_load", {
      "detail-syntax": "l1=${load1} l5=${load5} l15=${load15} type=${type}",
    });
    expect(q.result).toBe(OK); // no default thresholds -> always OK
    expect(messageOf(q)).toMatch(/l1=[\d.]+ l5=[\d.]+ l15=[\d.]+ type=total/);
    // load1/5/15 perf is emitted.
    expect(Object.keys(perfOf(q)).some((k) => /load1/.test(k))).toBe(true);
  });

  it("check_load percpu reports the scaled per-core load (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_load", { percpu: "true", "detail-syntax": "${type}" });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/scaled/);
  });

  it("check_cpu_utilization exposes iowait/steal breakdown (Linux)", async () => {
    if (onWindows) return; // check_cpu_utilization is CheckSystemUnix-only.
    const q = await executeQuery(key, "check_cpu_utilization", {
      warning: "total > 101", // never trips; just exercise the check
      critical: "total > 101",
      "detail-syntax": "total=${total} iowait=${iowait} steal=${steal}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/total=[\d.]+ iowait=[\d.]+ steal=[\d.]+/);
    const total = perfValue(q, "cpu_total"); // perf label is "<perf-syntax>_<keyword>"
    expect(total).toBeGreaterThanOrEqual(0);
    expect(total).toBeLessThanOrEqual(100);
  });

  it("check_kernel_stats reports ctxt/processes/threads (Linux)", async () => {
    if (onWindows) return; // check_kernel_stats is CheckSystemUnix-only.
    const q = await executeQuery(key, "check_kernel_stats", {
      "detail-syntax": "${name}=${current}",
    });
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    const msg = messageOf(q);
    expect(msg).toMatch(/ctxt=\d+/);
    expect(msg).toMatch(/processes=\d+/);
    expect(msg).toMatch(/threads=[1-9]\d*/); // there is always at least one thread
  });

  it("check_kernel_stats type= selects a single metric (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_kernel_stats", {
      type: "threads",
      "detail-syntax": "${name}",
    });
    expect(messageOf(q)).toMatch(/threads/);
    expect(messageOf(q)).not.toMatch(/ctxt/);
  });

  it("check_swap_io reports paging rates (Linux)", async () => {
    if (onWindows) return; // check_swap_io is CheckSystemUnix-only.
    const q = await executeQuery(key, "check_swap_io", {
      "detail-syntax": "in=${swap_in} out=${swap_out} count=${swap_count}",
    });
    expect(q.result).toBe(OK); // no default thresholds
    expect(messageOf(q)).toMatch(/in=[\d.]+ out=[\d.]+ count=\d+/);
  });

  it("check_os_version reports the distribution on Linux", async () => {
    if (onWindows) return; // distribution keywords are Linux-only.
    const q = await executeQuery(key, "check_os_version", {
      "detail-syntax":
        "os=${os}|distro=${distribution}|ver=${version}|fam=${family}|proc=${processor}",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    // ${os} and ${processor} used to render empty (the fixed bug); assert both
    // are now populated, plus the new distribution identity keywords.
    expect(msg).toMatch(/os=\S+/);
    expect(msg).not.toMatch(/os=\|/);
    expect(msg).toMatch(/distro=\w+/);
    expect(msg).toMatch(/proc=\w+/);
    expect(msg).not.toMatch(/proc=$/);
  });

  it("check_service maps systemd state and exposes process metrics (Linux)", async () => {
    if (onWindows) return; // Linux systemd semantics differ from Windows services.
    const q = await executeQuery(key, "check_service", {
      "top-syntax": "${list}",
      "detail-syntax": "${name}=${state}/${active} rss=${rss} tasks=${tasks}",
    });
    // Hosts without systemd (some CI containers) yield no services -> UNKNOWN.
    if (q.result === UNKNOWN) return;
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    const msg = messageOf(q);
    if (msg.includes("=")) {
      expect(msg).toMatch(/=(running|oneshot|static|starting|stopped|unknown)\//);
      expect(msg).toMatch(/rss=\d+ tasks=\d+/);
    }
  });
});
