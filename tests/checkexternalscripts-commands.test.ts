/**
 * Exercises the CheckExternalScripts module end-to-end against the real nscp
 * binary, over the one-shot CLI / client-query path (no server/port/docker).
 *
 * These cover the security-relevant behaviour of the module: the `ext-scr
 * install` argument-lockdown tool (cross-platform), and — on the Unix launcher —
 * the command timeout on a runaway script and the shell-fallback metacharacter
 * guard; plus output capture on the Windows launcher.
 *
 * The Unix launcher has a gtest of its own that pins the captured bytes
 * exactly, `include/process/execute_process_unix_test.cpp`. Its CMake target
 * is built `if(NOT WIN32)` and there is no w32 counterpart, so the Windows
 * block at the bottom is the only thing standing between a launcher change
 * and a silent loss of check output on Windows.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

// The timeout / shell-fallback cases drive the POSIX launcher (fork/execvp,
// /bin/sh, /bin/echo), so they only run off Windows.
const onUnix = process.platform === "win32" ? describe.skip : describe;
// ...and the capture cases drive the Windows one (CreatePipe, the
// STARTUPINFOEX inherit list, the chunked ReadFile drain).
const onWindows = process.platform === "win32" ? describe : describe.skip;

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

onUnix("CheckExternalScripts — command timeout enforcement (POSIX launcher)", () => {
  // A runaway script must be killed at the configured timeout and reported, not
  // left running. On Unix the shell-fallback path used to run through popen(),
  // which hid the child pid and blocked forever with the timeout unenforced;
  // both paths now go through the same fork/exec + deadline machinery.
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

  it("hands the script only its own stdio, not the service's descriptors", async () => {
    // The pipe is created close-on-exec and the child closes everything above
    // stderr before the exec, so nothing the service holds (sockets, the log
    // file, another script's pipe) is the script's to read or write. `ls` lists
    // its own descriptor table: 0-2 plus the directory it is reading (3).
    if (!fs.existsSync("/proc/self/fd")) return;
    fs.writeFileSync(path.join(scriptsDir, "fds.sh"), "#!/bin/sh\nls /proc/self/fd\n", {
      mode: 0o755,
    });
    await nscp.configure({
      "/modules": { CheckExternalScripts: "enabled" },
      "/settings/external scripts": { timeout: "10" },
      "/settings/external scripts/scripts": {
        check_fds: `/bin/sh ${path.join(scriptsDir, "fds.sh")}`,
      },
    });

    const { out } = await query("check_fds");

    const fds = out
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => /^\d+$/.test(line))
      .map(Number);
    expect(fds).toEqual(expect.arrayContaining([0, 1, 2]));
    expect(fds.filter((fd) => fd > 3)).toEqual([]);
  });
});

onUnix("CheckExternalScripts — shell-fallback metacharacter guard (POSIX launcher)", () => {
  // When a command template is not argv-safe (e.g. it contains a backslash the
  // tokeniser rejects), the command degrades to the shell fallback and the
  // stricter SHELL_METACHARS set is applied to user arguments. `%` and `^`
  // (cmd.exe variable expansion / escape) must be blocked there.
  let nscp: NscpInstance;

  beforeAll(() => {
    nscp = new NscpInstance();
    // A backslash escape (`\x`) makes the template tokeniser throw, forcing the
    // shell-fallback path where the SHELL_METACHARS guard runs. Arguments are
    // allowed but nasty characters are not. CSimpleIni stores values verbatim,
    // so the backslash survives into the stored command template.
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

onWindows("CheckExternalScripts — output capture (Windows launcher)", () => {
  // Everything here asserts on the *content* the agent read back, not merely
  // that the command succeeded: a launcher that hands the child the wrong
  // pipe end, or that stops draining early, still "works" by every other
  // measure while returning truncated or empty output.
  let nscp: NscpInstance;
  let scriptsDir: string;

  const MARKER = "nscp-win-capture-7c1f";
  // Comfortably past the launcher's ~4 KiB read chunk, so the drain loop has
  // to run many times and stitch the pieces back together in order.
  const BIG_LINES = 200;
  const BIG_FILL = "A".repeat(200);
  const lineTag = (n: number) => `LINE${String(n).padStart(3, "0")}`;

  const script = (name: string) => path.join(scriptsDir, name);

  beforeAll(() => {
    scriptsDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extscr-win-"));
    fs.writeFileSync(script("hello.bat"), `@echo off\r\necho OK: ${MARKER}\r\n`);
    fs.writeFileSync(script("stderr.bat"), `@echo off\r\necho ${MARKER}-on-stderr 1>&2\r\n`);
    fs.writeFileSync(
      script("critical.bat"),
      `@echo off\r\necho ${MARKER}-critical\r\nexit /b 2\r\n`,
    );
    const big = ["@echo off"];
    for (let i = 1; i <= BIG_LINES; i++) {
      big.push(`echo ${lineTag(i)}-${BIG_FILL}`);
    }
    fs.writeFileSync(script("big.bat"), `${big.join("\r\n")}\r\n`);
    // ping is the batch-file sleep that needs no console: -n 31 waits ~30s.
    fs.writeFileSync(
      script("sleeper.bat"),
      `@echo off\r\nping -n 31 127.0.0.1 >nul\r\necho done\r\n`,
    );
    nscp = new NscpInstance();
  });

  afterAll(() => {
    fs.rmSync(scriptsDir, { recursive: true, force: true });
  });

  async function configure(scripts: Record<string, string>, timeout = "30") {
    await nscp.configure({
      "/modules": { CheckExternalScripts: "enabled" },
      "/settings/external scripts": { timeout },
      "/settings/external scripts/scripts": scripts,
    });
  }

  /** Boot the module and run one script command via the client-query path. */
  async function query(command: string) {
    const r = await nscp.run(
      ["client", "--module", "CheckExternalScripts", "--boot", "--query", command],
      {
        allowFailure: true,
      },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  it("reads a script's output back verbatim", async () => {
    await configure({ check_hello: `cmd /c ${script("hello.bat")}` });

    const { out, code } = await query("check_hello");

    expect(out).toContain(`OK: ${MARKER}`);
    expect(code).toBe(0);
  });

  it("reads output from a script the launcher starts directly", async () => {
    // No `cmd /c` and no backslash, so the template tokenises and the argv
    // path is taken: CreateProcess with lpApplicationName and the inherit
    // list, rather than the legacy single-string command line.
    await configure({ check_direct: script("hello.bat").replace(/\\/g, "/") });

    const { out } = await query("check_direct");

    expect(out).toContain(`OK: ${MARKER}`);
  });

  it("reads output far larger than one read buffer, complete and in order", async () => {
    await configure({ check_big: `cmd /c ${script("big.bat")}` });

    const { out } = await query("check_big");

    // Every line is present exactly once, so nothing was dropped between two
    // reads of the pipe...
    expect(out.match(/LINE\d{3}-/g) ?? []).toHaveLength(BIG_LINES);
    // ...the first and the last both survived...
    expect(out).toContain(`${lineTag(1)}-${BIG_FILL}`);
    expect(out).toContain(`${lineTag(BIG_LINES)}-${BIG_FILL}`);
    // ...and they came back in the order the script printed them.
    expect(out.indexOf(`${lineTag(1)}-`)).toBeLessThan(out.indexOf(`${lineTag(BIG_LINES)}-`));
  });

  it("captures what a script writes to stderr", async () => {
    await configure({ check_stderr: `cmd /c ${script("stderr.bat")}` });

    const { out } = await query("check_stderr");

    expect(out).toContain(`${MARKER}-on-stderr`);
  });

  it("maps a script's exit code to the check status", async () => {
    await configure({ check_critical: `cmd /c ${script("critical.bat")}` });

    const { out, code } = await query("check_critical");

    expect(out).toContain(`${MARKER}-critical`);
    expect(code).toBe(2); // CRITICAL
  });

  it("kills a script that exceeds the timeout and reports it", async () => {
    await configure({ check_sleeper: `cmd /c ${script("sleeper.bat")}` }, "2");

    const started = Date.now();
    const { out } = await query("check_sleeper");
    const elapsed = (Date.now() - started) / 1000;

    // Enforced near the 2s timeout, nowhere near the script's 30s ping.
    expect(elapsed).toBeLessThan(25);
    expect(out).toMatch(/did.?n.?t terminate|timeout/i);
  });
});
