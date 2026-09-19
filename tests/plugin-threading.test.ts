/**
 * The threading contract the core promises every module.
 *
 * A check runs on whichever thread the transport handed it, and the core is
 * expected to stay out of the way: two callers must be able to be inside the
 * same module at once, a slow module must not stall an unrelated one, and a
 * module that dispatches back into itself must not wait on a lock it is
 * already holding. Those guarantees are invisible to a single-threaded test -
 * every check suite in this directory passes just as happily against a core
 * that serialises everything - so they get their own suite here.
 *
 * Transport matters. The web server runs its handlers on one I/O thread
 * (ServerBeastImpl.cpp: a single `ioc_.run()`), so concurrent REST requests
 * queue up in the HTTP layer and would measure the web server rather than the
 * dispatch path. NRPE has a real worker pool (`thread pool`, default 10), so
 * everything below goes over NRPE, driven by `nscp nrpe` - the client alias
 * built into the same binary, which is present wherever nscp is installed
 * (`check_nscp_nrpe` is a build-tree-only artifact and is not packaged).
 *
 * The blocking half of the suite needs a check that really parks its dispatch
 * thread, which here is an external script. It is written in Python so the same
 * script runs on every platform the suite does: both CI images already provide
 * an interpreter (`python3` on the Linux containers, setup-python on Windows).
 * The interpreter is registered by its absolute path rather than by name,
 * because the launcher calls CreateProcess with lpApplicationName, which does
 * no PATH search; paths are written with forward slashes because the command
 * tokeniser treats a backslash as an escape character and falls back to the
 * legacy single-string form when it sees one.
 *
 * Overlap is asserted from the agent's side, not from the client's clock: the
 * slow script appends a marker when it starts and another when it finishes, so
 * "these two ran at the same time" is a fact recorded by the checks themselves
 * rather than an inference from wall-clock timing on a loaded CI runner. The
 * elapsed-time assertions are kept as a loose backstop only.
 */
import { spawnSync } from "child_process";
import * as fs from "fs";
import * as path from "path";

import execa from "execa";

import { hasModule, NscpInstance } from "@fixtures/index";

jest.setTimeout(300_000);

/** NRPE port for this suite; kept off the other suites' ports. */
const NRPE_PORT = 15666;

/** How long the slow check blocks for, in milliseconds. */
const SLOW_MS = 2000;

/**
 * Absolute path to a Python interpreter, or null if there is none. Asked for
 * as `sys.executable` rather than assumed from the platform, so the agent gets
 * a path it can hand straight to CreateProcess / execv.
 */
function findPython(): string | null {
  for (const candidate of ["python3", "python"]) {
    const r = spawnSync(candidate, ["-c", "import sys; print(sys.executable)"], {
      encoding: "utf8",
    });
    if (!r.error && r.status === 0) {
      const exe = (r.stdout ?? "").trim();
      if (exe) return exe.replace(/\\/g, "/");
    }
  }
  return null;
}

const python = findPython();

/** The cases that need the external slow script. */
const itScript = python ? it : it.skip;

/**
 * PythonScript is an optional module - it is built only where Boost.Python and
 * libpython were found - so the scripted-check cases below ask for it rather
 * than assuming a package that ships it.
 */
const hasPythonScript = hasModule("PythonScript");
const itPy = hasPythonScript ? it : it.skip;

describe("plugin threading", () => {
  let nscp: NscpInstance;
  let scriptDir: string;
  let traceDir: string;

  /** `nscp nrpe --command <cmd>` against our own agent. Never throws: a
   * failure has to reach the assertion as text rather than as a rejected
   * promise, or a broken agent shows up as an unhelpful timeout. */
  async function nrpe(command: string, args: string[] = []): Promise<string> {
    const extra: string[] = [];
    for (const a of args) extra.push("--argument", a);
    const res = await execa(
      process.env.NSCP_BIN as string,
      [
        "nrpe",
        "--host",
        "127.0.0.1",
        "--port",
        String(NRPE_PORT),
        "--command",
        command,
        "--ssl",
        "false",
        "--timeout",
        "60",
        ...extra,
      ],
      { cwd: nscp.workDir, reject: false, all: true, timeout: 120_000, env: process.env },
    );
    return (res.all ?? "").trim();
  }

  /**
   * The trace is a directory, one file per event, not a shared append-only
   * log. Several checks record at once, and concurrent appends to one file are
   * only atomic where O_APPEND is - which Windows is not: the CRT's append
   * mode seeks to the end and then writes, so two writers overwrite each
   * other. That cost real markers on the Windows runners (four callers, two
   * lines) while Linux passed every time. Creating a distinct file per event
   * has no shared position to race over on either platform.
   *
   * Each check writes `<uid>.enter` when it starts and `<uid>.exit` when it
   * finishes, both containing a monotonic-ish timestamp in seconds.
   */
  interface TraceEvent {
    uid: string;
    kind: "enter" | "exit";
    at: number;
  }

  function readEvents(): TraceEvent[] {
    if (!fs.existsSync(traceDir)) return [];
    const events: TraceEvent[] = [];
    for (const name of fs.readdirSync(traceDir)) {
      const m = /^(.+)\.(enter|exit)$/.exec(name);
      if (!m) continue;
      const raw = fs.readFileSync(path.join(traceDir, name), "utf8").trim();
      const at = Number.parseFloat(raw);
      if (!Number.isFinite(at)) continue;
      events.push({ uid: m[1], kind: m[2] as "enter" | "exit", at });
    }
    return events;
  }

  const entered = (): number => readEvents().filter((e) => e.kind === "enter").length;
  const exited = (): number => readEvents().filter((e) => e.kind === "exit").length;

  function resetTrace(): void {
    fs.rmSync(traceDir, { recursive: true, force: true });
    fs.mkdirSync(traceDir, { recursive: true });
  }

  /**
   * Highest number of checks that were inside the agent at the same moment.
   * Pairs each enter with its exit and sweeps the resulting intervals, so it
   * reads real overlap rather than the order lines happened to land in. 1
   * means they were serialised.
   */
  function peakOverlap(events: TraceEvent[]): number {
    const starts = new Map<string, number>();
    for (const e of events) if (e.kind === "enter") starts.set(e.uid, e.at);

    const edges: Array<{ at: number; delta: number }> = [];
    for (const e of events) {
      if (e.kind === "enter") {
        edges.push({ at: e.at, delta: 1 });
      } else if (starts.has(e.uid)) {
        edges.push({ at: e.at, delta: -1 });
      }
    }
    // Close before opening at the same instant, so two checks that merely
    // abut are not counted as overlapping.
    edges.sort((a, b) => a.at - b.at || a.delta - b.delta);

    let depth = 0;
    let peak = 0;
    for (const edge of edges) {
      depth += edge.delta;
      peak = Math.max(peak, depth);
    }
    return peak;
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    scriptDir = nscp.scratch("threading");
    traceDir = path.join(scriptDir, "trace");
    resetTrace();

    // A check that genuinely blocks its dispatch thread: CheckExternalScripts
    // waits on the child process, so there is no cooperative yielding hiding
    // a serialised core. Each marker is one short line appended to a file
    // opened in append mode, which both platforms write atomically.
    const slowScript = path.join(scriptDir, "slow.py");
    // JSON.stringify produces a correctly escaped literal for Python too, so
    // a Windows path survives whichever separator it carries.
    const traceLiteral = JSON.stringify(traceDir);
    fs.writeFileSync(
      slowScript,
      [
        "import os, sys, time, uuid",
        "",
        `TRACE_DIR = ${traceLiteral}`,
        "UID = uuid.uuid4().hex",
        "",
        "def mark(kind):",
        "    # One file per event: appending to a shared log is not atomic on",
        "    # Windows and markers went missing there. See the test's helpers.",
        '    p = os.path.join(TRACE_DIR, "%s.%s" % (UID, kind))',
        '    with open(p, "w") as f:',
        '        f.write("%.6f" % time.time())',
        "",
        'mark("enter")',
        `time.sleep(${SLOW_MS / 1000})`,
        'mark("exit")',
        'print("OK: slow done")',
        "sys.exit(0)",
        "",
      ].join("\n"),
      { mode: 0o755 },
    );

    // Two Lua commands that call back into the core. `lua_nested` is the
    // re-entrancy case that matters: LUAScript asks the core for a command
    // that LUAScript itself serves, so the module is dispatched into while it
    // is already inside a dispatch on the same thread.
    const luaScript = path.join(scriptDir, "threading.lua");
    fs.writeFileSync(
      luaScript,
      [
        "local core = Core()",
        "",
        "-- Pure CPU, no core calls and no nscp.sleep, so this never gives up",
        "-- the Lua lock: several of these at once must still each get their",
        "-- own consistent view of the interpreter.",
        "local function busy(command, args)",
        "  local n = 0",
        "  for i = 1, 2000000 do n = n + i end",
        "  return 'ok', 'busy done ' .. tostring(n)",
        "end",
        "",
        "local function inner(command, args)",
        "  return 'ok', 'inner reached'",
        "end",
        "",
        "-- LUAScript querying a command LUAScript serves: core -> module ->",
        "-- core -> same module, all on one thread.",
        "local function nested(command, args)",
        "  local code, msg, perf = core:simple_query('lua_inner', {})",
        "  return 'ok', 'nested saw: ' .. tostring(msg)",
        "end",
        "",
        "-- Two levels of the same thing.",
        "local function deep(command, args)",
        "  local code, msg, perf = core:simple_query('lua_nested', {})",
        "  return 'ok', 'deep saw: ' .. tostring(msg)",
        "end",
        "",
        "local reg = Registry()",
        "reg:simple_function('lua_busy', busy, 'cpu work, never releases the lua lock')",
        "reg:simple_function('lua_inner', inner, 'innermost self-query target')",
        "reg:simple_function('lua_nested', nested, 'queries a command its own module serves')",
        "reg:simple_function('lua_deep', deep, 'two levels of self-query')",
        "",
      ].join("\n"),
    );

    // The PythonScript twin of the Lua script above. Unlike Lua, CPython gives
    // every OS thread its own interpreter state, so concurrent invocations that
    // release the GIL around a core call do not share an execution stack.
    const pyScript = path.join(scriptDir, "threading.py");
    fs.writeFileSync(
      pyScript,
      [
        "from NSCP import Registry, Core, status, sleep",
        "",
        "import os, threading, time, uuid",
        "",
        "plugin_id = 0",
        "",
        `TRACE_DIR = ${JSON.stringify(traceDir)}`,
        "",
        "def mark(uid, kind):",
        "    # One file per event; a shared append-only log loses markers on",
        "    # Windows. See the test's trace helpers.",
        '    p = os.path.join(TRACE_DIR, "%s.%s" % (uid, kind))',
        '    with open(p, "w") as f:',
        '        f.write("%.6f" % time.time())',
        "",
        "def inner(arguments):",
        "    return (status.OK, 'inner reached')",
        "",
        "def nested(arguments):",
        "    core = Core.get(plugin_id)",
        "    # Held across the core call: if another thread could run on this",
        "    # frame's state, this local is what would come back wrong.",
        "    sentinel = 'sentinel-%d' % len(arguments)",
        "    (code, msg, perf) = core.simple_query('py_inner', [])",
        "    return (status.OK, 'nested saw: %s [%s]' % (msg, sentinel))",
        "",
        "def native(arguments):",
        "    # ctypes is a C extension: importing it resolves libpython's",
        "    # symbols, which only works when the module dlopen'd libpython",
        "    # RTLD_GLOBAL. That depends on the 'python lib' setting having",
        "    # been read before Py_Initialize ran.",
        "    import ctypes",
        "    return (status.OK, 'native ok %d' % ctypes.sizeof(ctypes.c_int))",
        "",
        "def sleeper(arguments):",
        "    # NSCP.sleep parks the thread with the GIL released",
        "    # (thread_unlocker -> PyEval_SaveThread), so other threads must",
        "    # be able to run Python while this one sits here.",
        "    uid = uuid.uuid4().hex",
        '    mark(uid, "enter")',
        `    sleep(${SLOW_MS})`,
        '    mark(uid, "exit")',
        "    return (status.OK, 'slept')",
        "",
        "def init(pid, plugin_alias, script_alias):",
        "    global plugin_id",
        "    plugin_id = pid",
        "    reg = Registry.get(plugin_id)",
        "    reg.simple_function('py_inner', inner, 'innermost self-query target')",
        "    reg.simple_function('py_nested', nested, 'queries a command its own module serves')",
        "    reg.simple_function('py_native', native, 'imports a C extension module')",
        "    reg.simple_function('py_sleep', sleeper, 'parks with the GIL released')",
        "",
      ].join("\n"),
    );

    await nscp.configure({
      "/modules": {
        NRPEServer: "enabled",
        CheckExternalScripts: "enabled",
        CheckHelpers: "enabled",
        LUAScript: "enabled",
        ...(hasPythonScript ? { PythonScript: "enabled" } : {}),
      },
      "/settings/NRPE/server": {
        port: NRPE_PORT,
        "allow arguments": "true",
        // Plaintext: this suite is about dispatch threading, and the TLS
        // handshake is covered by nrpe-tls.test.ts.
        "use ssl": "false",
      },
      // Absolute interpreter, forward slashes: see the header note on
      // lpApplicationName and the backslash-escaping tokeniser.
      ...(python
        ? {
            "/settings/external scripts/scripts": {
              slow: `${python} ${slowScript.replace(/\\/g, "/")}`,
            },
          }
        : {}),
      // The generated scripts live in this test's scratch directory, not under
      // ${scripts}, and a configured script has to sit inside an allowed root
      // to load. Naming the scratch directory is the supported way to run a
      // script the agent does not own, and is what an operator does for a
      // vendor plugin under /usr/lib/nagios/plugins.
      "/settings/lua": {
        "additional script roots": scriptDir,
      },
      "/settings/lua/scripts": {
        threading: luaScript,
      },
      ...(hasPythonScript
        ? {
            "/settings/python": { "additional script roots": scriptDir },
            "/settings/python/scripts": { threading: pyScript },
          }
        : {}),
    });

    await nscp.waitForPortFree(NRPE_PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(NRPE_PORT, { timeoutMs: 60_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  itScript("runs one slow check per caller without serialising them", async () => {
    resetTrace();
    const callers = 4;

    const started = Date.now();
    const results = await Promise.all(Array.from({ length: callers }, () => nrpe("slow")));
    const elapsed = Date.now() - started;

    for (const r of results) expect(r).toContain("slow done");

    const trace = readEvents();
    const peak = peakOverlap(trace);

    // A failure here is almost always one CI-only observation, so make sure
    // the evidence reaches the log rather than just the expected/received.
    const serialisedMs = callers * SLOW_MS;
    const budgetMs = serialisedMs * 0.75;
    if (peak <= 1 || elapsed >= budgetMs) {
      console.error(`peak=${peak} elapsed=${elapsed}ms events=${trace.length}`);
    }

    expect(trace.filter((e) => e.kind === "enter")).toHaveLength(callers);
    expect(trace.filter((e) => e.kind === "exit")).toHaveLength(callers);

    // The point of the suite: the agent had more than one of them inside it
    // at the same time. A core that dispatched under a single lock scores 1
    // here however fast the machine is.
    expect(peak).toBeGreaterThan(1);

    // Backstop on the clock, deliberately loose. A busy runner can let one
    // caller arrive after the others have finished, costing a second round
    // (~2 * SLOW_MS); a core that truly serialises needs all four
    // (~4 * SLOW_MS). The budget sits between the two so a straggler stays
    // green and a serialising core cannot.
    expect(elapsed).toBeLessThan(budgetMs);
  });

  itScript("keeps a different module answering while one module is blocked", async () => {
    resetTrace();

    // Live counts straight off the trace directory: one file per event, so a
    // reader never sees a half-written shared log.
    const hasEntered = () => entered() > 0;
    const exits = () => exited();

    // Hold CheckExternalScripts busy, then ask CheckHelpers for something
    // trivial. If a slow check could block the whole dispatch path, no fast
    // check could come back until the slow one had.
    const slow = nrpe("slow");

    // Wait for the slow check to actually be inside the agent rather than
    // assuming a fixed sleep was long enough on this machine.
    const readyBy = Date.now() + 30_000;
    while (!hasEntered() && Date.now() < readyBy) {
      await new Promise((r) => setTimeout(r, 25));
    }
    expect(hasEntered()).toBe(true);

    // Count the fast checks that demonstrably came back while the slow one was
    // still inside the agent: its entry marker written, its exit marker not.
    //
    // Read from the trace rather than compared as clocks. Two durations taken
    // from the same origin can tie at millisecond resolution, and a slow runner
    // can spend most of SLOW_MS on three NRPE round trips - which is exactly
    // how this failed on the arm64 runner, at "2046 < 2046". Stopping as soon
    // as the slow check exits also makes the loop adapt to the machine instead
    // of assuming a fixed count fits in the window.
    let servedWhileBlocked = 0;
    const deadline = Date.now() + 60_000;
    while (exits() === 0 && servedWhileBlocked < 3 && Date.now() < deadline) {
      const out = await nrpe("check_ok", ["message=fast"]);
      expect(out).toContain("fast");
      if (exits() === 0) servedWhileBlocked++;
    }

    expect(servedWhileBlocked).toBeGreaterThan(0);
    expect(await slow).toContain("slow done");
    expect(exits()).toBe(1);
  });

  it("lets a module dispatch into itself", async () => {
    // CheckHelpers' check_multi runs sub-checks through the core, so
    // CheckHelpers is re-entered while it is already handling a command on
    // this thread. A non-reentrant dispatch lock deadlocks here.
    const out = await nrpe("check_multi", [
      "command=check_ok message=one",
      "command=check_ok message=two",
    ]);
    expect(out).toContain("one");
    expect(out).toContain("two");
  });

  it("lets a script query a command its own module serves", async () => {
    // The Lua twin of the above, and the sharper case: the module holds its
    // own script-dispatch bookkeeping across the call, so re-entering it has
    // to be allowed explicitly rather than by luck.
    const once = await nrpe("lua_nested");
    expect(once).toContain("nested saw: inner reached");

    const twice = await nrpe("lua_deep");
    expect(twice).toContain("deep saw: nested saw: inner reached");
  });

  it("serves concurrent scripted checks without crossing their state", async () => {
    // Several Lua checks at once. These never release the interpreter lock,
    // so they are expected to take turns - what must not happen is one of
    // them observing another's state, which shows up as a wrong sum.
    const expected = "busy done " + ((2000000 * 2000001) / 2).toString();
    const results = await Promise.all(Array.from({ length: 4 }, () => nrpe("lua_busy")));
    for (const r of results) expect(r).toContain(expected);
  });

  itPy("boots with a python script configured", async () => {
    // Regression guard. PythonScript used to call settings.notify() - which
    // constructs every configured script, and the constructor takes the GIL -
    // before Py_Initialize, so any agent with a python script segfaulted on
    // boot. Nothing caught it because no suite enabled the module with a
    // script. Reaching this assertion at all means the agent came up.
    expect(await nrpe("py_inner")).toContain("inner reached");
  });

  itPy("can import a C extension module from a python script", async () => {
    // Regression guard for the other half of the interpreter bring-up: the
    // "python lib" setting has to be read before Py_Initialize, or the
    // RTLD_GLOBAL dlopen is skipped and every C extension module fails with
    // "undefined symbol: PyTuple_Type" - which takes protobuf, and so the
    // bundled scripts, down with it. A pure-Python script never notices.
    expect(await nrpe("py_native")).toContain("native ok");
  });

  itPy("runs concurrent python checks that release the GIL while parked", async () => {
    // NSCP.sleep drops the GIL for the duration (thread_unlocker wraps
    // PyEval_SaveThread), so three of these must sit inside the agent at the
    // same time rather than taking turns - and an unrelated module must stay
    // answerable throughout.
    //
    // Worth stating what this does NOT depend on: the reload barrier in
    // dll_plugin only makes a dispatch wait while loadModuleEx is running on
    // the module. No reload happens here, so dispatch costs nothing, and the
    // GIL is the only thing being contended.
    resetTrace();
    const callers = 3;

    const started = Date.now();
    const inFlight = Array.from({ length: callers }, () => nrpe("py_sleep"));

    // An unrelated module while all three are parked.
    await new Promise((r) => setTimeout(r, SLOW_MS / 4));
    expect(await nrpe("check_ok", ["message=awake"])).toContain("awake");

    for (const r of await Promise.all(inFlight)) expect(r).toContain("slept");
    const elapsed = Date.now() - started;

    const trace = readEvents();
    const peak = peakOverlap(trace);
    if (peak <= 1) {
      console.error(`peak=${peak} elapsed=${elapsed}ms events=${trace.length}`);
    }

    expect(trace.filter((e) => e.kind === "enter")).toHaveLength(callers);
    expect(trace.filter((e) => e.kind === "exit")).toHaveLength(callers);

    // The point: more than one Python check was inside at once. A GIL held
    // across the sleep would score 1 here however fast the machine is.
    expect(peak).toBeGreaterThan(1);
    expect(elapsed).toBeLessThan(callers * SLOW_MS * 0.75);
  });

  itPy("lets a python script query a command its own module serves", async () => {
    expect(await nrpe("py_nested")).toContain("nested saw: inner reached");
  });

  itPy("serves concurrent python checks that call back into the core", async () => {
    // The case Lua cannot pass (see the skip below). CPython gives each OS
    // thread its own interpreter state, so releasing the GIL around the core
    // call does not hand another thread this frame's stack: every caller must
    // get its own sentinel back, and the agent must survive.
    const rounds = 3;
    for (let i = 0; i < rounds; i++) {
      const results = await Promise.all(Array.from({ length: 6 }, () => nrpe("py_nested")));
      for (const r of results) expect(r).toContain("nested saw: inner reached [sentinel-0]");
    }
    expect(await nrpe("check_ok", ["message=still-alive"])).toContain("still-alive");
  });

  // KNOWN DEFECT - skipped because it kills the agent, not because it is
  // flaky. Two callers running the same Lua script at once, where the script
  // calls back into the core, corrupt the interpreter and the process dies
  // with SIGSEGV (or a Lua PANIC -> abort). Reproduced on this branch and on
  // an unmodified main, so it predates the concurrency fixes; four concurrent
  // `lua_nested` calls took three rounds to bring the agent down.
  //
  // Mechanism: lua_core.hpp's prep_function hands out `information->
  // user_data.L`, i.e. one lua_State per *script*, shared by every concurrent
  // invocation of every function in it. lua_script.cpp drops the interpreter
  // lock around core calls (`lua::lua_gil::release` in the simple_query
  // binding), so while one thread is inside the core a second thread pushes
  // onto and runs the very same lua_State. Two threads driving one Lua stack
  // corrupts it: a local that cannot be nil reads back as nil, and then the
  // process dies.
  //
  // Fixing it means giving each invocation its own execution state (a
  // lua_newthread coroutine off the script's state is the usual answer) or
  // holding the lock across core calls. Both are a change to the Lua
  // threading model rather than a tweak, so this is left failing-by-omission
  // and documented here instead of being papered over. Un-skip once fixed.
  it.skip("serves concurrent scripted checks that call back into the core", async () => {
    const results = await Promise.all(Array.from({ length: 4 }, () => nrpe("lua_nested")));
    for (const r of results) expect(r).toContain("nested saw: inner reached");
    expect(await nrpe("check_ok", ["message=still-alive"])).toContain("still-alive");
  });

  itScript("keeps serving checks while the agent reloads", async () => {
    resetTrace();

    // A reload re-runs loadModuleEx on every live module while these checks
    // are inside them. The checks must still be answered and the agent must
    // still be there afterwards.
    const inFlight = [nrpe("slow"), nrpe("slow")];
    await new Promise((r) => setTimeout(r, 300));
    await nscp.run(["settings", "--path", "/settings/default", "--key", "timeout", "--set", "47"]);

    const results = await Promise.all(inFlight);
    for (const r of results) expect(r).toContain("slow done");

    // Still alive and answering after all of that.
    expect(await nrpe("check_ok", ["message=after-reload"])).toContain("after-reload");
  });
});
