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
    // `usage` is the renamed keyword (was `total`, which clashed with the
    // generic summary keyword); its perf keeps the historical _total suffix.
    const q = await executeQuery(key, "check_cpu", {
      warning: "usage > 101",
      critical: "usage > 101",
    });
    expect(q.result).toBe(OK);
    const load = perfValue(q, "total 5m_total");
    expect(load).toBeGreaterThanOrEqual(0);
    expect(load).toBeLessThanOrEqual(100);
  });

  it("check_cpu still accepts the deprecated total alias", async () => {
    const q = await executeQuery(key, "check_cpu", {
      warning: "total > 101",
      critical: "total > 101",
    });
    expect(q.result).toBe(OK);
    const load = perfValue(q, "total 5m_total");
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

  it("check_process reports cumulative CPU seconds without delta (time == kernel + user)", async () => {
    // Without delta the kernel/user/time keywords are cumulative CPU seconds and
    // need no collector. `time` must equal kernel + user — it used to always read
    // 0 because total_time was only ever populated by the delta path.
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "user=${user} kernel=${kernel} time=${time}",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    const m = /user=(\d+) kernel=(\d+) time=(\d+)/.exec(msg);
    expect(m).not.toBeNull();
    const [, user, kernel, time] = m!.map(Number);
    expect(time).toBe(user + kernel);
  });

  it("check_process rss is an alias for working_set", async () => {
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "rss=${rss} ws=${working_set}",
    });
    expect(q.result).toBe(OK);
    // Both render the same human-readable size (e.g. "20.309MB"); rss is a
    // straight alias for working_set, so the two must be identical.
    const m = /rss=(\S+) ws=(\S+)/.exec(messageOf(q));
    expect(m).not.toBeNull();
    expect(m![1]).toBe(m![2]);
  });

  it("check_process accepts 'running' as a synonym for 'started'", async () => {
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      warning: "state != 'running'",
      critical: "none",
      "top-syntax": "${list}",
      "detail-syntax": "${exe}=${state}",
    });
    expect(q.result).toBe(OK);
    // The rendered state is still "started" for back-compat.
    expect(messageOf(q)).toMatch(new RegExp(`${SELF_RE.source}=started`, "i"));
  });

  it("check_process resolve-owner=true populates username/uid (Windows)", async () => {
    if (!onWindows) return;
    // Owner resolution is opt-in. Scoped to our own process, so the lookup is a
    // single (local) account and fast. Default (no flag) leaves username empty.
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "resolve-owner": "true",
      "top-syntax": "${list}",
      "detail-syntax": "user=[${username}] uid=[${uid}]",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    // Our process has an owner; both the name and the SID should be non-empty.
    expect(msg).toMatch(/user=\[[^\]]+\]/);
    expect(msg).toMatch(/uid=\[S-\d[-\d]+\]/); // a SID string
  });

  it("check_process leaves username empty without resolve-owner (Windows)", async () => {
    if (!onWindows) return;
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "user=[${username}]",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/user=\[\]/); // owner not resolved by default
  });

  it("check_process resolve-owner=true populates username (Linux)", async () => {
    if (onWindows) return; // Windows resolves a SID; covered by its own test above.
    // uid needs no flag (it is read straight out of /proc/<pid>/status), the
    // name does because it goes through NSS.
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "resolve-owner": "true",
      "top-syntax": "${list}",
      "detail-syntax": "user=[${username}] uid=[${uid}]",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    expect(msg).toMatch(/user=\[[^\]]+\]/);
    expect(msg).toMatch(/uid=\[\d+\]/); // a numeric uid, not a SID
  });

  it("check_process reports a numeric uid without resolve-owner (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "user=[${username}] uid=[${uid}]",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    expect(msg).toMatch(/user=\[\]/); // name not resolved by default
    expect(msg).toMatch(/uid=\[\d+\]/); // ...but the uid is always there
  });

  it("check_process uid is numerically thresholdable (Linux)", async () => {
    if (onWindows) return;
    // The point of uid being an int rather than a string: `uid < 1000` selects
    // system accounts. Our own process is owned by whoever runs the suite, so
    // assert on the expression evaluating rather than on a specific verdict.
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      warning: "none",
      critical: "uid < 0", // never true: uid is always known for a live process
      "top-syntax": "${list}",
      "detail-syntax": "${exe} uid=${uid}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/uid=\d+/);
  });

  it("check_process exposes ppid (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "pid=${pid} ppid=${ppid}",
    });
    expect(q.result).toBe(OK);
    const m = /pid=(\d+) ppid=(\d+)/.exec(messageOf(q));
    expect(m).not.toBeNull();
    const [, pid, ppid] = m!.map(Number);
    expect(pid).toBeGreaterThan(0);
    // We were started by the test harness, so we have a real parent that is
    // not ourselves.
    expect(ppid).toBeGreaterThan(0);
    expect(ppid).not.toBe(pid);
  });

  it("check_process proc_state reports the raw Linux state (Linux)", async () => {
    if (onWindows) return;
    // A live process is in one of the running states; `state` stays "started".
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "state=${state} proc_state=${proc_state}",
    });
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    expect(msg).toMatch(/state=started/);
    expect(msg).toMatch(/proc_state=(running|sleeping|disk_sleep|idle)/);
  });

  it("check_process proc_state is usable in threshold expressions (Linux)", async () => {
    if (onWindows) return;
    // The canonical use: alert on zombies. We are not a zombie, so this must
    // come back OK — and it must parse, which is the real assertion (an
    // unknown state name would evaluate to `unknown` and never match).
    const q = await executeQuery(key, "check_process", {
      process: "*",
      warning: "none",
      critical: "proc_state = 'zombie' and exe = 'definitely-not-a-real-process'",
      "top-syntax": "${status}: ${count} processes",
    });
    expect(q.result).toBe(OK);
  });

  it("check_process exposes elapsed seconds since start (Linux)", async () => {
    if (onWindows) return;
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      "top-syntax": "${list}",
      "detail-syntax": "elapsed=${elapsed}",
    });
    expect(q.result).toBe(OK);
    const elapsed = Number(/elapsed=(\d+)/.exec(messageOf(q))?.[1]);
    // The instance has been up since beforeAll; a sane age is seconds to
    // minutes, and certainly less than a day.
    expect(elapsed).toBeGreaterThanOrEqual(0);
    expect(elapsed).toBeLessThan(86_400);
  });

  it("check_process delta=true reports CPU as a percentage (Linux)", async () => {
    // CheckSystemUnix computes the delta in-line (sample/sleep/sample); Windows
    // sources it from the background collector, covered in its own describe below.
    if (onWindows) return;
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

  it("check_process delta=true is UNKNOWN and names the setting when the collector is off (Windows)", async () => {
    // On Windows the CPU% for delta comes from the 'process cpu' collector,
    // which is NOT enabled on this instance. delta=true must fail fast telling
    // the user which flag to set — regardless of whether the CPU fields appear
    // in the syntax — rather than silently returning OK / cumulative seconds.
    if (!onWindows) return;
    const q = await executeQuery(key, "check_process", {
      process: SELF_EXE,
      delta: "true",
    });
    expect(q.result).toBe(UNKNOWN);
    expect(messageOf(q)).toMatch(/process cpu/i);
  });

  // Regression: `total` is a boolean option passed over REST as the token
  // `total=true`. It must be declared po::value<bool>()->implicit_value(true),
  // not po::bool_switch — the latter rejects a value with "option '--total'
  // does not take any arguments".
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
    // Which contract applies is decided by the empty-syntax message, not by
    // hunting for a perf label: the perf counter is named after the battery
    // (${name} - "BAT1", "BAT0", ...), so a machine that *has* one has no perf
    // key containing "charge" and used to be mistaken for a machine without.
    if (/no battery found/i.test(messageOf(q))) {
      // No (usable) battery on this machine (typical CI/VM): zero rows match,
      // so the pinned empty-state decides the result and the empty-syntax
      // renders the informative message.
      expect(q.result).toBe(OK);
      expect(Object.keys(perfOf(q))).toHaveLength(0);
      return;
    }
    // A battery is present: one perf counter per battery carrying its charge
    // percent, and the detail-syntax renders "<name>: <n>% (<source>, <status>)".
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    const perf = Object.entries(perfOf(q));
    expect(perf.length).toBeGreaterThan(0);
    for (const [label, entry] of perf) {
      expect(label).not.toEqual("");
      expect(entry.value).toBeGreaterThanOrEqual(0);
      expect(entry.value).toBeLessThanOrEqual(100);
    }
    expect(messageOf(q)).toMatch(/\d+% \(/);
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

  it("check_cpu_frequency exposes CPU inventory keywords (Windows)", async () => {
    if (!onWindows) return; // architecture/l2_cache/l3_cache come from Win32_Processor.
    const q = await pollQuery(
      key,
      "check_cpu_frequency",
      {
        warning: "l2_cache > 999999G",
        critical: "l3_cache > 999999G",
        "detail-syntax": "arch=${architecture} cores=${cores}/${logical_processors} l2=${l2_cache} l3=${l3_cache}",
      },
      (r) => r.result !== UNKNOWN,
      20_000,
    );
    if (q.result === UNKNOWN) {
      expect(messageOf(q)).toMatch(/no cpu frequency/i);
      return; // Same no-WMI-clock-data contract as the base test above.
    }
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    // Architecture always maps to a name; cache sizes render as sizes (0B on
    // VMs that do not report them).
    expect(msg).toMatch(/arch=(x86|x64|ARM64|ARM|ia64|unknown)/);
    expect(msg).toMatch(/cores=[1-9]\d*\/[1-9]\d*/);
    expect(msg).toMatch(/l2=\S+ l3=\S+/);
  });

  it("check_cpu_frequency load_pct is a sample or explicitly absent (Windows, #1391)", async () => {
    if (!onWindows) return; // load_pct comes from Win32_Processor.LoadPercentage.
    const q = await pollQuery(
      key,
      "check_cpu_frequency",
      {
        // LoadPercentage is 0-100 and a missing sample compares sure-false,
        // so these never trip: deterministic OK whenever there is data at all.
        warning: "load_pct > 100",
        critical: "load_pct > 100",
        "detail-syntax": "load=${load_pct}",
      },
      (r) => r.result !== UNKNOWN,
      20_000,
    );
    if (q.result === UNKNOWN) {
      expect(messageOf(q)).toMatch(/no cpu frequency/i);
      return; // Same no-WMI-clock-data contract as the base test above.
    }
    expect(q.result).toBe(OK);
    // A cycle where WMI has no LoadPercentage sample renders the explicit
    // no-value marker; it must never become a fabricated 0 or discard the
    // whole collection (#1391).
    expect(messageOf(q)).toMatch(/load=(\d{1,3}\b|no load sample)/);
  });

  // --- check_network ----------------------------------------------------------

  it("check_network lists at least one interface with throughput perf", async () => {
    const args = {
      warning: "throughput > 999999999999",
      critical: "throughput > 999999999999",
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

  // --- check_service summary (Windows) ---------------------------------------

  // `summary` is a new opt-in boolean (§4.6). Like every REST boolean it must be
  // declared po::value<bool>()->implicit_value(true), NOT po::bool_switch, or
  // REST's `summary=true` token is rejected with "does not take any arguments".
  it("check_service summary=true emits aggregate state-count perfdata (Windows)", async () => {
    if (!onWindows) return; // summary was added to the Windows CheckSystem check_service.
    const q = await executeQuery(key, "check_service", {
      summary: "true",
      filter: "none",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments/);
    const perf = perfOf(q);
    // The five rollup counters must be present and consistent.
    expect(perf["service_count"]).toBeDefined();
    expect(perf["running_services"]).toBeDefined();
    expect(perf["stopped_services"]).toBeDefined();
    expect(perf["paused_services"]).toBeDefined();
    expect(perf["pending_services"]).toBeDefined();
    const total = perf["service_count"].value as number;
    const parts =
      (perf["running_services"].value as number) +
      (perf["stopped_services"].value as number) +
      (perf["paused_services"].value as number) +
      (perf["pending_services"].value as number);
    expect(total).toBeGreaterThan(0);
    // Every service falls into one of the tallied buckets (or an untracked state),
    // so the parts can never exceed the total.
    expect(parts).toBeLessThanOrEqual(total);
  });

  // --- check_pending_reboot (Windows) ----------------------------------------

  it("check_pending_reboot returns one aggregate row with perf (Windows)", async () => {
    if (!onWindows) return; // check_pending_reboot is Windows-only (CheckSystem).
    // Pin thresholds off so the result is deterministic regardless of the host's
    // actual reboot state; here we exercise the command and its perfdata.
    const q = await executeQuery(key, "check_pending_reboot", {
      warning: "none",
      critical: "none",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/reboot/i);
    const perf = perfOf(q);
    expect(perf["reboot_pending"]).toBeDefined();
    expect(perf["reboot_signals"]).toBeDefined();
    // pending is a 0/1 flag; signals is the number of active signals
    // (the keyword was renamed from `count`, which clashed with the generic
    // summary keyword).
    expect([0, 1]).toContain(perf["reboot_pending"].value as number);
    expect(perf["reboot_signals"].value as number).toBeGreaterThanOrEqual(0);
  });

  it("check_pending_reboot accepts per-cause boolean expressions over REST (Windows)", async () => {
    if (!onWindows) return;
    // Regression: the bool keywords (servicing/windows_update/…) must parse in
    // warn/crit expressions and not be rejected as valueless boolean options.
    const q = await executeQuery(key, "check_pending_reboot", {
      warning: "none",
      critical: "servicing = 1 or windows_update = 1",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments/);
    expect(messageOf(q)).not.toMatch(/invalid|error/i);
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
  });

  it("check_pending_reboot accepts duration thresholds on age over REST (Windows)", async () => {
    if (!onWindows) return;
    // age is optional ('unknown' without a timestamped CBS/WU signal) and
    // duration-typed: `3650d` must parse as ten years, not the number 3650 —
    // a plain type_int keyword would silently read it as seconds and fire.
    // No real host has had a reboot pending for a decade, so the result is a
    // deterministic OK whatever this machine's actual reboot state.
    const q = await executeQuery(key, "check_pending_reboot", {
      warning: "none",
      critical: "pending = 1 and age > 3650d",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments/);
    expect(messageOf(q)).not.toMatch(/invalid|error/i);
    expect(q.result).toBe(OK);
  });

  // --- check_patch_age (Windows) ---------------------------------------------

  it("check_patch_age reports installed hotfixes with perf (Windows)", async () => {
    if (!onWindows) return; // check_patch_age is Windows-only (CheckSystem).
    // Default crit=missing>0 is inert without a hotfix= request, so a bare call
    // is OK and reports the aggregate.
    const q = await executeQuery(key, "check_patch_age", {});
    expect(q.result).toBe(OK);
    const perf = perfOf(q);
    // `patches` is the renamed keyword (was `count`, which clashed with the
    // generic summary keyword).
    expect(perf["patch_patches"]).toBeDefined();
    expect(perf["patch_age"]).toBeDefined();
    expect(perf["patch_missing"]).toBeDefined();
    // A real Windows host has at least one servicing hotfix and none missing.
    expect(perf["patch_patches"].value as number).toBeGreaterThan(0);
    expect(perf["patch_missing"].value as number).toBe(0);
  });

  it("check_patch_age is CRITICAL when a required hotfix is absent (Windows)", async () => {
    if (!onWindows) return;
    // KB0000001 cannot exist, so the presence check must flag it as missing.
    const q = await executeQuery(key, "check_patch_age", { hotfix: "KB0000001" });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/missing: KB0000001/);
    expect(perfOf(q)["patch_missing"].value as number).toBe(1);
  });

  // --- check_installed_software (both platforms) ------------------------------

  it("check_installed_software inventories installed packages with count perf", async () => {
    // Windows walks the registry Uninstall hives; Linux asks dpkg/rpm/pacman.
    // Any real host has at least one visible package, so a bare call is an
    // OK inventory with the aggregate count as perf.
    const q = await executeQuery(key, "check_installed_software", {});
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/software packages installed/i);
    expect(perfValue(q, "count")).toBeGreaterThan(0);
  });

  it("check_installed_software policy expressions parse and stay quiet when nothing matches", async () => {
    // The unwanted-software pattern (crit=name like ...) and the
    // recent-install pattern (warn=install_date > -Nd) passed as single k=v
    // tokens — REST-style argument parsing. Nothing can match either
    // expression (no product has this name; nothing installed within 1s),
    // so the result must be a clean OK.
    const q = await executeQuery(key, "check_installed_software", {
      warning: "install_date > -1s",
      critical: "name like 'zz_no_such_product_zz'",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|failed to parse|exception/i);
    expect(q.result).toBe(OK);
  });

  it("check_installed_software flags matching software as critical", async () => {
    // Inverted match: every installed package trips the expression, proving
    // that a hit is escalated and named in the problem list.
    const q = await executeQuery(key, "check_installed_software", {
      critical: "name not like 'zz_no_such_product_zz'",
    });
    expect(q.result).toBe(CRITICAL);
    expect(messageOf(q)).toMatch(/critical/i);
  });

  it("check_installed_software empty filter result takes the empty state", async () => {
    // The absent-unwanted-software probe: a filter matching nothing is OK
    // with the documented no-data message, never UNKNOWN or an error.
    const q = await executeQuery(key, "check_installed_software", {
      filter: "name like 'zz_no_such_product_zz'",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/no installed software found/i);
  });

  it("check_installed_software windows keywords filter machine-wide software (Windows)", async () => {
    if (!onWindows) return; // hive/architecture/system_component are registry-view concepts.
    const q = await executeQuery(key, "check_installed_software", {
      filter: "hive = 'machine' and system_component = 0",
    });
    expect(q.result).toBe(OK);
    expect(perfValue(q, "count")).toBeGreaterThan(0);
  });

  it("check_installed_software exposes the package manager keyword (Linux)", async () => {
    if (onWindows) return; // manager is the unix package-manager keyword.
    // Whatever manager owns this host, every entry carries the same value, so
    // filtering on the full known set must keep the inventory non-empty.
    const q = await executeQuery(key, "check_installed_software", {
      filter: "manager = 'dpkg' or manager = 'rpm' or manager = 'pacman'",
    });
    expect(q.result).toBe(OK);
    expect(perfValue(q, "count")).toBeGreaterThan(0);
  });

  // --- check_kernel_memory (both platforms) -------------------------------------

  it("check_kernel_memory reports kernel gauges and fault rates with perf", async () => {
    // Windows samples the PDH Memory counters; Linux reads /proc/meminfo and
    // /proc/vmstat. Both take a 1s window for the fault rates.
    const q = await executeQuery(key, "check_kernel_memory", {});
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/cache/i);
    const perf = perfOf(q);
    // The byte gauges are never zero on a live kernel.
    const gauge = onWindows ? "kernel_pool_nonpaged" : "kernel_slab_unreclaimable";
    expect(perf[gauge]).toBeDefined();
    expect(perf[gauge].value as number).toBeGreaterThan(0);
    expect(perf["kernel_cache"].value as number).toBeGreaterThan(0);
    expect(perf["kernel_page_faults_per_sec"]).toBeDefined();
  });

  it("check_kernel_memory size-unit and rate thresholds parse over REST", async () => {
    // Pool/slab thresholds use size units and the fault thresholds are rates,
    // all passed as single k=v tokens. Pinned so they can never trip.
    const gauge = onWindows ? "pool_nonpaged" : "slab_unreclaimable";
    const rate = onWindows ? "hard_faults_per_sec" : "major_faults_per_sec";
    const q = await executeQuery(key, "check_kernel_memory", {
      warning: `${gauge} > 999999G`,
      critical: `${rate} > 99999999`,
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|failed to parse|exception/i);
    expect(q.result).toBe(OK);
  });

  // --- check_hostname (both platforms) -----------------------------------------

  it("check_hostname reports host identity", async () => {
    const q = await executeQuery(key, "check_hostname", {
      "detail-syntax": "h=${hostname} f=${fqdn} c=${fqdn_consistent}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/h=\S+ f=\S+ c=\d/);
  });

  it("check_hostname pinned identity expressions parse and stay quiet", async () => {
    // The identity-pinning policy patterns as single k=v tokens. None can
    // match: no host answers to these sentinel names.
    const q = await executeQuery(key, "check_hostname", {
      warning: "hostname = 'zz_no_such_host_zz'",
      critical: "fqdn = 'zz_no_such_fqdn_zz' or domain = 'zz.example.invalid'",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|failed to parse|exception/i);
    expect(q.result).toBe(OK);
  });

  it("check_hostname exposes the domain-join state (Windows)", async () => {
    if (!onWindows) return; // join/join_name have no Linux equivalent.
    const q = await executeQuery(key, "check_hostname", {
      "detail-syntax": "join=${join} nb=${netbios_matches_dns}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/join=(domain|workgroup|standalone|unknown) nb=\d/);
  });

  // --- check_hardware (Windows) ------------------------------------------------

  it("check_hardware inventories the machine with memory perf (Windows)", async () => {
    if (!onWindows) return; // WMI-based hardware inventory is Windows-only.
    // Even a stripped-down VM answers at least one of the four classes, so a
    // bare call is an OK inventory line with memory/module perf.
    const q = await executeQuery(key, "check_hardware", {});
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/memory module/i);
    const perf = perfOf(q);
    expect(perf["hardware_memory"]).toBeDefined();
    expect(perf["hardware_modules"]).toBeDefined();
    expect(perf["hardware_modules"].value as number).toBeGreaterThanOrEqual(0);
  });

  it("check_hardware pinned-expectation expressions parse over REST (Windows)", async () => {
    if (!onWindows) return;
    // The serial-pinning / DIMM-drop policy patterns as single k=v tokens.
    // Neither can trip: the sentinel serial matches nothing and the module
    // count is never that large.
    const q = await executeQuery(key, "check_hardware", {
      warning: "modules > 99999",
      critical: "serial = 'zz_no_such_serial_zz' or chassis like 'zz_no_such_chassis'",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|failed to parse|exception/i);
    expect(q.result).toBe(OK);
  });

  // --- check_printqueue (Windows) --------------------------------------------

  it("check_printqueue runs and reports a valid status (Windows)", async () => {
    if (!onWindows) return; // print queues are a Windows feature.
    // A host with no printers takes empty-state=ok; hosts with idle printers are
    // also OK. With thresholds pinned off it must never be UNKNOWN/error.
    const q = await executeQuery(key, "check_printqueue", {
      warning: "none",
      critical: "none",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/printer|ok/i);
  });

  it("check_printqueue accepts the offline/error/age threshold keywords (Windows)", async () => {
    if (!onWindows) return;
    // Regression: the queue keywords must parse in warn/crit expressions and the
    // thresholds below never trip on a healthy/idle host, so the result is OK.
    const q = await executeQuery(key, "check_printqueue", {
      warning: "jobs > 999999 or oldest_job_age > 999999",
      critical: "error = 1 and offline = 1 and jobs > 999999",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|invalid|error parsing/i);
    expect(q.result).toBe(OK);
  });

  it("check_printqueue reports the driver and port of each queue (Windows)", async () => {
    if (!onWindows) return;
    // The device inventory is what tells "the queue is fine" from "the queue now
    // points at a different driver". Every Windows host has at least one queue
    // (Microsoft Print to PDF) with a driver and a port.
    const q = await executeQuery(key, "check_printqueue", {
      warning: "none",
      critical: "none",
      "detail-syntax": "${printer}|${driver}|${port}|${default}${shared}${network}",
      "top-syntax": "${list}",
    });
    expect(q.result).toBe(OK);
    // "<name>|<driver>|<port>|<3 flags>" — driver and port are never empty.
    expect(messageOf(q)).toMatch(/[^|]+\|[^|]+\|[^|]+\|[01][01][01]/);
  });

  it("check_printqueue filters on the new device keywords (Windows)", async () => {
    if (!onWindows) return;
    // Regression: the inventory keywords must be usable in filter/threshold
    // expressions, not just in the syntax strings.
    // Both syntaxes are set: a top syntax without ${list} lets the ok syntax win
    // while everything is fine, so the count would otherwise not be rendered.
    const q = await executeQuery(key, "check_printqueue", {
      filter: "driver like 'PDF' or port like 'PORTPROMPT'",
      warning: "none",
      critical: "none",
      "top-syntax": "matched ${count}",
      "ok-syntax": "matched ${count}",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|invalid|error parsing/i);
    // Microsoft Print to PDF matches on both halves of the expression, so the
    // filter has to have selected at least one queue.
    expect(messageOf(q)).toMatch(/^matched [1-9]\d*$/);
  });

  // --- check_printjobs (Windows) -----------------------------------------------

  it("check_printjobs reports the queued jobs or the empty contract (Windows)", async () => {
    if (!onWindows) return; // check_printjobs is Windows-only (CheckSystem).
    // A runner usually has an empty spooler, which is OK with the documented
    // message; when something is queued the row names it. Accept either, and
    // always expect the count perf so the queue depth can be graphed.
    const q = await executeQuery(key, "check_printjobs", { warning: "none", critical: "none" });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/No print jobs queued|by /);
    expect(perfOf(q)["count"]).toBeDefined();
  });

  it("check_printjobs exposes the per-job detail keywords (Windows)", async () => {
    if (!onWindows) return;
    // With an empty queue the detail syntax renders nothing, so this asserts the
    // keywords parse and, when a job is present, that each field is populated.
    const q = await executeQuery(key, "check_printjobs", {
      warning: "none",
      critical: "none",
      "detail-syntax": "id=${id} doc=[${document}] owner=[${owner}] status=${job_status} size=${size} pages=${pages}/${pages_printed} age=${age}",
      "top-syntax": "${list}",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|invalid|error parsing/i);
    if (/id=/.test(messageOf(q))) {
      expect(messageOf(q)).toMatch(/id=\d+ doc=\[.*\] owner=\[.*\] status=\S+ size=\d+ pages=\d+\/\d+ age=-?\d+/);
    }
  });

  it("check_printjobs accepts its status and size thresholds over REST (Windows)", async () => {
    if (!onWindows) return;
    // The status bits are valued booleans in expressions (the bool_switch trap)
    // and none of these can trip on a healthy/empty spooler. `size` is a
    // size-typed keyword, so the literal carries a unit.
    const q = await executeQuery(key, "check_printjobs", {
      warning: "size > 100G or pages > 999999 or age > 24h",
      critical: "error = 1 and blocked = 1 and user_intervention = 1",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|invalid|error parsing/i);
    expect(q.result).toBe(OK);
  });

  // --- check_w32time (Windows) -------------------------------------------------

  it("check_w32time reports the Windows Time state (Windows)", async () => {
    if (!onWindows) return; // check_w32time is Windows-only (CheckSystem).
    // The service is trigger-started on a workgroup client and always running on
    // a domain member, so pin the thresholds off and assert on the verdict text
    // rather than on a status that depends on the host's role.
    const q = await executeQuery(key, "check_w32time", { warning: "none", critical: "none" });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/Windows Time service|synchroniz/i);
  });

  it("check_w32time exposes the service, configuration and source keywords (Windows)", async () => {
    if (!onWindows) return;
    const q = await executeQuery(key, "check_w32time", {
      warning: "none",
      critical: "none",
      "detail-syntax": "svc=${service_state}/${start_type} type=${sync_type} from=${source_from} peers=${peer_count} run=${running} sync=${synchronized}",
      "top-syntax": "${list}",
    });
    // service_state and start_type come from the SCM, sync_type from the W32Time
    // registry, source_from says whether the source is live or the configured one.
    expect(messageOf(q)).toMatch(/svc=(running|stopped|starting|stopping|paused|not installed)\/\w+/);
    expect(messageOf(q)).toMatch(/type=(NT5DS|NTP|AllSync|NoSync|unknown)/);
    expect(messageOf(q)).toMatch(/from=(service|configuration|unknown)/);
    expect(messageOf(q)).toMatch(/peers=\d+ run=[01] sync=[01]/);
  });

  it("check_w32time renders unmeasured counters as unknown rather than a number (Windows)", async () => {
    if (!onWindows) return;
    // The "Windows Time Service" counters only carry data while the service is
    // running; when it is not, offset must read 'unknown' (and emit no perfdata)
    // instead of a fake zero or -1. Either outcome is valid on a given host.
    const q = await executeQuery(key, "check_w32time", {
      warning: "none",
      critical: "none",
      "detail-syntax": "offset=${offset}",
      "top-syntax": "${list}",
    });
    expect(messageOf(q)).toMatch(/^offset=(unknown|\d+)$/);
    if (/offset=unknown/.test(messageOf(q))) {
      expect(perfOf(q)["w32time_offset"]).toBeUndefined();
    }
  });

  it("check_w32time accepts its threshold keywords over REST (Windows)", async () => {
    if (!onWindows) return;
    // Regression: the keywords must parse in warn/crit expressions, including
    // the optional numbers, which compare false while they are unknown.
    const q = await executeQuery(key, "check_w32time", {
      warning: "offset > 999999",
      critical: "synchronized = 0 and running = 1 and local_clock = 1",
    });
    expect(messageOf(q)).not.toMatch(/does not take any arguments|invalid|error parsing/i);
    expect(q.result).toBe(OK);
  });

  it("check_w32time is CRITICAL when the machine follows no time source (Windows)", async () => {
    if (!onWindows) return;
    // The default critical is synchronized = 0; assert the two agree, whichever
    // way this host is configured, so the default is pinned to the real state.
    const state = await executeQuery(key, "check_w32time", {
      warning: "none",
      critical: "none",
      "detail-syntax": "sync=${synchronized}",
      "top-syntax": "${list}",
    });
    const synchronized = /sync=1/.test(messageOf(state));
    // Leave the default critical in place but silence the drift warning, so the
    // outcome depends only on whether the host follows a source.
    const q = await executeQuery(key, "check_w32time", { warning: "none" });
    expect(q.result).toBe(synchronized ? OK : CRITICAL);
  });

  // --- check_load (both platforms) ---------------------------------------------

  it("check_load reports the three load averages", async () => {
    // Linux reads /proc/loadavg; Windows synthesises the averages on the 1 Hz
    // collector tick (processor queue length + busy cores) and reports "not
    // available yet" until the first sample lands, so poll until it is OK.
    const q = await pollQuery(
      key,
      "check_load",
      { "detail-syntax": "l1=${load1} l5=${load5} l15=${load15} type=${type}" },
      (r) => r.result === OK,
    );
    expect(q.result).toBe(OK); // no default thresholds -> always OK
    expect(messageOf(q)).toMatch(/l1=[\d.]+ l5=[\d.]+ l15=[\d.]+ type=total/);
    // load1/5/15 perf is emitted.
    expect(Object.keys(perfOf(q)).some((k) => /load1/.test(k))).toBe(true);
  });

  it("check_load percpu reports the scaled per-core load", async () => {
    // percpu is a valued boolean over REST (the bool_switch trap).
    const q = await pollQuery(key, "check_load", { percpu: "true", "detail-syntax": "${type}" }, (r) => r.result === OK);
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/scaled/);
  });

  // --- Linux-only checks (no Windows CheckSystem equivalent) ------------------

  it("check_cpu_utilization exposes iowait/steal breakdown (Linux)", async () => {
    if (onWindows) return; // check_cpu_utilization is CheckSystemUnix-only.
    const q = await executeQuery(key, "check_cpu_utilization", {
      warning: "usage > 101", // never trips; just exercise the check
      critical: "usage > 101",
      "detail-syntax": "usage=${usage} iowait=${iowait} steal=${steal}",
    });
    expect(q.result).toBe(OK);
    expect(messageOf(q)).toMatch(/usage=[\d.]+ iowait=[\d.]+ steal=[\d.]+/);
    const usage = perfValue(q, "cpu_usage"); // perf label is "<perf-syntax>_<keyword>"
    expect(usage).toBeGreaterThanOrEqual(0);
    expect(usage).toBeLessThanOrEqual(100);
  });

  it("check_kernel_stats reports ctxt/processes/threads", async () => {
    // Linux reads /proc/stat; Windows samples the PDH System counters. Both
    // expose ctxt/processes/threads rows (Windows adds a syscalls row, and its
    // processes row is a gauge rather than a fork rate).
    const q = await executeQuery(key, "check_kernel_stats", {
      "detail-syntax": "${name}=${current}",
    });
    expect(q.result).toBeLessThanOrEqual(CRITICAL);
    const msg = messageOf(q);
    expect(msg).toMatch(/ctxt=\d+/);
    expect(msg).toMatch(/processes=\d+/);
    expect(msg).toMatch(/threads=[1-9]\d*/); // there is always at least one thread
  });

  it("check_kernel_stats type= selects a single metric", async () => {
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

// The Windows delta=true CPU path is backed by the 'process cpu' background
// collector. This runs in its own nscp instance (own describe) so the main
// suite can assert the collector-off contract while this one asserts the
// collector-on behaviour. Skipped entirely off Windows.
(onWindows ? describe : describe.skip)("CheckSystem check_process delta with the CPU collector (Windows)", () => {
  let nscp: NscpInstance;
  let key: string;

  beforeAll(async () => {
    nscp = new NscpInstance();
    key = await setupQueryNscp(nscp, "CheckSystem", {
      [SYSTEM_PATH]: { "process cpu": true },
    });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  it("reports CPU as a percentage when 'process cpu' is enabled", async () => {
    // The collector needs two 1 Hz samples before a delta exists. Even before
    // that a matched process reports 0%, so poll until the field is present and
    // numeric rather than asserting an exact value.
    const q = await pollQuery(
      key,
      "check_process",
      {
        process: SELF_EXE,
        delta: "true",
        "top-syntax": "${list}",
        "detail-syntax": "cpu=${time} user=${user} kernel=${kernel}",
      },
      (r) => r.result === OK && /cpu=\d+/.test(messageOf(r)),
    );
    expect(q.result).toBe(OK);
    const msg = messageOf(q);
    const cpu = Number(/cpu=(\d+)/.exec(msg)?.[1]);
    expect(cpu).toBeGreaterThanOrEqual(0);
    expect(cpu).toBeLessThanOrEqual(100);
    expect(msg).toMatch(/user=\d+ kernel=\d+/);
  });
});
