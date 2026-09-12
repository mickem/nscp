/**
 * Exercises the `nscp test` console (the CommandClient module) over its
 * non-interactive path: stdin fed from a string, stdout captured.
 *
 * There is no pty here, so the prompt itself — line editing, history,
 * completion, colours — is not what these tests cover; that half only exists
 * when stdin and stdout are both a terminal. What they do cover is everything
 * the non-interactive path has to keep doing:
 *
 *   * commands read from stdin actually run (on Windows they did not: the
 *     readiness check used GetNumberOfConsoleInputEvents, which fails on a
 *     file or pipe handle, so a redirected `nscp test` sat there silently),
 *   * log output reaches stdout as it happens rather than sitting in a buffer
 *     until something else flushes it (which on Windows used to be the next
 *     keystroke),
 *   * `exit` shuts the process down,
 *   * with stdin at end of input the process stays up, because that is how the
 *     agent is normally started under a supervisor — and how every other suite
 *     in this directory runs it.
 */
import execa from "execa";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

describe("nscp test console", () => {
  let nscp: NscpInstance;

  /** Run `nscp test`, feed it `input` on stdin, return everything it printed. */
  async function runConsole(input: string, timeout = 60_000): Promise<string> {
    const r = await nscp.run(["test"], { input, timeout, allowFailure: true });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    // CheckHelpers gives us check_ok, which is deterministic everywhere.
    await nscp.configure({ "/modules": { CheckHelpers: "enabled" } });
  });

  it("runs a query typed on stdin", async () => {
    const out = await runConsole("check_ok\nexit\n");
    expect(out).toMatch(/OK/);
  });

  it("passes arguments through in REST style (key=value as one token)", async () => {
    const out = await runConsole("check_ok message=hello-from-stdin\nexit\n");
    expect(out).toContain("hello-from-stdin");
  });

  it("prints the built-in command list for help", async () => {
    const out = await runConsole("help\nexit\n");
    // Rendered from client::builtin_commands(), which is also the list the
    // prompt completes and highlights — so this pins the two against each
    // other. The old hard-coded help text listed barely half of them.
    expect(out).toContain("Commands:");
    for (const verb of ["help", "exit", "queries", "aliases", "plugins", "desc", "load", "unload", "reload", "settings", "metrics"]) {
      expect(out).toMatch(new RegExp(`\\b${verb}\\b`));
    }
    expect(out).toContain("<any other command>");
  });

  it("lists registered queries", async () => {
    const out = await runConsole("queries\nexit\n");
    expect(out).toContain("check_ok");
  });

  it("describes a query and its parameters", async () => {
    const out = await runConsole("desc check_ok\nexit\n");
    expect(out).toContain("check_ok");
    expect(out).toMatch(/Parameters/i);
  });

  it("keeps running commands after a module reload", async () => {
    // A settings reload re-enters CommandClient::loadModuleEx on the live
    // module while the console is up. It used to replace the client object
    // the console was built on, leaving the prompt's completion hooks with a
    // freed pointer: the first refresh after a reload - a fleet configuration
    // push being what hit it - took the process down. The completion hooks
    // only exist with a terminal and cannot be driven through a pipe, so this
    // pins the half that can be: the reload lands while the console is
    // running, and what is typed afterwards still runs on an intact client.
    const overrides: string[] = [];
    for (const [k, v] of Object.entries(nscp.pathOverrides))
      overrides.push("--path-override", `${k}=${v}`);
    const proc = execa(
      process.env.NSCP_BIN as string,
      ["test", "--settings", nscp.settingsFile, ...overrides],
      {
        cwd: nscp.workDir,
        all: true,
        timeout: 60_000,
        reject: false,
        env: process.env,
      },
    );
    const stdin = proc.stdin;
    if (!stdin) throw new Error("no stdin pipe");
    stdin.write("reload\n");
    // The reload is queued ("delayed,service") and runs on a core thread a
    // moment later; give it time to complete before typing the next command
    // so that command really does run after loadModuleEx has been re-entered.
    await new Promise((resolve) => setTimeout(resolve, 5_000));
    stdin.write("check_ok message=after-reload\nexit\n");
    stdin.end();
    const r = await proc;
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toContain("after-reload");
    expect(r.timedOut).toBe(false);
    expect(r.exitCode).toBe(0);
  });

  it("exits on the exit command instead of running to the timeout", async () => {
    const r = await nscp.run(["test"], { input: "exit\n", timeout: 30_000, allowFailure: true });
    expect(r.timedOut).toBe(false);
  });

  it("streams the boot log to stdout, and stays up, with stdin already at EOF", async () => {
    // Two things at once, because one run shows both.
    //
    // The log: the console logger installs a 64k buffer on std::cout, and
    // nothing used to flush it. On Windows the whole boot log therefore sat in
    // that buffer until something else flushed the tied stream — in practice
    // the next read from stdin, i.e. the next keystroke. Here the process is
    // killed by the timeout without ever reading a command, so anything we can
    // see was flushed as it was logged.
    //
    // Staying up: end of input is not a reason to stop. Every other suite here
    // starts the agent with stdin closed and expects it to keep serving.
    const r = await nscp.run(["test"], { input: "", timeout: 15_000, allowFailure: true });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toMatch(/Started!/);
    expect(r.timedOut).toBe(true);
  });
});
