/**
 * Exercises the `registry access` gate on check_registry_key and
 * check_registry_value.
 *
 * This is the sharpest of the access gates. check_registry_value returns the
 * value data itself - with binary values rendered as hex, and the value in the
 * *default* detail syntax, so no syntax argument is needed - and `recursive`
 * walks a whole subtree. Where the caller picks the argument (NRPE with `allow
 * arguments`, or REST) that is an arbitrary registry read as SYSTEM.
 *
 * Each case runs a one-shot client query - `nscp client --module CheckSystem
 * --boot --query ...` - which re-reads the settings file every time, so the
 * mode can be rewritten between cases without restarting a server.
 * The registry is Windows-only, so the suite is skipped elsewhere.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

const UNKNOWN = 3;
/** Present on every Windows install and safe to read. */
const CURRENT_VERSION = "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
const CV_LONG = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
/** Deliberately outside anything the tests allow. */
const ELSEWHERE = "HKLM\\SYSTEM\\CurrentControlSet\\Control";

onWindows("CheckSystem registry access modes", () => {
  let nscp: NscpInstance;

  async function query(command: string, args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", "--module", "CheckSystem", "--boot", "--query", command, ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** Point [/settings/system/windows] at one mode, always writing both keys. */
  async function setAccess(mode: string, allowed = "-"): Promise<void> {
    await nscp.configure({
      "/settings/system/windows": { "registry access": mode, "allowed registry keys": allowed },
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckSystem: "enabled" } });
  });

  // --- any: the default, and what every earlier release did ------------------

  describe("any (default)", () => {
    beforeAll(() => setAccess("any"));

    it("reads any key the caller names", async () => {
      const { out, code } = await query("check_registry_value", [`key=${CURRENT_VERSION}`, "value=ProductName"]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).not.toMatch(/Refusing/);
    });

    it("does not enforce an allow list which is configured but unused", async () => {
      await setAccess("any", "HKLM\\SOFTWARE\\Nothing");
      const { out } = await query("check_registry_value", [`key=${CURRENT_VERSION}`, "value=ProductName"]);
      expect(out).not.toMatch(/Refusing/);
      await setAccess("any");
    });
  });

  // --- allowed: only keys at or below an entry -------------------------------

  describe("allowed", () => {
    beforeAll(() => setAccess("allowed", "HKLM\\SOFTWARE\\Microsoft\\Windows NT"));

    it("reads a key below an allowed entry", async () => {
      const { out } = await query("check_registry_value", [`key=${CURRENT_VERSION}`, "value=ProductName"]);
      expect(out).not.toMatch(/Refusing/);
    });

    it("refuses a key outside the allowed subtree", async () => {
      const { out, code } = await query("check_registry_value", [`key=${ELSEWHERE}`, "value=*"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing registry key/);
      expect(out).toMatch(/allowed registry keys/);
    });

    it("gates check_registry_key the same way", async () => {
      const { out, code } = await query("check_registry_key", [`key=${ELSEWHERE}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing registry key/);
    });

    // The reason this is a prefix match and not a glob: an entry must not cover
    // a sibling whose name merely starts with it.
    it("does not cover a sibling sharing the entry's name", async () => {
      await setAccess("allowed", "HKLM\\SOFTWARE\\Microsoft\\Windows");
      const { out, code } = await query("check_registry_key", ["key=HKLM\\SOFTWARE\\Microsoft\\Windows NT"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing registry key/);
      await setAccess("allowed", "HKLM\\SOFTWARE\\Microsoft\\Windows NT");
    });

    // Both hive spellings mean the same hive, so an allow list written one way
    // must still match a caller who used the other.
    it("accepts the long hive spelling against a short entry", async () => {
      const { out } = await query("check_registry_value", [`key=${CV_LONG}`, "value=ProductName"]);
      expect(out).not.toMatch(/Refusing/);
    });

    it("refuses the long hive spelling of a key outside the subtree", async () => {
      const { out, code } = await query("check_registry_value", ["key=HKEY_LOCAL_MACHINE\\SYSTEM", "value=*"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing registry key/);
    });

    it("refuses every key when one of several is not allowed", async () => {
      const { out, code } = await query("check_registry_value", [`key=${CURRENT_VERSION}`, `key=${ELSEWHERE}`, "value=*"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing registry key/);
    });

    // A restricted check reads the local registry only: `computer=` is an
    // outbound destination the allow list says nothing about.
    it("refuses a remote computer while restricted", async () => {
      const { out, code } = await query("check_registry_value", [`key=${CURRENT_VERSION}`, "value=ProductName", "computer=some-other-host"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing computer/);
    });

    it("honours a wildcard entry", async () => {
      await setAccess("allowed", "HKLM\\SOFTWARE\\Microsoft\\*\\CurrentVersion");
      expect((await query("check_registry_value", [`key=${CURRENT_VERSION}`, "value=ProductName"])).out).not.toMatch(/Refusing/);
      expect((await query("check_registry_key", [`key=${ELSEWHERE}`])).code).toBe(UNKNOWN);
      await setAccess("allowed", "HKLM\\SOFTWARE\\Microsoft\\Windows NT");
    });

    it("does not disclose the allow list in the refusal", async () => {
      await setAccess("allowed", "HKLM\\SOFTWARE\\SecretVendor");
      const { out } = await query("check_registry_key", [`key=${ELSEWHERE}`]);
      expect(out).not.toMatch(/SecretVendor/);
      await setAccess("allowed", "HKLM\\SOFTWARE\\Microsoft\\Windows NT");
    });
  });

  // --- predefined: only names the operator configured ------------------------

  describe("predefined", () => {
    beforeAll(async () => {
      await nscp.configure({ "/settings/system/windows/registry": { winver: CURRENT_VERSION } });
      await setAccess("predefined");
    });

    it("reads a key by its configured name", async () => {
      const { out } = await query("check_registry_value", ["key=winver", "value=ProductName"]);
      expect(out).not.toMatch(/Refusing/);
    });

    it("refuses a raw key", async () => {
      const { out, code } = await query("check_registry_value", [`key=${CURRENT_VERSION}`, "value=ProductName"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("refuses a name which is not configured", async () => {
      const { out, code } = await query("check_registry_key", ["key=nosuchname"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("resolves a configured name in any mode too", async () => {
      await setAccess("any");
      expect((await query("check_registry_value", ["key=winver", "value=ProductName"])).out).not.toMatch(/Refusing/);
      await setAccess("predefined");
    });
  });

  // --- a typo in the mode must not read as "no restriction" ------------------

  describe("an invalid mode", () => {
    it("fails closed and names the valid values", async () => {
      await setAccess("allwed", "HKLM\\SOFTWARE");
      const { out, code } = await query("check_registry_key", [`key=${CURRENT_VERSION}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/expected any, allowed or predefined/);
      await setAccess("any");
    });
  });
});
