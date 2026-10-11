/**
 * Live/remote acceptance suite against one agent.
 *
 * Runs the agent checks every live target must pass (src/live-smoke.ts)
 * against an nscp that is already installed, configured and running: a
 * machine the build/powershell setup scripts provisioned, a locally-installed
 * service or a hand-started dev build. Nothing is spawned here; the target
 * and credentials come from NSCP_TARGET_* or a .vm.pwd file (see
 * src/live-target.ts).
 *
 *   npm run test:live -- live-checks
 *
 * The estate runs the same cases against every machine, next to its role's
 * checks (live/estate.test.ts).
 */
import {
  liveConfig,
  liveLogin,
  liveDetectOs,
  type LiveTarget,
  type TargetOs,
} from "@fixtures/live-target";
import { smokeCasesFor, SMOKE_CASES } from "@fixtures/live-smoke";

/** Whether NSCP_TARGET_* (or a .vm.pwd) name a target at all. */
function targetConfigured(): boolean {
  try {
    liveConfig();
    return true;
  } catch {
    return false;
  }
}

// `npm run test:live -- estate` with only NSCP_ESTATE_MANIFEST set is an estate
// run: there is no single target, so this suite steps aside rather than fail
// every test on the missing password. Without a manifest either, it runs and
// fails, saying what to set.
const describeLive =
  targetConfigured() || !process.env.NSCP_ESTATE_MANIFEST ? describe : describe.skip;

describeLive("live agent checks", () => {
  let target: LiveTarget;
  let os: TargetOs | undefined;

  beforeAll(async () => {
    target = await liveLogin();
    os = await liveDetectOs(target);
    // eslint-disable-next-line no-console
    console.log(`[live] target OS: ${os ?? "unknown"}`);
  });

  it("authenticates and issues a session key", () => {
    expect(target.key).toBeTruthy();
  });

  it("reports which OS the target is", () => {
    expect(os === "windows" || os === "linux").toBe(true);
  });

  // Registered before the OS is known, so every case is declared and the ones
  // for the other platform pass without running.
  for (const c of SMOKE_CASES) {
    it(c.os ? `${c.name} (${c.os})` : c.name, async () => {
      if (!smokeCasesFor(os).includes(c)) return;
      await c.run(target);
    });
  }
});
