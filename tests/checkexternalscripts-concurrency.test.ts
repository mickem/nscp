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
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const NRPE_PORT = 5666;
const onWindows = process.platform === "win32";

describe("CheckExternalScripts — concurrent scripts keep their streams apart", () => {
  let nscp: NscpInstance;
  let scriptsDir: string;

  // Distinctive enough that finding one inside the other's output can only
  // mean the two streams crossed.
  const ALPHA = "alpha-3f2a1c";
  const BETA = "beta-9d4b7e";
  // Each script waits before printing, so both are still alive, each holding
  // its own pipe, while the other one is spawned.
  const WAIT_SECONDS = 3;

  const scriptFile = (name: string) => path.join(scriptsDir, `${name}.${onWindows ? "bat" : "sh"}`);

  function writeScript(name: string, marker: string): void {
    // One line of output, printed last: it proves the script ran to
    // completion, and it keeps the payload clear of any question about how a
    // transport treats multi-line check output.
    const body = onWindows
      ? `@echo off\r\nping -n ${WAIT_SECONDS + 1} 127.0.0.1 >nul\r\necho ${marker}-done\r\n`
      : `#!/bin/sh\nsleep ${WAIT_SECONDS}\necho "${marker}-done"\n`;
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
        "--command",
        command,
      ],
      { allowFailure: true, timeout: 60_000 },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(async () => {
    scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extscr-conc-"));
    writeScript("alpha", ALPHA);
    writeScript("beta", BETA);

    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { CheckExternalScripts: "enabled", NRPEServer: "enabled" },
      // Comfortably longer than the scripts, so nothing here is a timeout.
      "/settings/external scripts": { timeout: "60" },
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
    const [alphaOut, betaOut] = await Promise.all([
      nrpeQuery("check_alpha"),
      nrpeQuery("check_beta"),
    ]);
    const elapsed = (Date.now() - started) / 1000;

    // Each check got its own script's line...
    expect(alphaOut).toContain(`${ALPHA}-done`);
    expect(betaOut).toContain(`${BETA}-done`);

    // ...and neither saw a single byte of the other's.
    expect(alphaOut).not.toContain(BETA);
    expect(betaOut).not.toContain(ALPHA);

    // The assertions above only mean something if the two scripts really were
    // alive at the same time. Run back to back they would take twice the
    // wait; overlapping they take a little over one of them, so a bound
    // between the two says they overlapped without pinning an exact duration.
    expect(elapsed).toBeLessThan(WAIT_SECONDS * 2);
  });
});
