/**
 * Legacy NSClient protocol (check_nt, port 12489) end-to-end against the
 * REAL check_nt client from the official nagios-plugins release, built
 * from source in a container (Dockerfiles/check_nt.Dockerfile). Covers:
 *
 *   - wire compatibility for the classic variables (CLIENTVERSION,
 *     UPTIME, CPULOAD everywhere; MEMUSE / USEDDISKSPACE / PROCSTATE on
 *     Windows — their server-side mappings dispatch to Windows-shaped
 *     queries, see the per-test notes),
 *   - password enforcement (a wrong password gets the generic
 *     "ERROR: Bad request." with no valid/invalid oracle),
 *   - the `allow` setting: groups and individual command names permit
 *     exactly what they say, everything else is refused before dispatch
 *     with "ERROR: Command not allowed.", and an allow list that resolves
 *     to nothing fails closed.
 *
 * check_nt turns any "ERROR"-prefixed payload into exit 3 (UNKNOWN) with
 * the payload echoed after "NSClient - ", which is what the deny
 * assertions key on.
 */
import * as path from "path";
import {
  DOCKER_HOST_ALLOWED_HOSTS,
  GenericContainer,
  NscpInstance,
  dockerOrSkip,
  dockerRunOnce,
  hostGatewayExtraHosts,
} from "@fixtures/index";

jest.setTimeout(900_000);

const onWindows = process.platform === "win32";
const PASSWORD = "check_nt-password";

dockerOrSkip()("check_nt (legacy NSClient) integration", () => {
  let nscp: NscpInstance;
  const image = "check_nt";

  beforeAll(async () => {
    await GenericContainer.fromDockerfile(
      path.resolve(__dirname),
      "Dockerfiles/check_nt.Dockerfile",
    ).build(image, { deleteOnExit: false });
    nscp = new NscpInstance();
  });

  afterAll(async () => {
    await nscp?.stop();
  });

  /**
   * (Re)configure and (re)start nscp with the NSClient (check_nt) server.
   * Leaving `allow` undefined keeps the built-in default ("any").
   */
  async function startNsclient(allow?: string): Promise<void> {
    await nscp.stop();
    await nscp.configure({
      // CheckSystem serves UPTIME/CPULOAD/MEMUSE/PROCSTATE; CheckDisk
      // serves USEDDISKSPACE (check_drivesize) and FILEAGE (check_files).
      "/modules": { NSClientServer: "enabled", CheckSystem: "enabled", CheckDisk: "enabled" },
      "/settings/default": {
        "allowed hosts": DOCKER_HOST_ALLOWED_HOSTS,
        password: PASSWORD,
      },
      "/settings/NSClient/server": {
        // The real check_nt never learned TLS, and the server now
        // defaults to ssl on — the legacy-interop scenario is exactly
        // the case where the operator has to opt out explicitly.
        "use ssl": false,
        port: "12489",
        ...(allow !== undefined ? { allow } : {}),
      },
    });
    nscp.start();
    await nscp.waitForPort(12489, { timeoutMs: 30_000 });
    // Same beat as nrpe-tls.test.ts: after a restart Rancher Desktop's
    // gvisor port forward can lag the host-side rebind.
    await new Promise((res) => setTimeout(res, 1500));
  }

  function checkNt(
    args: string[],
    opts: { password?: string; allowFailure?: boolean } = {},
  ): ReturnType<typeof dockerRunOnce> {
    return dockerRunOnce(
      image,
      [
        "check_nt",
        "-H",
        "host.docker.internal",
        "-p",
        "12489",
        "-s",
        opts.password ?? PASSWORD,
        // Generous client timeout: the first full process enumeration
        // (PROCSTATE) can take several seconds on a loaded host, and
        // netutils turns a quiet-but-open socket into "No data was
        // received from host!" once its window passes.
        "-t",
        "30",
        ...args,
      ],
      { extraHosts: hostGatewayExtraHosts(), allowFailure: opts.allowFailure },
    );
  }

  /**
   * Poll a check_nt invocation until it exits 0 or the deadline passes,
   * returning the last result either way. Used for the variables that
   * read the 1 Hz background collector (CPULOAD), which legitimately
   * answer with an ERROR payload for the first seconds after boot.
   */
  async function checkNtEventually(args: string[], deadlineMs = 60_000) {
    const deadline = Date.now() + deadlineMs;
    for (;;) {
      const r = await checkNt(args, { allowFailure: true });
      if (r.exitCode === 0 || Date.now() >= deadline) return r;
      await new Promise((res) => setTimeout(res, 1000));
    }
  }

  describe("with the default allow (any)", () => {
    beforeAll(async () => {
      await startNsclient();
    });

    it("CLIENTVERSION returns the agent version", async () => {
      const r = await checkNt(["-v", "CLIENTVERSION"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/\d+\.\d+\.\d+/);
    });

    it("UPTIME reports the system uptime", async () => {
      const r = await checkNt(["-v", "UPTIME"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/System Uptime - \d+ day\(s\) \d+ hour\(s\) \d+ minute\(s\)/);
    });

    it("CPULOAD reports collector-backed load with perfdata", async () => {
      // warn/crit pinned at 100 so only a fully saturated minute could
      // escalate — and checkNtEventually retries that away along with
      // the collector warm-up.
      const r = await checkNtEventually(["-v", "CPULOAD", "-l", "1,100,100"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/CPU Load \d+% \(1 min average\)/);
      expect(r.all).toMatch(/'1 min avg Load'=\d+%/);
    });

    it("MEMUSE reports committed memory (Windows)", async () => {
      // The server maps MEMUSE to `check_memory type=committed`; the
      // Unix check_memory only knows physical/cached/swap, so this
      // legacy variable only answers on Windows.
      if (!onWindows) return;
      const r = await checkNt(["-v", "MEMUSE"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/Memory usage: total:\d+\.\d+ MB - used: \d+\.\d+ MB \(\d+%\)/);
    });

    it("USEDDISKSPACE reports drive usage (Windows)", async () => {
      // check_nt only accepts a single-character drive designator, which
      // is drive-letter semantics; keep this variable Windows-only.
      if (!onWindows) return;
      const r = await checkNt(["-v", "USEDDISKSPACE", "-l", "c"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/c:\\ - total: \d+\.\d+ Gb - used: \d+\.\d+ Gb \(\d+%\)/);
    });

    it("PROCSTATE finds the agent's own process (Windows)", async () => {
      // The mapping renders `${legacy_state}`, which only the Windows
      // check_process registers.
      if (!onWindows) return;
      const r = await checkNt(["-v", "PROCSTATE", "-d", "SHOWALL", "-l", "nscp.exe"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/nscp\.exe: Running/);
    });

    it("a wrong password is rejected with the generic error", async () => {
      const r = await checkNt(["-v", "UPTIME"], {
        password: "not-the-password",
        allowFailure: true,
      });
      expect(r.exitCode).toBe(3);
      expect(r.all).toContain("ERROR: Bad request.");
      // The response must not leak whether the password or the command
      // was the problem (that distinction used to be a guessing oracle).
      expect(r.all).not.toMatch(/password/i);
    });
  });

  describe("with allow = 'info, uptime'", () => {
    beforeAll(async () => {
      await startNsclient("info, uptime");
    });

    it("allows CLIENTVERSION via the 'info' group", async () => {
      const r = await checkNt(["-v", "CLIENTVERSION"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/\d+\.\d+\.\d+/);
    });

    it("allows UPTIME via the individual command name", async () => {
      const r = await checkNt(["-v", "UPTIME"]);
      expect(r.exitCode).toBe(0);
      expect(r.all).toMatch(/System Uptime/);
    });

    it("denies MEMUSE", async () => {
      const r = await checkNt(["-v", "MEMUSE"], { allowFailure: true });
      expect(r.exitCode).toBe(3);
      expect(r.all).toContain("ERROR: Command not allowed.");
    });

    it("denies COUNTER before dispatch", async () => {
      // COUNTER maps to the Windows-only check_pdh, but the allow gate
      // rejects the request before any dispatch happens, so the denial
      // is identical on every platform.
      const r = await checkNt(
        ["-v", "COUNTER", "-l", "\\\\Processor(_Total)\\\\% Processor Time"],
        { allowFailure: true },
      );
      expect(r.exitCode).toBe(3);
      expect(r.all).toContain("ERROR: Command not allowed.");
    });

    it("denies PROCSTATE", async () => {
      const r = await checkNt(["-v", "PROCSTATE", "-l", "nscp.exe"], { allowFailure: true });
      expect(r.exitCode).toBe(3);
      expect(r.all).toContain("ERROR: Command not allowed.");
    });
  });

  describe("with an allow list that resolves to nothing", () => {
    beforeAll(async () => {
      await startNsclient("no-such-command");
    });

    it("fails closed: even CLIENTVERSION is refused", async () => {
      const r = await checkNt(["-v", "CLIENTVERSION"], { allowFailure: true });
      expect(r.exitCode).toBe(3);
      expect(r.all).toContain("ERROR: Command not allowed.");
    });
  });
});
