/**
 * Exercises the `log access` gate on check_eventlog: which event log channels
 * a caller may ask it to read.
 *
 * The event text itself comes back through the `message`, `strings` and `xml`
 * keywords, and `message` is part of the *default* detail syntax, so a caller
 * needs no syntax argument to get it. Where the caller picks the channel (NRPE
 * with `allow arguments`, or REST) that reaches everything SYSTEM can read -
 * the Security channel, and the PowerShell and Sysmon operational channels
 * among them.
 *
 * Each case runs a one-shot client query - `nscp client --module CheckEventLog
 * --boot --query check_eventlog ...` - which re-reads the settings file every
 * time. The event log is Windows-only, so the suite is skipped elsewhere.
 *
 * Cases assert on whether the gate let the channel through rather than on the
 * events found: a CI machine's logs are not deterministic, and an empty result
 * is a legitimate outcome. `scan-range` is pinned so no case depends on how
 * much history the box happens to hold.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

const UNKNOWN = 3;

onWindows("CheckEventLog log access modes", () => {
  let nscp: NscpInstance;

  async function check(args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      ["client", "--module", "CheckEventLog", "--boot", "--query", "check_eventlog", ...args, "scan-range=-10m", "empty-state=ok"],
      { allowFailure: true },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** Point [/settings/eventlog] at one mode, always writing both keys. */
  async function setAccess(mode: string, allowed = "-"): Promise<void> {
    await nscp.configure({
      "/settings/eventlog": { "log access": mode, "allowed logs": allowed },
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckEventLog: "enabled" } });
  });

  // --- any: the default, and what every earlier release did ------------------

  describe("any (default)", () => {
    beforeAll(() => setAccess("any"));

    it("reads any channel the caller names", async () => {
      const { out } = await check(["file=Security"]);
      expect(out).not.toMatch(/Refusing log/);
    });

    it("does not enforce an allow list which is configured but unused", async () => {
      await setAccess("any", "Application");
      const { out } = await check(["file=Security"]);
      expect(out).not.toMatch(/Refusing log/);
      await setAccess("any");
    });
  });

  // --- allowed: only channels at or below an entry ---------------------------

  describe("allowed", () => {
    beforeAll(() => setAccess("allowed", "Application, System"));

    it("reads an allowed channel", async () => {
      const { out } = await check(["file=Application"]);
      expect(out).not.toMatch(/Refusing log/);
    });

    it("refuses a channel outside the list", async () => {
      const { out, code } = await check(["file=Security"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing log 'Security'/);
      expect(out).toMatch(/allowed logs/);
    });

    it("gates the log= alias the same way", async () => {
      const { out, code } = await check(["log=Security"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing log/);
    });

    it("refuses every channel when one of several is not allowed", async () => {
      const { out, code } = await check(["file=Application", "file=Security"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing log/);
    });

    // An entry covers the channel family below it, so a whole provider can be
    // allowed without naming each of its channels.
    it("covers a channel family by its prefix", async () => {
      await setAccess("allowed", "Microsoft-Windows-Sysmon");
      const { out } = await check(["file=Microsoft-Windows-Sysmon/Operational"]);
      expect(out).not.toMatch(/Refusing log/);
      await setAccess("allowed", "Application, System");
    });

    // The prefix must end on a separator, or an entry would cover an unrelated
    // provider whose name merely starts with it.
    it("does not cover a provider sharing the entry's name", async () => {
      await setAccess("allowed", "Microsoft-Windows-Sysmon");
      const { out, code } = await check(["file=Microsoft-Windows-SysmonOther/Operational"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing log/);
      await setAccess("allowed", "Application, System");
    });

    // The defaults are not exempt: an operator who restricted access did not
    // opt Application and System in by leaving the argument off.
    it("applies to the default channels when none is named", async () => {
      await setAccess("allowed", "Microsoft-Windows-Sysmon");
      const { out, code } = await check([]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing log/);
      await setAccess("allowed", "Application, System");
    });

    it("does not disclose the allow list in the refusal", async () => {
      await setAccess("allowed", "Microsoft-Windows-SecretProvider");
      const { out } = await check(["file=Security"]);
      expect(out).not.toMatch(/SecretProvider/);
      await setAccess("allowed", "Application, System");
    });
  });

  // --- predefined: only names the operator configured ------------------------

  describe("predefined", () => {
    beforeAll(async () => {
      await nscp.configure({ "/settings/eventlog/logs": { app: "Application" } });
      await setAccess("predefined");
    });

    it("reads a channel by its configured name", async () => {
      const { out } = await check(["file=app"]);
      expect(out).not.toMatch(/Refusing log/);
    });

    it("refuses a raw channel name", async () => {
      const { out, code } = await check(["file=Security"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("refuses a name which is not configured", async () => {
      const { out, code } = await check(["file=nosuchname"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("resolves a configured name in any mode too", async () => {
      await setAccess("any");
      expect((await check(["file=app"])).out).not.toMatch(/Refusing log/);
      await setAccess("predefined");
    });
  });

  // --- a typo in the mode must not read as "no restriction" ------------------

  describe("an invalid mode", () => {
    it("fails closed and names the valid values", async () => {
      await setAccess("allwed", "Application");
      const { out, code } = await check(["file=Application"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/expected any, allowed or predefined/);
      await setAccess("any");
    });
  });
});
