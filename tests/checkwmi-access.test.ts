/**
 * Exercises the `query access` gate on CheckWMI: which WMI queries a caller
 * may ask check_wmi to run once an operator has restricted it.
 *
 * WMI reaches most of what a Windows machine knows - including the filesystem,
 * through CIM_DataFile and Win32_Directory - so on a host where callers pass
 * arguments (NRPE with `allow arguments`, or REST) an unrestricted `query=` is
 * a general read primitive running as SYSTEM. The three modes here are what
 * lets an operator narrow it.
 *
 * The case which matters most is the one where the gate could be fooled rather
 * than merely bypassed: a query whose real class differs from the one a naive
 * reading would approve. Those forms are refused outright rather than guessed
 * at, and that is asserted here.
 *
 * Each case runs a one-shot client query - `nscp client --module CheckWMI
 * --boot --query check_wmi ...` - which re-reads the settings file every time,
 * so the mode can be rewritten between cases without restarting a server.
 * WMI is Windows-only, so the suite is skipped elsewhere.
 */
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

const onWindows = process.platform === "win32" ? describe : describe.skip;

const UNKNOWN = 3;
const OS_QUERY = "SELECT Caption FROM Win32_OperatingSystem";
const SERVICE_QUERY = "SELECT Name, State FROM Win32_Service";
/** The filesystem side of WMI: what restricting by class is meant to shut out. */
const FILE_QUERY = "SELECT Name FROM CIM_DataFile WHERE Drive = 'C:' AND Path = '\\\\Windows\\\\'";

onWindows("CheckWMI query access modes", () => {
  let nscp: NscpInstance;

  async function check(args: string[]): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", "--module", "CheckWMI", "--boot", "--query", "check_wmi", ...args], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /**
   * Point [/settings/wmi] at one mode. Every key is always written so a
   * previous case's list can never leak into the next one.
   *
   * Both lists default to empty rather than to a placeholder: an empty
   * 'allowed namespaces' is what means "the default root\\cimv2 only", so a
   * placeholder there would silently refuse every case that does not name a
   * namespace.
   */
  async function setAccess(mode: string, classes = "", namespaces = ""): Promise<void> {
    await nscp.configure({
      "/settings/wmi": {
        "query access": mode,
        "allowed classes": classes,
        "allowed namespaces": namespaces,
      },
    });
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    await nscp.configure({ "/modules": { CheckWMI: "enabled" } });
  });

  // --- any: the default, and what every earlier release did ------------------

  describe("any (default)", () => {
    beforeAll(() => setAccess("any"));

    it("runs any query the caller sends", async () => {
      const { out, code } = await check([`query=${OS_QUERY}`]);
      expect(code).not.toBe(UNKNOWN);
      expect(out).not.toMatch(/Refusing/);
    });

    it("does not enforce an allow list which is configured but unused", async () => {
      await setAccess("any", "Win32_Service");
      const { out } = await check([`query=${OS_QUERY}`]);
      expect(out).not.toMatch(/Refusing/);
      await setAccess("any");
    });
  });

  // --- allowed: only queries reading a class on the list ---------------------

  describe("allowed", () => {
    beforeAll(() => setAccess("allowed", "Win32_OperatingSystem"));

    it("runs a query reading an allowed class", async () => {
      const { out } = await check([`query=${OS_QUERY}`]);
      expect(out).not.toMatch(/Refusing/);
    });

    it("refuses a query reading a class which is not allowed", async () => {
      const { out, code } = await check([`query=${SERVICE_QUERY}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing WMI class 'Win32_Service'/);
      expect(out).toMatch(/allowed classes/);
    });

    it("refuses the filesystem classes", async () => {
      const { out, code } = await check([`query=${FILE_QUERY}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing/);
    });

    it("honours a wildcard entry", async () => {
      await setAccess("allowed", "Win32_Perf*, Win32_OperatingSystem");
      expect((await check([`query=${OS_QUERY}`])).out).not.toMatch(/Refusing/);
      expect((await check([`query=${SERVICE_QUERY}`])).code).toBe(UNKNOWN);
      await setAccess("allowed", "Win32_OperatingSystem");
    });

    // A gate which cannot say for certain which class WMI will read has to
    // refuse, not guess: approving the trailing identifier of a qualified path
    // would let the query read from another namespace entirely.
    it("refuses a query whose class it cannot determine", async () => {
      for (const query of [
        "ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='C:'}",
        "SELECT * FROM root\\cimv2:Win32_OperatingSystem",
        "SELECT Caption FROM Win32_OperatingSystem; SELECT Name FROM Win32_Service",
      ]) {
        const { out, code } = await check([`query=${query}`]);
        expect(code).toBe(UNKNOWN);
        expect(out).toMatch(/Refusing query/);
      }
    });

    it("refuses a namespace other than the default when no list is set", async () => {
      const { out, code } = await check([`query=${OS_QUERY}`, "namespace=root\\securitycenter2"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing namespace/);
    });

    it("accepts a namespace on the list", async () => {
      await setAccess("allowed", "*", "root\\cimv2, root\\securitycenter2");
      const { out } = await check([`query=${OS_QUERY}`, "namespace=root\\cimv2"]);
      expect(out).not.toMatch(/Refusing/);
      await setAccess("allowed", "Win32_OperatingSystem");
    });

    // An unknown target would otherwise be taken as a bare host name, pointing
    // a restricted check at a machine of the caller's choosing.
    it("refuses a target which is not configured", async () => {
      const { out, code } = await check([`query=${OS_QUERY}`, "target=some-other-host"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/Refusing target/);
    });
  });

  // --- predefined: only queries the operator configured ----------------------

  describe("predefined", () => {
    beforeAll(async () => {
      await nscp.configure({ "/settings/wmi/queries": { os: OS_QUERY } });
      await setAccess("predefined");
    });

    it("runs a query by its configured name", async () => {
      const { out } = await check(["query=os"]);
      expect(out).not.toMatch(/Refusing/);
    });

    it("refuses a raw query", async () => {
      const { out, code } = await check([`query=${OS_QUERY}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("refuses a name which is not configured", async () => {
      const { out, code } = await check(["query=nosuchname"]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/set to predefined/);
    });

    it("resolves a configured name in any mode too", async () => {
      await setAccess("any");
      expect((await check(["query=os"])).out).not.toMatch(/Refusing/);
      await setAccess("predefined");
    });

    // A predefined query is operator-authored, so it is trusted as written -
    // it is not parsed and not held against the class list.
    it("runs a predefined query whose class is not on the allow list", async () => {
      await nscp.configure({ "/settings/wmi/queries": { svc: SERVICE_QUERY } });
      await setAccess("allowed", "Win32_OperatingSystem");
      const { out } = await check(["query=svc"]);
      expect(out).not.toMatch(/Refusing/);
      await setAccess("predefined");
    });
  });

  // --- a typo in the mode must not read as "no restriction" ------------------

  describe("an invalid mode", () => {
    it("fails closed and names the valid values", async () => {
      await setAccess("allwed", "*");
      const { out, code } = await check([`query=${OS_QUERY}`]);
      expect(code).toBe(UNKNOWN);
      expect(out).toMatch(/expected any, allowed or predefined/);
      await setAccess("any");
    });
  });
});
