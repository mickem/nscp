/**
 * Exercises the CheckExternalScripts module end-to-end against the real nscp
 * binary, over the one-shot CLI / client-query path (no server/port/docker).
 *
 * These cover the security-relevant behaviour of the module: the `ext-scr
 * install` argument-lockdown tool, the argument metacharacter guard, and the
 * timeout enforcement on a runaway script. They are cross-platform except where
 * a case explicitly guards on the OS.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32";

describe("CheckExternalScripts — ext-scr install argument lockdown (settings path)", () => {
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
  });

  /** Read a single settings value back from the store. */
  async function getSetting(settingsPath: string, key: string): Promise<string> {
    const r = await nscp.run(["settings", "--show", "--path", settingsPath, "--key", key], { allowFailure: true });
    return (r.all ?? `${r.stdout}\n${r.stderr}`).trim();
  }

  it("`ext-scr install --arguments=false` actually disables arguments on the path the module reads", async () => {
    // Start from a permissive config written where the module actually reads it.
    await nscp.configure({
      "/settings/external scripts": { "allow arguments": "true", "allow nasty characters": "true" },
    });

    // The lockdown tool must clear it on the same path. Before the fix it wrote
    // to `/settings/external scripts/server`, which nothing reads, so the
    // permissive value below stayed in force — a fail-dangerous no-op.
    const r = await nscp.run(["ext-scr", "install", "--arguments=false"], { allowFailure: true });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toMatch(/Arguments are NOT allowed/i);

    expect(await getSetting("/settings/external scripts", "allow arguments")).toMatch(/false/i);
    expect(await getSetting("/settings/external scripts", "allow nasty characters")).toMatch(/false/i);
  });

  it("`ext-scr install --arguments=safe` enables arguments but not nasty characters", async () => {
    await nscp.configure({
      "/settings/external scripts": { "allow arguments": "false", "allow nasty characters": "false" },
    });

    const r = await nscp.run(["ext-scr", "install", "--arguments=safe"], { allowFailure: true });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toMatch(/SAFE Arguments are allowed/i);

    expect(await getSetting("/settings/external scripts", "allow arguments")).toMatch(/true/i);
    expect(await getSetting("/settings/external scripts", "allow nasty characters")).toMatch(/false/i);
  });
});

describe("CheckExternalScripts — command timeout enforcement", () => {
  // A runaway script must be killed at the configured timeout and reported, not
  // left running. On Unix the shell-fallback path used to run through popen(),
  // which hid the child pid and blocked forever with the timeout unenforced;
  // both paths now go through the same fork/exec + deadline machinery.
  if (onWindows) {
    it.skip("timeout enforcement (Unix launcher only)", () => undefined);
    return;
  }

  let nscp: NscpInstance;
  let scriptsDir: string;

  beforeAll(() => {
    scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extscr-"));
    fs.writeFileSync(path.join(scriptsDir, "sleeper.sh"), "#!/bin/sh\nsleep 30\necho done\n", { mode: 0o755 });
    fs.writeFileSync(path.join(scriptsDir, "hello.sh"), "#!/bin/sh\necho hello-from-script\n", { mode: 0o755 });

    nscp = new NscpInstance();
  });

  afterAll(() => {
    fs.rmSync(scriptsDir, { recursive: true, force: true });
  });

  /** Boot the module and run one script command via the client-query path. */
  async function query(command: string) {
    const r = await nscp.run(["client", "--module", "CheckExternalScripts", "--boot", "--query", command], { allowFailure: true });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  it("kills a script that exceeds the timeout and reports it", async () => {
    await nscp.configure({
      "/modules": { CheckExternalScripts: "enabled" },
      "/settings/external scripts": { timeout: "2" },
      "/settings/external scripts/scripts": { check_sleeper: `/bin/sh ${path.join(scriptsDir, "sleeper.sh")}` },
    });

    const started = Date.now();
    const { out } = await query("check_sleeper");
    const elapsed = (Date.now() - started) / 1000;

    // Enforced near the 2s timeout, nowhere near the script's 30s sleep.
    expect(elapsed).toBeLessThan(20);
    expect(out).toMatch(/did.?n.?t terminate|timeout/i);
  });

  it("runs a fast script to completion and returns its output", async () => {
    await nscp.configure({
      "/modules": { CheckExternalScripts: "enabled" },
      "/settings/external scripts": { timeout: "10" },
      "/settings/external scripts/scripts": { check_hello: `/bin/sh ${path.join(scriptsDir, "hello.sh")}` },
    });

    const { out } = await query("check_hello");
    expect(out).toMatch(/hello-from-script/);
  });
});

describe("CheckExternalScripts — shell-fallback metacharacter guard", () => {
  // When a command template is not argv-safe (e.g. it contains a backslash the
  // tokeniser rejects), the command degrades to the shell fallback and the
  // stricter SHELL_METACHARS set is applied to user arguments. `%` and `^`
  // (cmd.exe variable expansion / escape) must be blocked there.
  if (onWindows) {
    it.skip("shell-fallback guard (Unix launcher only)", () => undefined);
    return;
  }

  let nscp: NscpInstance;

  beforeAll(async () => {
    nscp = new NscpInstance();
    // A backslash escape (`\x`) makes the template tokeniser throw, forcing the
    // shell-fallback path where the SHELL_METACHARS guard runs. Arguments are
    // allowed but nasty characters are not.
    const ini = [
      "[/modules]",
      "CheckExternalScripts = enabled",
      "",
      "[/settings/external scripts]",
      "allow arguments = true",
      "allow nasty characters = false",
      "",
      "[/settings/external scripts/scripts]",
      "check_fallback = /bin/echo not\\xargv $ARG1$",
      "",
    ].join("\n");
    fs.writeFileSync(nscp.settingsFile, ini);
  });

  async function query(arg: string) {
    const r = await nscp.run(["client", "--module", "CheckExternalScripts", "--boot", "--query", "check_fallback", arg], { allowFailure: true });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  it.each(["pct%val", "car^et"])("rejects an argument containing a cmd.exe metacharacter (%s)", async (arg) => {
    const out = await query(arg);
    expect(out).toMatch(/illegal characters/i);
  });

  it("still runs a clean argument through the shell fallback", async () => {
    const out = await query("plainvalue");
    expect(out).toMatch(/plainvalue/);
    expect(out).not.toMatch(/illegal characters/i);
  });
});
