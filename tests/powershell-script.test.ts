/**
 * Exercises the PowerShellScript module (modules/PowerShellScript +
 * libs/powershell-host) end-to-end against the real nscp binary: the module
 * starts the .NET runtime through hostfxr, loads the managed script host, which
 * loads the engine out of an installed PowerShell 7, runs a `.ps1` in a runspace
 * of its own and routes the checks it registered back into it.
 *
 * Each case runs a one-shot client query — `nscp client --module
 * PowerShellScript --boot --query <cmd>` — which boots the module with the
 * test's settings, runs the query and prints the raw Nagios result line. No
 * server/port/docker needed.
 *
 * Two pieces are not always there, and the suite reports (rather than fails)
 * when they are missing: the managed host `modules/dotnet/NSCP.PowerShell.dll`
 * is only built when the dotnet SDK was available to CMake, and the PowerShell
 * engine has to be installed on the machine running the tests. The cases that
 * need neither — the module loading with nothing configured, and the errors it
 * reports — always run.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

describe("PowerShellScript", () => {
  let hostDir: string | undefined;
  let havePwsh = false;
  let scriptDir: string;

  /** A fresh agent whose `${scripts}` is the folder this suite writes its scripts into. */
  function agent(): NscpInstance {
    return new NscpInstance({ pathOverrides: { scripts: scriptDir } });
  }

  /** Write a script into the script folder and return the file name. */
  function script(name: string, body: string): string {
    fs.writeFileSync(path.join(scriptDir, name), body, "utf8");
    return name;
  }

  async function query(
    nscp: NscpInstance,
    command: string,
    args: string[] = [],
  ): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      ["client", "--module", "PowerShellScript", "--boot", "--query", command, ...args],
      { allowFailure: true, timeout: 150_000 },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  async function exec(nscp: NscpInstance, ...args: string[]): Promise<string> {
    const r = await nscp.run(["client", "--module", "PowerShellScript", "--boot", "--exec", ...args], {
      allowFailure: true,
      timeout: 150_000,
    });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  /** True when both halves are present; otherwise says which one is not. */
  function canRunScripts(): boolean {
    if (!hostDir) {
      console.warn(
        "[powershell] modules/dotnet/NSCP.PowerShell.dll not found: the build had no dotnet SDK; skipping the script assertions",
      );
      return false;
    }
    if (!havePwsh) {
      console.warn("[powershell] no PowerShell 7 installation found on this machine; skipping the script assertions");
      return false;
    }
    return true;
  }

  beforeAll(() => {
    scriptDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-ps-"));
    const probe = new NscpInstance();
    const modulePath = probe.pathOverrides["module-path"];
    const candidates = [
      modulePath ? path.join(modulePath, "dotnet") : undefined,
      path.join(path.dirname(process.env.NSCP_BIN ?? ""), "modules", "dotnet"),
      "/usr/lib/nsclient/modules/dotnet",
    ].filter((p): p is string => !!p);
    hostDir = candidates.find((p) => fs.existsSync(path.join(p, "NSCP.PowerShell.dll")));

    // The same search the module does, cut down to what a test can check
    // cheaply: PSHOME, a pwsh on PATH, and the platform install folders.
    const engine = "System.Management.Automation.dll";
    const roots: string[] = [];
    for (const variable of ["NSCP_POWERSHELL_HOME", "PSHOME"]) {
      const value = process.env[variable];
      if (value) roots.push(value);
    }
    const exe = process.platform === "win32" ? "pwsh.exe" : "pwsh";
    for (const dir of (process.env.PATH ?? "").split(path.delimiter)) {
      if (!dir) continue;
      const candidate = path.join(dir, exe);
      if (!fs.existsSync(candidate)) continue;
      roots.push(path.dirname(fs.realpathSync(candidate)));
    }
    for (const base of [
      process.env.ProgramFiles ? path.join(process.env.ProgramFiles, "PowerShell") : undefined,
      "/opt/microsoft/powershell",
      "/usr/local/microsoft/powershell",
    ]) {
      if (!base || !fs.existsSync(base)) continue;
      for (const entry of fs.readdirSync(base)) roots.push(path.join(base, entry));
    }
    havePwsh = roots.some((root) => fs.existsSync(path.join(root, engine)));
  });

  it("loads with no scripts configured without starting a runtime", async () => {
    const { out } = await query(agent(), "check_nothing");
    // Nothing registered the command, so it cannot be routed — but the module
    // must not have complained about .NET or PowerShell for a configuration
    // that asks it to run nothing at all.
    expect(out).not.toMatch(/No \.NET runtime found/);
    expect(out).not.toMatch(/No PowerShell 7 installation found/);
    expect(out).not.toMatch(/Failed to load/i);
  });

  it("reports a script that does not exist and keeps running", async () => {
    if (!canRunScripts()) return;
    const nscp = agent();
    await nscp.configure({ "/settings/powershell/scripts": { missing: "no-such-script.ps1" } });
    const { out } = await query(nscp, "check_nothing");
    expect(out).toMatch(/PowerShell script not found: no-such-script\.ps1/);
  });

  it("answers a check a script registered", async () => {
    if (!canRunScripts()) return;
    const nscp = agent();
    script(
      "hello.ps1",
      [
        "function Check-Hello {",
        "    param([string]$command, [string[]]$arguments)",
        "    $name = 'world'",
        "    foreach ($argument in $arguments) {",
        "        if ($argument -like 'name=*') { $name = $argument.Substring(5) }",
        "    }",
        "    return @('ok', \"Hello $name ($command)\", \"'greetings'=1\")",
        "}",
        "$nscp.Registry.SimpleQuery('check_hello', 'Say hello', 'Check-Hello')",
      ].join("\n"),
    );
    await nscp.configure({ "/settings/powershell/scripts": { hello: "hello.ps1" } });
    // `k=v` as a single token is how REST passes arguments, and what the
    // one-shot client query path hands through unchanged.
    const { out, code } = await query(nscp, "check_hello", ["name=there"]);
    expect(out).toMatch(/Hello there \(check_hello\)/);
    expect(out).toMatch(/'greetings'=1/);
    expect(code).toBe(0);
  });

  it("maps the returned status onto the check result", async () => {
    if (!canRunScripts()) return;
    const nscp = agent();
    script(
      "states.ps1",
      [
        "$nscp.Registry.SimpleQuery('check_warn', 'Warn', { param($c, $a) @('warning', 'Not good') })",
        "$nscp.Registry.SimpleQuery('check_crit', 'Crit', { param($c, $a) @('critical', 'Very bad') })",
        "$nscp.Registry.SimpleQuery('check_boom', 'Boom', { param($c, $a) throw 'kaboom' })",
      ].join("\n"),
    );
    await nscp.configure({ "/settings/powershell/scripts": { states: "states.ps1" } });

    const warn = await query(nscp, "check_warn");
    expect(warn.out).toMatch(/Not good/);
    expect(warn.code).toBe(1);

    const crit = await query(nscp, "check_crit");
    expect(crit.out).toMatch(/Very bad/);
    expect(crit.code).toBe(2);

    // A script that throws is an UNKNOWN check, not a dead module.
    const boom = await query(nscp, "check_boom");
    expect(boom.out).toMatch(/kaboom/);
    expect(boom.code).toBe(3);
  });

  it("lets a script call back into the agent", async () => {
    if (!canRunScripts()) return;
    const nscp = agent();
    script(
      "inner.ps1",
      [
        "function Check-Inner {",
        "    param([string]$command, [string[]]$arguments)",
        "    return @('warning', 'inner ran', \"'inner'=7\")",
        "}",
        "$nscp.Registry.SimpleQuery('check_inner', 'Inner', 'Check-Inner')",
      ].join("\n"),
    );
    script(
      "callback.ps1",
      [
        "function Check-Callback {",
        "    param([string]$command, [string[]]$arguments)",
        "    $inner = $nscp.Core.SimpleQuery('check_inner')",
        "    $port = $nscp.Settings.GetInt('/settings/WEB/server', 'port', 0)",
        "    $nscp.Info('callback ran')",
        "    return @('ok', \"inner=$($inner.Status):$($inner.Message):$($inner.Perf) port=$port\")",
        "}",
        "function Check-Myself {",
        "    param([string]$command, [string[]]$arguments)",
        "    $own = $nscp.Core.SimpleQuery('check_callback')",
        "    return @('ok', $own.Message)",
        "}",
        "$nscp.Registry.SimpleQuery('check_callback', 'Call back', 'Check-Callback')",
        "$nscp.Registry.SimpleQuery('check_myself', 'Call itself', 'Check-Myself')",
      ].join("\n"),
    );
    await nscp.configure({
      "/settings/powershell/scripts": { callback: "callback.ps1", inner: "inner.ps1" },
      "/settings/WEB/server": { port: "8443" },
    });
    // The inner query leaves the script, goes out through the core and comes
    // back into the module - status, message and performance data intact -
    // and the script then reads a setting through the same bridge.
    const { out, code } = await query(nscp, "check_callback");
    expect(out).toMatch(/inner=warning:inner ran:'inner'=7/);
    expect(out).toMatch(/port=8443/);
    expect(code).toBe(0);

    // A runspace runs one pipeline at a time, so a script that asks for a
    // check of its own cannot be served. It has to say so and come back - a
    // check that never answers would hang whatever is watching it.
    const self = await query(nscp, "check_myself");
    expect(self.out).toMatch(/cannot run a command it answers itself/);
  });

  it("lists the loaded scripts and what they answer", async () => {
    if (!canRunScripts()) return;
    const nscp = agent();
    script("listed.ps1", "$nscp.Registry.SimpleQuery('check_listed', 'A listed check', { param($c, $a) 'ok' })");
    await nscp.configure({ "/settings/powershell/scripts": { listed: "listed.ps1" } });
    const out = await exec(nscp, "list");
    expect(out).toMatch(/Loaded PowerShell scripts:/);
    expect(out).toMatch(/listed:.*listed\.ps1/);
    expect(out).toMatch(/query check_listed - A listed check/);
  });

  it("runs a script's main function from the command line", async () => {
    if (!canRunScripts()) return;
    const nscp = agent();
    script(
      "runner.ps1",
      ["function main {", "    param([string[]]$arguments)", "    return \"main saw $arguments\"", "}"].join("\n"),
    );
    const out = await exec(nscp, "execute", "--script", "runner.ps1", "alpha", "beta");
    expect(out).toMatch(/main saw alpha beta/);
  });

  it("prints usage for an unknown or bare sub command", async () => {
    const out = await exec(agent(), "help");
    // The module answers this itself, so it works with no scripts, no dotnet
    // SDK in the build and no PowerShell on the machine — unless the managed
    // host is missing, in which case it says so instead.
    expect(out).toMatch(/Usage: nscp powershell|PowerShell script host is (missing|not loaded)/);
  });
});
