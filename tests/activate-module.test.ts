/**
 * Verifies `nscp settings --activate-module` accepts MULTIPLE module names in a
 * single invocation — the regression guard for the vector/multitoken CLI option
 * (service/cli_parser.cpp) that lets you enable several modules in one line
 * instead of a shell loop.
 *
 * Each case runs a one-shot `nscp settings --activate-module <A> <B> ...`
 * against a throwaway settings file and inspects the resulting [/modules]
 * section. No server/port/docker needed.
 *
 * The modules used (CheckHelpers, CheckSystem, CheckDisk) exist on both Linux
 * and Windows — the Unix variants (CheckSystemUnix / CheckDiskUnix) register
 * under the same names — so the suite runs on both platforms.
 */
import * as fs from "fs";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

/** The module keys set to `enabled` in an nsclient.ini's [/modules] section. */
function enabledModules(iniPath: string): Set<string> {
  const enabled = new Set<string>();
  let inModules = false;
  for (const raw of fs.readFileSync(iniPath, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    if (line === "" || line.startsWith(";")) continue;
    if (line.startsWith("[")) {
      inModules = line === "[/modules]";
      continue;
    }
    if (!inModules) continue;
    const m = line.match(/^(\S+)\s*=\s*(\S+)/);
    if (m && m[2] === "enabled") enabled.add(m[1]);
  }
  return enabled;
}

describe("settings --activate-module", () => {
  it("activates several modules in one invocation", async () => {
    const nscp = new NscpInstance();
    const r = await nscp.run([
      "settings",
      "--activate-module",
      "CheckHelpers",
      "CheckSystem",
      "CheckDisk",
    ]);
    expect(r.exitCode).toBe(0);
    // Exactly the three requested modules are enabled — proving all names past
    // the first were consumed (the old single-string option dropped them).
    expect(enabledModules(nscp.settingsFile)).toEqual(
      new Set(["CheckHelpers", "CheckSystem", "CheckDisk"]),
    );
  });

  it("still accepts a single module", async () => {
    const nscp = new NscpInstance();
    const r = await nscp.run(["settings", "--activate-module", "CheckHelpers"]);
    expect(r.exitCode).toBe(0);
    expect(enabledModules(nscp.settingsFile).has("CheckHelpers")).toBe(true);
  });

  it("activates the loadable modules and reports failure when one is unknown", async () => {
    const nscp = new NscpInstance();
    const r = await nscp.run(["settings", "--activate-module", "CheckHelpers", "NoSuchModule"], {
      allowFailure: true,
    });
    // A module that can't be loaded makes the command exit non-zero...
    expect(r.exitCode).not.toBe(0);
    // ...but the loadable ones alongside it are still activated, and the
    // bogus one is not written to the config.
    const enabled = enabledModules(nscp.settingsFile);
    expect(enabled.has("CheckHelpers")).toBe(true);
    expect(enabled.has("NoSuchModule")).toBe(false);
  });
});
