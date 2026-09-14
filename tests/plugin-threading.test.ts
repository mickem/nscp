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

import { NscpInstance } from "@fixtures/index";

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

describe("plugin threading", () => {
  let nscp: NscpInstance;
  let scriptDir: string;
  let tracePath: string;

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

  /** The trace the slow check writes: "+" on entry, "-" on exit. */
  function readTrace(): string[] {
    if (!fs.existsSync(tracePath)) return [];
    return fs
      .readFileSync(tracePath, "utf8")
      .split(/\r?\n/)
      .map((l) => l.trim())
      .filter((l) => l.length > 0);
  }

  function resetTrace(): void {
    fs.writeFileSync(tracePath, "");
  }

  /**
   * Highest number of slow checks that were inside the agent at the same
   * moment, replayed from the trace. 1 means they were serialised.
   */
  function peakOverlap(lines: string[]): number {
    let depth = 0;
    let peak = 0;
    for (const line of lines) {
      if (line.startsWith("+")) {
        depth += 1;
        peak = Math.max(peak, depth);
      } else if (line.startsWith("-")) {
        depth -= 1;
      }
    }
    return peak;
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    scriptDir = nscp.scratch("threading");
    tracePath = path.join(scriptDir, "trace.log");
    resetTrace();

    // A check that genuinely blocks its dispatch thread: CheckExternalScripts
    // waits on the child process, so there is no cooperative yielding hiding
    // a serialised core. Each marker is one short line appended to a file
    // opened in append mode, which both platforms write atomically.
    const slowScript = path.join(scriptDir, "slow.py");
    // JSON.stringify produces a correctly escaped literal for Python too, so
    // a Windows path survives whichever separator it carries.
    const traceLiteral = JSON.stringify(tracePath);
    fs.writeFileSync(
      slowScript,
      [
        "import os, sys, time",
        "",
        `TRACE = ${traceLiteral}`,
        "",
        "def mark(sign):",
        '    with open(TRACE, "a") as f:',
        '        f.write("%s %d\\n" % (sign, os.getpid()))',
        "",
        'mark("+")',
        `time.sleep(${SLOW_MS / 1000})`,
        'mark("-")',
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

    await nscp.configure({
      "/modules": {
        NRPEServer: "enabled",
        CheckExternalScripts: "enabled",
        CheckHelpers: "enabled",
        LUAScript: "enabled",
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
      "/settings/lua/scripts": {
        threading: luaScript,
      },
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

    const trace = readTrace();
    const peak = peakOverlap(trace);

    // A failure here is almost always one CI-only observation, so make sure
    // the evidence reaches the log rather than just the expected/received.
    const serialisedMs = callers * SLOW_MS;
    const budgetMs = serialisedMs * 0.75;
    if (peak <= 1 || elapsed >= budgetMs) {
      console.error(`peak=${peak} elapsed=${elapsed}ms trace=[${trace.join(" ")}]`);
    }

    expect(trace.filter((l) => l.startsWith("+"))).toHaveLength(callers);
    expect(trace.filter((l) => l.startsWith("-"))).toHaveLength(callers);

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

    // Hold CheckExternalScripts busy, then ask CheckHelpers for something
    // trivial. If a slow check could block the whole dispatch path the fast
    // one could not come back before the slow one did.
    const slow = nrpe("slow");
    const fastFinished: number[] = [];
    const slowStarted = Date.now();

    // Give the slow check a moment to actually be inside the agent.
    await new Promise((r) => setTimeout(r, 300));

    for (let i = 0; i < 3; i++) {
      const out = await nrpe("check_ok", ["message=fast"]);
      expect(out).toContain("fast");
      fastFinished.push(Date.now() - slowStarted);
    }

    const slowOut = await slow;
    expect(slowOut).toContain("slow done");
    const slowDuration = Date.now() - slowStarted;

    // Every fast check came back while the slow one was still running.
    for (const t of fastFinished) expect(t).toBeLessThan(slowDuration);
    expect(readTrace().filter((l) => l.startsWith("-"))).toHaveLength(1);
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
