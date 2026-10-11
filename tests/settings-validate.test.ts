/**
 * Configuration nothing reads (#647): a section written without its leading
 * '/', a misspelled section or key, a key above the first section. They used
 * to be ignored without a word - a script defined under
 * [settings/external scripts/scripts] was simply "Unknown command".
 *
 * Covers the two places that now say so without a web server: `nscp settings
 * --validate` (report and exit code) and the summary `nscp test` logs once
 * every module is loaded. The REST endpoint is covered in rest-settings.
 *
 * CheckExternalScripts and CheckHelpers exist on Linux and Windows alike.
 */
import * as fs from "fs";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const BROKEN = [
  "[/modules]",
  "CheckExternalScripts = enabled",
  "CheckHelpers = enabled",
  "",
  "[/settings/external scripts]",
  "alow arguments = true",
  "",
  "[settings/external scripts/scripts]",
  "check_cert = echo ok",
  "",
  // NRPEServer is not enabled: its section is info, not a warning.
  "[/settings/NRPE/server]",
  "port = 5666",
  "",
].join("\n");

const CLEAN = [
  "[/modules]",
  "CheckExternalScripts = enabled",
  "",
  "[/settings/external scripts]",
  "allow arguments = true",
  "",
  "[/settings/external scripts/scripts]",
  "check_cert = echo ok",
  "",
].join("\n");

/** Poll the agent's stdout until `pattern` shows up (or give up). */
async function waitForOutput(
  nscp: NscpInstance,
  pattern: RegExp,
  timeoutMs: number,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (pattern.test(nscp.capturedStdout())) return;
    await new Promise((r) => setTimeout(r, 250));
  }
  throw new Error(
    `nscp did not log ${pattern} within ${timeoutMs}ms:\n${nscp.capturedStdout()}\n${nscp.capturedStderr()}`,
  );
}

function withConfig(content: string): NscpInstance {
  const nscp = new NscpInstance();
  fs.writeFileSync(nscp.settingsFile, content);
  return nscp;
}

describe("nscp settings --validate", () => {
  it("reports what nothing reads and exits with 1", async () => {
    const nscp = withConfig(BROKEN);
    const r = await nscp.run(["settings", "--validate"], { allowFailure: true });

    expect(r.exitCode).toBe(1);
    expect(r.stdout).toContain(
      "warning: Section [settings/external scripts/scripts] is ignored: settings paths start with '/'. " +
        "Did you mean [/settings/external scripts/scripts]?",
    );
    expect(r.stdout).toContain(
      "warning: Key 'alow arguments' in [/settings/external scripts] is not a known key. Did you mean 'allow arguments'?",
    );
    expect(r.stdout).toMatch(
      /info: {4}Section \[\/settings\/NRPE\/server\] is not used by any loaded module/,
    );
    expect(r.stdout).toMatch(/2 warning\(s\), \d+ info/);
  });

  it("exits with 0 for a configuration the loaded modules read", async () => {
    const nscp = withConfig(CLEAN);
    const r = await nscp.run(["settings", "--validate"], { allowFailure: true });

    expect(r.exitCode).toBe(0);
    expect(r.stdout).not.toContain("warning:");
  });

  it("with --load-all, a section no installed module claims is a warning", async () => {
    const nscp = withConfig(CLEAN + "\n[/settings/no such module]\nkey = value\n");
    const r = await nscp.run(["settings", "--validate", "--load-all"], { allowFailure: true });

    expect(r.exitCode).toBe(1);
    expect(r.stdout).toContain(
      "warning: Section [/settings/no such module] is not used by any installed module.",
    );
  });
});

describe("nscp test", () => {
  it("logs a warning naming each ignored entry once the modules are loaded", async () => {
    const nscp = withConfig(BROKEN);
    nscp.start();
    try {
      await waitForOutput(nscp, /2 entries are ignored because nothing reads them/, 60_000);
      expect(nscp.capturedStdout()).toContain("Did you mean [/settings/external scripts/scripts]?");
      expect(nscp.capturedStdout()).toContain("Did you mean 'allow arguments'?");
    } finally {
      await nscp.stop();
    }
  });
});
