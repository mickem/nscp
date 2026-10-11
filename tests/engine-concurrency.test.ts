/**
 * Checks running at the same time in one agent must not interfere - the two
 * regressions the stress harness's slow-check-isolation scenario found, pinned
 * here so they run on every pull request rather than only in an opt-in soak.
 *
 * Both are driven over NRPE (plain TCP, from stress/lib/nrpe-client.ts),
 * because its socket server runs a worker pool and so really runs requests in
 * parallel; the WEB server dispatches every request on one thread, where
 * neither can show.
 *
 * - **Lua calls sharing one stack.** Every call into a Lua script ran on that
 *   script's main lua_State. nscp.sleep and the Core calls release the Lua
 *   lock, so a second call into the same script could start while the first
 *   was parked - on the same stack. Two concurrent nscp.sleep calls were enough
 *   to crash the agent with an access violation inside lua.dll. Each call now
 *   runs on a Lua thread of its own.
 *
 * - **Legacy asio strands.** Each socket connection bound its handlers to an
 *   io_context::strand, and those share a fixed pool of 193 implementations, so
 *   an unrelated connection could land on the strand of one whose check was
 *   running - and the check runs inside the strand-bound read handler. About
 *   one request in a few hundred waited for a slow check to finish. Executor
 *   strands each get their own implementation. With five slow checks in flight
 *   and several hundred probes, the old code was all but certain to queue one
 *   (the chance of none is about e^-10).
 */
import { describeWithModules } from "@fixtures/index";
import { type EngineAgent, SLOW_CHECKS, startEngineAgent } from "@stress/engines";

jest.setTimeout(180_000);

const SLOW_MS = 4_000;

describeWithModules(
  "CheckExternalScripts",
  "LUAScript",
  "PythonScript",
  "NRPEServer",
)("checks running at the same time do not interfere", () => {
  let agent: EngineAgent;

  beforeAll(async () => {
    agent = await startEngineAgent();
  });

  afterAll(async () => {
    await agent?.nscp.stop();
  });

  it("runs several Lua calls that sleep or call the core at once, each on its own stack", async () => {
    const started = Date.now();
    const calls = [
      ...[1, 2, 3].map((i) => ({
        command: "lua_sleep",
        args: [String(SLOW_MS), `lua${i}`],
        expect: `lua slept ${SLOW_MS}`,
      })),
      {
        command: "lua_via_py_sleep",
        args: [String(SLOW_MS), "lua_py"],
        expect: `lua>py slept ${SLOW_MS}`,
      },
      {
        command: "py_via_lua_sleep",
        args: [String(SLOW_MS), "py_lua"],
        expect: `py>lua slept ${SLOW_MS}`,
      },
    ];
    const answers = await Promise.all(calls.map((c) => agent.nrpe.query(c.command, c.args)));
    const elapsed = Date.now() - started;

    expect(answers.map((a) => ({ result: a.result, message: a.message }))).toEqual(
      calls.map((c) => ({ result: 0, message: c.expect })),
    );
    // They overlapped rather than queued: five back to back take five times as long.
    expect({ elapsed, overlapped: elapsed < 2 * SLOW_MS }).toMatchObject({ overlapped: true });
    // And the agent is still there, which it was not after the first pair.
    await agent.nrpe.expect("lua_ok", ["after"], "lua after");
  });

  it("never queues a new connection behind another connection's slow check", async () => {
    const slow = SLOW_CHECKS.map((s) =>
      agent.nrpe.query(s.command, [String(SLOW_MS), `strand_${s.command}`]),
    );
    let answered = 0;
    for (const p of slow) void p.then(() => answered++);

    // Probe in parallel batches until the first slow check answers.
    const latencies: number[] = [];
    let n = 0;
    while (answered === 0) {
      const batch = await Promise.all(
        Array.from({ length: 8 }, async () => {
          const token = `p${n++}`;
          const t0 = Date.now();
          await agent.nrpe.expect("check_ok", [`message=${token}`], token);
          return Date.now() - t0;
        }),
      );
      latencies.push(...batch);
    }
    const answers = await Promise.all(slow);

    expect(answers.map((a) => a.result)).toEqual(SLOW_CHECKS.map(() => 0));
    // A probe queued behind a slow check takes the rest of that check; a
    // second is far beyond anything a check_ok costs on any runner.
    const queued = latencies.filter((ms) => ms >= 1_000);
    expect({ probes: latencies.length, queued }).toMatchObject({ queued: [] });
    // And enough probes for a collision to have been all but certain on the
    // old strands; a first batch stuck behind a slow check also ends here.
    expect(latencies.length).toBeGreaterThanOrEqual(400);
  });
});
