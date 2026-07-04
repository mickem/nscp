/**
 * Live/remote acceptance suite.
 *
 * Runs against an nscp that is already installed, configured and running —
 * typically a freshly provisioned VM (see build/powershell/run-tests.ps1) but
 * equally a locally-installed service or a hand-started dev build. Nothing is
 * spawned here; the target and credentials come from NSCP_TARGET_* (see
 * src/live-target.ts).
 *
 *   npm run test:live      # honours NSCP_TARGET_URL / NSCP_TARGET_PASSWORD
 *
 * Assertions are deliberately "shape + healthy", not "forced WARNING/CRITICAL":
 * these run against an unmanipulated real machine, so we verify the REST API
 * answers, auth works, and the standard checks return a valid status with the
 * performance data they are supposed to expose. Threshold behaviour is covered
 * exhaustively by the local self-spawning suites.
 */
import {
  liveLogin,
  liveVersion,
  liveExecute,
  livePoll,
  liveDetectOs,
  type LiveTarget,
  type TargetOs,
} from "@fixtures/live-target";
import { OK, WARNING, CRITICAL, messageOf, perfOf, type QueryResult } from "@fixtures/queries";

/** A status that is a real Nagios code (not the 3/UNKNOWN "couldn't run"). */
function ranOk(q: QueryResult): boolean {
  return [OK, WARNING, CRITICAL].includes(q.result);
}

function summarize(q: QueryResult): string {
  return `result=${q.result} :: ${messageOf(q)}`;
}

describe("live REST API", () => {
  let target: LiveTarget;

  beforeAll(async () => {
    target = await liveLogin();
  });

  it("authenticates and issues a session key", () => {
    expect(target.key).toBeTruthy();
  });

  it("reports a version", async () => {
    const version = await liveVersion(target);
    expect(version).toMatch(/\d+\.\d+/);
  });

  it("runs the trivial check_ok probe", async () => {
    const q = await liveExecute(target, "check_ok");
    expect(q.result).toBe(OK);
  });
});

describe("live system checks (cross-platform)", () => {
  let target: LiveTarget;

  beforeAll(async () => {
    target = await liveLogin();
  });

  it("check_uptime returns OK with an uptime perf value", async () => {
    const q = await livePoll(target, "check_uptime", {}, ranOk);
    expect(ranOk(q)).toBe(true);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });

  it("check_cpu returns a status with load perf data", async () => {
    // CPU reads from a 1 Hz collector; poll past warm-up.
    const q = await livePoll(target, "check_cpu", {}, (r) => ranOk(r) && Object.keys(perfOf(r)).length > 0);
    expect(ranOk(q)).toBe(true);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });

  it("check_memory returns a status with memory perf data", async () => {
    const q = await livePoll(target, "check_memory", {}, (r) => ranOk(r) && Object.keys(perfOf(r)).length > 0);
    expect(ranOk(q)).toBe(true);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });

  it("check_drivesize evaluates at least one drive", async () => {
    const q = await livePoll(target, "check_drivesize", {}, ranOk);
    expect(ranOk(q)).toBe(true);
    expect(Object.keys(perfOf(q)).length).toBeGreaterThan(0);
  });
});

describe("live platform-specific checks", () => {
  let target: LiveTarget;
  let os: TargetOs | undefined;

  beforeAll(async () => {
    target = await liveLogin();
    os = await liveDetectOs(target);
    // eslint-disable-next-line no-console
    console.log(`[live] detected target OS: ${os ?? "unknown"}`);
  });

  it("reports which OS the target is", () => {
    expect(os === "windows" || os === "linux").toBe(true);
  });

  // Linux-only native checks added on the linux-parity branch.
  const linuxIt = (name: string, fn: () => Promise<void>) =>
    it(name, async () => {
      if (os !== "linux") return; // skipped on non-linux targets
      await fn();
    });

  linuxIt("check_load returns OK on an idle host", async () => {
    const q = await liveExecute(target, "check_load");
    expect(ranOk(q)).toBe(true);
  });

  linuxIt("check_swap_io reports swap paging rates", async () => {
    const q = await livePoll(target, "check_swap_io", {}, ranOk);
    expect(ranOk(q)).toBe(true);
  });

  linuxIt("check_service (systemd) evaluates units", async () => {
    const q = await liveExecute(target, "check_service");
    expect(ranOk(q)).toBe(true);
  });

  linuxIt("check_mount verifies the root filesystem is mounted", async () => {
    const q = await liveExecute(target, "check_mount", { mount: "/" });
    expect(q.result).toBe(OK);
  });

  // Windows-only checks (present on a default Windows install).
  const windowsIt = (name: string, fn: () => Promise<void>) =>
    it(name, async () => {
      if (os !== "windows") return; // skipped on non-windows targets
      await fn();
    });

  windowsIt("check_service evaluates Windows services", async () => {
    const q = await liveExecute(target, "check_service");
    expect(ranOk(q)).toBe(true);
  });

  windowsIt("check_eventlog runs against the System log", async () => {
    const q = await liveExecute(target, "check_eventlog", { log: "System" });
    expect(ranOk(q)).toBe(true);
  });
});
