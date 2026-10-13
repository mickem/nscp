/**
 * Two external scripts running at the same time inside one agent must each
 * get their own output back, and nothing of the other's.
 *
 * This is the property the launcher's handle and descriptor hygiene exists
 * for. On Windows a child used to inherit every inheritable handle in the
 * service, so a script spawned while another was running received that other
 * script's stdout pipe, both ends, and could read its output or write a
 * forged result into it. On Unix the pipe was not close-on-exec, with the
 * same effect for a script forked at the wrong moment.
 *
 * Transport matters here. The one-shot `client --boot --query` path the rest
 * of the CheckExternalScripts suite uses runs each check in its own
 * short-lived process, so two of them never overlap. The WEB server runs a
 * single io thread and dispatches synchronously, so two REST queries against
 * it serialise. A socket server has a real thread pool, ten threads by
 * default, so two NRPE requests to one long-lived `nscp test` are handled on
 * different threads and the two scripts genuinely run at once.
 *
 * That last part has to be *checked*, or the isolation assertions below prove
 * nothing - two scripts that never overlapped trivially keep their streams
 * apart. It used to be checked with one clock reading against twice the wait,
 * which measured the runner as much as the agent: the two client processes
 * have to be scheduled promptly for the scripts to overlap, and on a loaded
 * runner the second one can start seconds late. That produced a real CI
 * failure at 6.026s against a 6s bound, with the agent dispatching perfectly
 * well.
 *
 * So the scripts say so themselves: each creates a marker file while it runs
 * and removes it on the way out, and the test watches for both being present
 * at the same instant. That is a fact recorded by the scripts rather than
 * inferred from the client's clock, and it needs nothing a `.bat` cannot do.
 *
 * That still left the overlap itself to chance, which is how this failed on
 * the arm64 runner: recording the window correctly does not make the window
 * happen. Each query spawns its own client process, and if the second is
 * scheduled a few seconds late the first script has already finished - no
 * overlap to observe, through a marker file or anything else. So the scripts
 * now rendezvous: each marks itself, waits for the other's marker, and only
 * then runs down its clock. Overlap is therefore constructed rather than hoped
 * for, and a core that serialised dispatch still cannot produce it - the first
 * script would wait out the barrier alone, and `bothAlive` would stay false.
 *
 * The barrier is only as good as its length, though. On the Windows x86 runner
 * the agent logged alpha's dispatch before beta's connection was even
 * accepted, and the case took two full barrier-plus-hold cycles back to back:
 * the second client process had reached the agent more than fifteen seconds
 * after the first, alpha had given up waiting, and beta then waited out the
 * barrier alone. Nothing on the agent's side holds a second request back - the
 * socket server runs each connection on its own pool thread, and neither
 * launcher keeps a lock across a running child. So the barrier is now long
 * enough to absorb a client that slow, every timeout on the path (script, NRPE
 * server, NRPE client) is raised to cover it, and a failure reports when each
 * script was seen running, so the next one says how far apart they were.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance, onWindows } from "@fixtures/index";

jest.setTimeout(300_000);

const NRPE_PORT = 5666;

describe("CheckExternalScripts — concurrent scripts keep their streams apart", () => {
  let nscp: NscpInstance;
  let scriptsDir: string;

  // Distinctive enough that finding one inside the other's output can only
  // mean the two streams crossed.
  const ALPHA = "alpha-3f2a1c";
  const BETA = "beta-9d4b7e";
  // How long a script waits for its peer to show up before giving up and
  // finishing alone. This is the slack for the second client process reaching
  // the agent late, which a loaded Windows runner has been seen to stretch past
  // fifteen seconds; it costs nothing when the two meet, and is only waited out
  // in full when they never overlap.
  const BARRIER_SECONDS = 45;
  // How long both markers stay up once the two have met. This is the window
  // the test polls for, so it wants to be comfortably wider than a starved
  // event loop's polling interval - not long, just not marginal.
  const HOLD_SECONDS = 2;
  // Every timeout between the client and the script - the script's own, the
  // NRPE server's socket and the NRPE client's - has to outlast a script that
  // waits out the whole barrier, or a late peer turns into a timeout instead
  // of a rendezvous. Comfortably longer than that, so nothing here is one.
  const IO_TIMEOUT_SECONDS = 120;

  const scriptFile = (name: string) => path.join(scriptsDir, `${name}.${onWindows ? "bat" : "sh"}`);

  /** Present exactly while `name`'s script is running. */
  const runningFile = (name: string) => path.join(scriptsDir, `${name}.running`);

  /** How many of the two scripts are inside the agent right now. */
  const runningCount = () => ["alpha", "beta"].filter((n) => fs.existsSync(runningFile(n))).length;

  function writeScript(name: string, marker: string, peer: string): void {
    // One line of output, printed last: it proves the script ran to
    // completion, and it keeps the payload clear of any question about how a
    // transport treats multi-line check output.
    //
    // The marker file brackets the wait, so "both were alive at once" is
    // something the scripts record rather than something the test infers from
    // how quickly the two clients happened to start. Waiting for the peer's
    // marker before running the clock down is what makes the overlap certain:
    // whichever script is dispatched first sits at the barrier until the other
    // arrives, so a late second client delays the test instead of breaking it.
    const mark = runningFile(name);
    const peerMark = runningFile(peer);
    // ping is the portable batch sleep: -n 2 waits about a second.
    const body = onWindows
      ? [
          "@echo off",
          `echo running > "${mark}"`,
          "set /a waited=0",
          ":wait",
          `if exist "${peerMark}" goto ready`,
          "ping -n 2 127.0.0.1 >nul",
          "set /a waited+=1",
          `if %waited% LSS ${BARRIER_SECONDS} goto wait`,
          ":ready",
          `ping -n ${HOLD_SECONDS + 1} 127.0.0.1 >nul`,
          `del "${mark}"`,
          `echo ${marker}-done`,
          "",
        ].join("\r\n")
      : [
          "#!/bin/sh",
          `echo running > "${mark}"`,
          "waited=0",
          `while [ ! -f "${peerMark}" ] && [ "$waited" -lt ${BARRIER_SECONDS * 10} ]; do`,
          "  sleep 0.1",
          "  waited=$((waited+1))",
          "done",
          `sleep ${HOLD_SECONDS}`,
          `rm -f "${mark}"`,
          `echo "${marker}-done"`,
          "",
        ].join("\n");
    fs.writeFileSync(scriptFile(name), body, { mode: 0o755 });
  }

  /** The command template the module runs for `name`. */
  const runner = (name: string) =>
    onWindows ? `cmd /c ${scriptFile(name)}` : `/bin/sh ${scriptFile(name)}`;

  /** Ask the running agent for one check over NRPE. */
  async function nrpeQuery(command: string): Promise<string> {
    const r = await nscp.run(
      [
        "nrpe",
        "--host",
        "127.0.0.1",
        "--port",
        String(NRPE_PORT),
        "--insecure",
        "--version",
        "2",
        "--timeout",
        String(IO_TIMEOUT_SECONDS),
        "--command",
        command,
      ],
      { allowFailure: true, timeout: (IO_TIMEOUT_SECONDS + 30) * 1000 },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(async () => {
    scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extscr-conc-"));
    writeScript("alpha", ALPHA, "beta");
    writeScript("beta", BETA, "alpha");

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { CheckExternalScripts: "enabled", NRPEServer: "enabled" },
      "/settings/external scripts": { timeout: String(IO_TIMEOUT_SECONDS) },
      "/settings/external scripts/scripts": {
        check_alpha: runner("alpha"),
        check_beta: runner("beta"),
      },
    });
    await nscp.run([
      "nrpe",
      "install",
      "--allowed-hosts",
      "127.0.0.1",
      "--insecure",
      "--verify=none",
    ]);
    // After the install, so nothing it writes to this section can reset it.
    // The socket timeout bounds the whole request, check included.
    await nscp.configure({ "/settings/NRPE/server": { timeout: String(IO_TIMEOUT_SECONDS) } });

    await nscp.waitForPortFree(NRPE_PORT, { timeoutMs: 30_000 });
    nscp.start();
    await nscp.waitForPort(NRPE_PORT, { timeoutMs: 30_000 });
  });

  afterAll(async () => {
    await nscp?.stop();
    fs.rmSync(scriptsDir, { recursive: true, force: true });
  });

  it("gives each concurrent script only its own output", async () => {
    const started = Date.now();
    let settled = false;
    const inFlight = Promise.all([nrpeQuery("check_alpha"), nrpeQuery("check_beta")]).then((r) => {
      settled = true;
      return r;
    });

    // Watch for the moment both markers exist. Polling rather than sleeping a
    // fixed time: once the two have met they both hold their markers for
    // HOLD_SECONDS, which is thousands of samples at this interval. Kept up
    // until both queries return, so that a failure can say when each script
    // was actually running rather than only that the two never met.
    let bothAlive = false;
    const seen: Record<string, { first: number; last: number }> = {};
    const deadline = Date.now() + (2 * (BARRIER_SECONDS + HOLD_SECONDS) + 60) * 1000;
    while (!settled && Date.now() < deadline) {
      const now = Date.now() - started;
      for (const name of ["alpha", "beta"]) {
        if (!fs.existsSync(runningFile(name))) continue;
        seen[name] = { first: seen[name]?.first ?? now, last: now };
      }
      if (runningCount() === 2) bothAlive = true;
      await new Promise((r) => setTimeout(r, 25));
    }
    const ranFor = (name: string) =>
      seen[name]
        ? `${name} ran ${seen[name].first / 1000}s-${seen[name].last / 1000}s`
        : `${name} never seen running`;
    const overlap = bothAlive
      ? "overlapped"
      : `never overlapped: ${ranFor("alpha")}, ${ranFor("beta")}`;

    const [alphaOut, betaOut] = await inFlight;
    const elapsed = (Date.now() - started) / 1000;

    // Each check got its own script's line...
    expect(alphaOut).toContain(`${ALPHA}-done`);
    expect(betaOut).toContain(`${BETA}-done`);

    // ...and neither saw a single byte of the other's.
    expect(alphaOut).not.toContain(BETA);
    expect(betaOut).not.toContain(ALPHA);

    // ...and the assertions above mean something, because the two scripts were
    // demonstrably inside the agent at the same instant rather than run back
    // to back. This is the claim the old wall-clock bound was standing in for.
    // On failure the message gives when each script was seen running: two
    // disjoint windows, each about the length of the barrier, mean the second
    // script only started after the first had given up waiting for it.
    expect(overlap).toBe("overlapped");

    // Loose backstop only, and deliberately never the assertion that reports
    // a real problem: two scripts that meet promptly are done in about
    // HOLD_SECONDS, while two that never overlap sit out the barrier twice and
    // fail on `bothAlive` above first. This is here to catch a gross stall.
    expect(elapsed).toBeLessThan((BARRIER_SECONDS + HOLD_SECONDS) * 4);
  });
});
