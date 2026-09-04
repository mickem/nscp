/**
 * Exercises the DotnetPlugins module (modules/DotnetPlugins) end-to-end against
 * the real nscp binary: the module locates and starts the .NET runtime through
 * hostfxr, loads the managed plugin API (NSCP.Core.dll) and the C# sample
 * plugin (modules/CSharpSamplePlugin), and routes `check_dotnet` to it.
 *
 * Each case runs a one-shot client query — `nscp client --module DotnetPlugins
 * --boot --query <cmd>` — which boots the module with the test's settings, runs
 * the query and prints the raw Nagios result line. No server/port/docker needed.
 *
 * The managed parts are only built when the dotnet SDK is available to CMake,
 * so the suite checks for `modules/dotnet/NSCP.Core.dll` next to the native
 * modules and reports (rather than fails) when the build did not include them.
 * A .NET runtime (8.0 or newer) must be installed on the machine running the
 * tests.
 */
import * as fs from "fs";
import * as path from "path";
import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

describe("DotnetPlugins", () => {
  let dotnetDir: string | undefined;

  /** A fresh agent (own settings file) so each case configures only its own plugins. */
  function agent(): NscpInstance {
    return new NscpInstance();
  }

  /** Run a query through the module and return output + exit code. */
  async function query(
    nscp: NscpInstance,
    command: string,
    args: string[] = [],
  ): Promise<{ out: string; code: number }> {
    const r = await nscp.run(
      ["client", "--module", "DotnetPlugins", "--boot", "--query", command, ...args],
      {
        allowFailure: true,
        timeout: 90_000,
      },
    );
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  /** True when the managed API was built into this tree / package. */
  function haveManagedApi(): boolean {
    if (!dotnetDir) {
      console.warn(
        "[dotnetplugins] modules/dotnet/NSCP.Core.dll not found: the build had no dotnet SDK; skipping the managed assertions",
      );
      return false;
    }
    return true;
  }

  beforeAll(() => {
    const probe = agent();
    const modulePath = probe.pathOverrides["module-path"];
    const candidates = [
      modulePath ? path.join(modulePath, "dotnet") : undefined,
      path.join(path.dirname(process.env.NSCP_BIN ?? ""), "modules", "dotnet"),
      "/usr/lib/nsclient/modules/dotnet",
    ].filter((p): p is string => !!p);
    dotnetDir = candidates.find((p) => fs.existsSync(path.join(p, "NSCP.Core.dll")));
  });

  it("boots with no plugins configured", async () => {
    const { out } = await query(agent(), "check_dotnet");
    // Nothing registered check_dotnet, so the core cannot route it; the module
    // itself must not have failed to load or started a runtime for nothing.
    expect(out).not.toMatch(/Failed to load/i);
    expect(out).not.toMatch(/No \.NET runtime found/);
  });

  it("answers a query from the C# sample plugin", async () => {
    if (!haveManagedApi()) return;
    const nscp = agent();
    await nscp.configure({
      "/settings/dotnet/plugins": { "NSCP.Plugin.CSharpSample": "enabled" },
    });
    const { out, code } = await query(nscp, "check_dotnet");
    // The sample registers check_dotnet on load and answers every query with
    // a fixed OK line; the client path prints the raw message with exit code =
    // Nagios status.
    expect(out).toMatch(/Hello from C#/);
    expect(out).not.toMatch(/ERROR/);
    expect(code).toBe(0);
  });

  it("lets the plugin read settings and log through the core", async () => {
    if (!haveManagedApi()) return;
    const nscp = agent();
    await nscp.configure({
      "/settings/dotnet/plugins": { "NSCP.Plugin.CSharpSample": "enabled" },
      "/settings/WEB/server": { port: "8443" },
    });
    const { out } = await query(nscp, "check_dotnet");
    // On load the sample reads /settings/WEB/server port through
    // SettingsHelper and logs it through LogHelper: proves both directions of
    // the bridge (settings query + log) work, not just the query path.
    expect(out).toMatch(/Webserver port is: 8443/);
    expect(out).toMatch(/Hello from C#/);
  });

  it("reports a plugin assembly that does not exist and keeps running", async () => {
    if (!haveManagedApi()) return;
    const nscp = agent();
    await nscp.configure({
      "/settings/dotnet/plugins": { Missing: "enabled", "NSCP.Plugin.CSharpSample": "enabled" },
    });
    const { out, code } = await query(nscp, "check_dotnet");
    expect(out).toMatch(/Plugin Missing not found: .*Missing\.dll/);
    // The good plugin next to it still loads and answers.
    expect(out).toMatch(/Hello from C#/);
    expect(code).toBe(0);
  });

  it("reports a wrong factory class for one plugin", async () => {
    if (!haveManagedApi()) return;
    const nscp = agent();
    await nscp.configure({
      "/settings/dotnet/plugins": { "NSCP.Plugin.CSharpSample": "enabled" },
      "/settings/dotnet/plugins/NSCP.Plugin.CSharpSample": { "factory class": "No.Such.Factory" },
    });
    const { out } = await query(nscp, "check_dotnet");
    expect(out).toMatch(/Factory class No\.Such\.Factory not found/);
    expect(out).not.toMatch(/Hello from C#/);
  });
});
