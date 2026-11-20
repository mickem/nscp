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

  // Every key under [/settings/permissions/policies] is a rule the operator
  // names; with --load-all an unclaimed key would be a warning.
  it("accepts permission rules and module names, even with --load-all", async () => {
    const nscp = withConfig(
      CLEAN +
        "\n[/settings/permissions]\nenabled = false\n" +
        "\n[/settings/permissions/policies]\nWEBServer:admin = *\nNRPEServer = CheckSystem.check_cpu\n",
    );
    const r = await nscp.run(["settings", "--validate", "--load-all"], { allowFailure: true });

    expect(r.stdout).not.toContain("permissions/policies");
    expect(r.stdout).not.toContain("[/modules]");
    expect(r.exitCode).toBe(0);
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

  // The core's own keys are registered at boot, so only a running agent knows
  // them. They were once registered with the key and its type swapped - as
  // keys named "string" and "int" - which left the real ones unknown: a typo
  // in one was passed over, and the correct spelling was itself listed as
  // read by nothing.
  it("knows the core's own keys in [/settings/core]", async () => {
    const nscp = withConfig(
      CLEAN +
        "\n[/settings/core]\n" +
        "settings maintenance interval = 7m\n" +
        "metrics interval = 20s\n" +
        "metric interval = 20s\n" +
        "diagnostics interval = 30s\n" +
        "settings maintenance threads = 1\n",
    );
    nscp.start();
    try {
      await waitForOutput(nscp, /1 entry is ignored because nothing reads it/, 60_000);
      const out = nscp.capturedStdout();
      expect(out).toContain(
        "Key 'metric interval' in [/settings/core] is not a known key. Did you mean 'metrics interval'?",
      );
      for (const key of [
        "settings maintenance interval",
        "metrics interval",
        "diagnostics interval",
        "settings maintenance threads",
      ]) {
        expect(out).not.toContain(`Key '${key}' in [/settings/core]`);
      }
    } finally {
      await nscp.stop();
    }
  });
});
