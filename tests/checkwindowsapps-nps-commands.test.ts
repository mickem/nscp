/** Native dispatch and documented unavailable-data contracts on any Windows host. */
import { NscpInstance, describeOnWindows } from "@fixtures/index";

jest.setTimeout(120_000);
describeOnWindows("CheckWindowsApps NPS commands", () => {
  let nscp: NscpInstance;
  beforeAll(() => {
    nscp = new NscpInstance();
  });
  async function query(command: string, args: string[] = []) {
    const result = await nscp.run(
      ["client", "--module", "CheckWindowsApps", "--boot", "--query", command, ...args],
      { allowFailure: true },
    );
    return result.all ?? `${result.stdout}\n${result.stderr}`;
  }
  it.each(["check_nps_auth", "check_nps_accounting", "check_nps_counters"])(
    "dispatches %s and reports actual data or unavailability",
    async (command) => {
      const output = await query(command);
      expect(output).not.toMatch(/Unknown command|Failed to load|does not take any arguments/);
      expect(output).toMatch(/accepted|accounting discards|RADIUS|NPS .*unavailable/);
    },
  );
  it.each(["check_nps_auth", "check_nps_accounting"])(
    "%s accepts valued booleans",
    async (command) => {
      const output = await query(command, ["require-traffic=false", "window=1"]);
      expect(output).not.toMatch(/does not take any arguments|Failed to parse|Unknown command/);
      expect(output).toMatch(/accepted|accounting discards|NPS .*unavailable/);
    },
  );
  it("validates the scan window and grouping before accessing the logs", async () => {
    expect(await query("check_nps_auth", ["window=0"])).toMatch(/window must be/);
    expect(await query("check_nps_auth", ["group-by=bad"])).toMatch(/group-by must be/);
    expect(await query("check_nps_auth", ["group-by=client", "require-traffic=true"])).toMatch(
      /require-traffic requires group-by=all/,
    );
  });
  it("validates accounting freshness prerequisites and sample interval", async () => {
    expect(await query("check_nps_accounting", ["require-traffic=true"])).toMatch(
      /require-traffic needs log-file/,
    );
    expect(await query("check_nps_counters", ["sample-ms=0"])).toMatch(/sample-ms must be/);
  });
});
