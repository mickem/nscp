/**
 * Exercises the `counter access` gate on CheckSystem: which performance
 * counters a caller may ask check_pdh (check_counter) to read once an operator
 * has restricted it.
 *
 * PDH exposes every performance object on the machine, so on a host where
 * callers pass arguments (NRPE with `allow arguments`, or REST) an
 * unrestricted `counter=` reads any of them. Unlike the logfile and WMI gates
 * this one needs no new predefined list: the counters an operator already
 * configured in [/settings/system/windows/counters] are the predefined set.
 *
 * Each case runs a one-shot client query - `nscp client --module CheckSystem
 * --boot --query check_pdh ...` - which re-reads the settings file every time.
 * PDH is Windows-only, so the suite is skipped elsewhere.
 *
 * Note on the accept cases: a counter referenced by name is served from the
 * background collector, which may not have sampled yet in a one-shot process.
 * Those cases therefore assert that the gate let the counter through (no
 * refusal) rather than asserting a value came back - the value is the
 * collector's business and is covered by the main CheckSystem suite.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

const UNKNOWN = 3;
/** Present on every Windows install, and free of characters needing quoting. */
const THREADS = "\\System\\Threads";
const PROCESSES = "\\System\\Processes";
const MEMORY = "\\Memory\\Pages/sec";

onWindows("CheckSystem counter access modes", () => {
  let nscp: NscpInstance;

  async function check(args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", "--module", "CheckSystem", "--boot", "--query", "check_pdh", ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** Point [/settings/system/windows] at one mode, always writing both keys. */
  async function setAccess(mode: string, allowed = "-"): Promise<void> {
    await nscp.configure({
      "/settings/system/windows": { "counter access": mode, "allowed counters": allowed },
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckSystem: "enabled" } });
  });

  // --- any: the default, and what every earlier release did ------------------

  describe("any (default)", () => {
    beforeAll(() => setAccess("any"));

    it("reads any counter the caller names", async () => {
      const { out, code } = await check([`counter=${THREADS}`, "warning=value > 0"]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).not.toMatch(/Refusing counter/);
    });

    it("does not enforce an allow list which is configured but unused", async () => {
      await setAccess("any", "\\Memory\\*");
      const { out } = await check([`counter=${THREADS}`, "warning=value > 0"]);
      expect(out).not.toMatch(/Refusing counter/);
      await setAccess("any");
    });
  });

  // --- allowed: only paths matching the list ---------------------------------

  describe("allowed", () => {
    beforeAll(() => setAccess("allowed", "\\System\\*"));

    it("reads a counter matching the allow list", async () => {
      const { out } = await check([`counter=${THREADS}`, "warning=value > 0"]);
      expect(out).not.toMatch(/Refusing counter/);
    });

    it("refuses a counter outside the allow list", async () => {
      const { out, code } = await check([`counter=${MEMORY}`, "warning=value > 0"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing counter/);
      expect(out).toMatch(/allowed counters/);
    });

    it("refuses the whole check when one of several counters is not allowed", async () => {
      const { out, code } = await check([`counter=${THREADS}`, `counter=${MEMORY}`, "warning=value > 0"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing counter/);
    });

    it("accepts several entries", async () => {
      await setAccess("allowed", "\\System\\*, \\Memory\\*");
      expect((await check([`counter=${THREADS}`, "warning=value > 0"])).out).not.toMatch(/Refusing counter/);
      expect((await check([`counter=${MEMORY}`, "warning=value > 0"])).out).not.toMatch(/Refusing counter/);
      await setAccess("allowed", "\\System\\*");
    });

    // The parentheses and backslashes a counter path is full of must be
    // matched literally, not read as regular-expression syntax.
    it("treats an instance name as literal text", async () => {
      await setAccess("allowed", "\\Processor(_Total)\\*");
      const { out } = await check(["counter=\\Processor(_Total)\\% Processor Time", "warning=value > 100"]);
      expect(out).not.toMatch(/Refusing counter/);
      await setAccess("allowed", "\\System\\*");
    });

    it("also gates the counter:<alias>= form", async () => {
      const { out, code } = await check([`counter:mem=${MEMORY}`, "warning=value > 0"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing counter/);
    });
  });

  // --- predefined: only the configured counters ------------------------------

  describe("predefined", () => {
    beforeAll(async () => {
      await nscp.configure({ "/settings/system/windows/counters": { threads: THREADS } });
      await setAccess("predefined");
    });

    it("accepts a counter referenced by its configured name", async () => {
      const { out } = await check(["counter=threads", "warning=value > 0"]);
      expect(out).not.toMatch(/Refusing counter/);
    });

    it("refuses a raw counter path", async () => {
      const { out, code } = await check([`counter=${PROCESSES}`, "warning=value > 0"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("refuses a name which is not configured", async () => {
      const { out, code } = await check(["counter=nosuchname", "warning=value > 0"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing counter/);
    });

    it("accepts a configured name in allowed mode too", async () => {
      await setAccess("allowed", "\\Memory\\*");
      const { out } = await check(["counter=threads", "warning=value > 0"]);
      expect(out).not.toMatch(/Refusing counter/);
      await setAccess("predefined");
    });
  });

  // --- a typo in the mode must not read as "no restriction" ------------------

  describe("an invalid mode", () => {
    it("fails closed and names the valid values", async () => {
      await setAccess("allwed", "*");
      const { out, code } = await check([`counter=${THREADS}`, "warning=value > 0"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/expected any, allowed or predefined/);
      await setAccess("any");
    });
  });
});
