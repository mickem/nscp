/**
 * A predefined counter that does not exist must not take the other predefined
 * counters down with it (#634).
 *
 * The collector used to add every counter in [/settings/system/windows/counters]
 * to one PDH query, so a single counter that was not registered - typically one
 * a service registers when it starts, after NSClient++ did - failed the whole
 * query and every predefined counter answered "Failed to get value" until the
 * next restart. Each counter is now added on its own and a missing one is
 * retried in the background; `reload=true` asks for that retry right away.
 *
 * Each case runs a one-shot client query, so `reload=true` also waits for the
 * collector thread to have finished its setup, which keeps the "the valid
 * counter is served" assertion independent of thread start-up timing. PDH is
 * Windows-only, so the suite is skipped elsewhere.
 */
import { NscpInstance, describeOnWindows } from "@fixtures/index";

jest.setTimeout(180_000);

const UNKNOWN = 3;
/** Present on every Windows install. */
const THREADS = "\\System\\Threads";
/** An object no Windows install registers. */
const MISSING = "\\NSClient Missing Object 634\\Nothing";

describeOnWindows("CheckSystem predefined counters with one missing", () => {
  let nscp: NscpInstance;

  async function check(args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      ["client", "--module", "CheckSystem", "--boot", "--query", "check_pdh", ...args],
      {
        allowFailure: true,
      },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({
      "/modules": { CheckSystem: "enabled" },
      "/settings/system/windows/counters": { threads: THREADS, missing: MISSING },
    });
  });

  it("still serves the counters that exist", async () => {
    const { out, code } = await check([
      "counter=threads",
      "reload=true",
      "warning=value < 0",
      "critical=value < 0",
    ]);
    expect(out).not.toMatch(/Failed to get value/);
    expect(out).not.toMatch(/does not take any arguments/);
    expect(code).not.toBe(UNKNOWN);
  });

  it("reports the missing counter as missing", async () => {
    const { out, code } = await check([
      "counter=missing",
      "reload=true",
      "warning=value < 0",
      "critical=value < 0",
    ]);
    expect(code).toBe(UNKNOWN);
    expect(out).toMatch(/Failed to get value/);
  });
});
