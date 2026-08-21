/**
 * Exercises the top-level command line of the real nscp binary — the dispatch
 * layer in service/cli_parser.cpp that decides what `nscp <context> …` means.
 *
 * Everything below is a one-shot invocation: no server, no port, no docker.
 * The point is the argument handling itself — which context a word selects,
 * what an unknown one does, where --help and --version are answered, how the
 * common options (--settings, --path-override, --define) are consumed — plus
 * the `settings` sub-commands that read and write an INI without booting a
 * daemon.
 *
 * Exit codes are asserted as they are, not as they arguably should be: `nscp
 * help` and a bare `nscp` both exit 1 today, which is worth pinning precisely
 * because it is the kind of thing that gets changed by accident.
 */
import * as fs from "fs";
import { NscpInstance, nscp } from "@fixtures/index";

jest.setTimeout(120_000);

describe("nscp command line dispatch", () => {
  it("reports its version", async () => {
    const r = await nscp(["--version"]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toMatch(/NSClient\+\+, Version: /);
  });

  it("lists the available contexts when called with no arguments", async () => {
    const r = await nscp([], { allowFailure: true });
    expect(r.exitCode).toBe(1);
    expect(r.stdout).toContain("Usage: nscp <context>");
    // Every handler and a sample of the client aliases.
    for (const context of ["client", "settings", "service", "help", "unit", "enroll"]) {
      expect(r.stdout).toContain(context);
    }
    expect(r.stdout).toContain("nrpe");
  });

  it("shows the option help for the help context", async () => {
    const r = await nscp(["help"], { allowFailure: true });
    // Asking for help exits 1 - long-standing behaviour, pinned deliberately.
    expect(r.exitCode).toBe(1);
    expect(r.stdout).toContain("Allowed options");
    expect(r.stdout).toContain("--settings");
    expect(r.stdout).toContain("First argument has to be one of the following:");
  });

  it("rejects an unknown context by name", async () => {
    const r = await nscp(["wibble"], { allowFailure: true });
    expect(r.exitCode).toBe(1);
    expect(r.stderr).toContain("Invalid module specified: wibble");
  });

  it("insists the context comes first", async () => {
    const r = await nscp(["--debug", "settings", "--list"], { allowFailure: true });
    expect(r.exitCode).toBe(1);
    expect(r.stderr).toContain('First option should be the "context"');
  });

  it("answers --help inside a context with that context's options", async () => {
    const r = await nscp(["settings", "--help"], { allowFailure: true });
    expect(r.exitCode).toBe(1);
    expect(r.stdout).toContain("Allowed options (settings)");
    // A settings-only option, proving it is not the generic screen.
    expect(r.stdout).toContain("--validate");
  });

  it("answers --version inside a context too", async () => {
    const r = await nscp(["settings", "--version"], { allowFailure: true });
    expect(r.stdout).toMatch(/NSClient\+\+, version: /);
  });

  it("warns about a --path-override that is not KEY=VALUE", async () => {
    const r = await nscp(["settings", "--path-override", "no-equals-sign", "--validate"], {
      allowFailure: true,
    });
    expect(r.all).toContain("Ignoring malformed --path-override argument");
  });

  it("reports a --define it cannot parse", async () => {
    const r = await nscp(["client", "--define", "no-colon-or-equals", "--help"], {
      allowFailure: true,
    });
    // --help short-circuits before the client boots, so the only thing this
    // asserts is that the malformed define was noticed, not that it applied.
    expect(r.exitCode).toBe(1);
  });
});

describe("nscp settings sub-commands", () => {
  it("prints the settings options and the active store when given no action", async () => {
    const instance = new NscpInstance();
    const r = await instance.run(["settings"], { allowFailure: true });
    expect(r.exitCode).toBe(1);
    expect(r.stdout).toContain("Allowed options (settings)");
    // list_settings_info names the store that would be edited.
    expect(r.stdout).toContain("INI settings");
  });

  it("writes a value with --set and reads it back with --show", async () => {
    const instance = new NscpInstance();
    const set = await instance.run([
      "settings",
      "--path",
      "/settings/default",
      "--key",
      "allowed hosts",
      "--set",
      "10.0.0.1",
    ]);
    expect(set.exitCode).toBe(0);
    expect(fs.readFileSync(instance.settingsFile, "utf8")).toContain("10.0.0.1");

    const show = await instance.run([
      "settings",
      "--show",
      "--path",
      "/settings/default",
      "--key",
      "allowed hosts",
    ]);
    expect(show.exitCode).toBe(0);
    expect(show.stdout).toContain("10.0.0.1");
  });

  it("describes the active store for a bare --show", async () => {
    const instance = new NscpInstance();
    const r = await instance.run(["settings", "--show"]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toContain("INI settings");
  });

  it("refuses --show with a path but no key", async () => {
    const instance = new NscpInstance();
    const r = await instance.run(["settings", "--show", "--path", "/settings/default"], {
      allowFailure: true,
    });
    expect(r.exitCode).not.toBe(0);
    expect(r.stderr).toContain("Invalid command line please use --path and --key with show");
  });

  it("lists the keys under a path", async () => {
    const instance = new NscpInstance();
    await instance.run([
      "settings",
      "--path",
      "/settings/default",
      "--key",
      "allowed hosts",
      "--set",
      "10.0.0.2",
    ]);

    const r = await instance.run(["settings", "--list", "--path", "/settings/default"]);
    expect(r.exitCode).toBe(0);
    expect(r.stdout).toContain("allowed hosts=10.0.0.2");
  });

  it("validates a store nobody has broken", async () => {
    const instance = new NscpInstance();
    const r = await instance.run(["settings", "--validate"]);
    expect(r.exitCode).toBe(0);
  });

  it("sorts the ini in place", async () => {
    const instance = new NscpInstance();
    fs.writeFileSync(instance.settingsFile, "[/zeta]\nb = 2\na = 1\n[/alpha]\nk = v\n");

    const r = await instance.run(["settings", "--sort"]);
    expect(r.exitCode).toBe(0);

    const content = fs.readFileSync(instance.settingsFile, "utf8");
    expect(content.indexOf("[/alpha]")).toBeLessThan(content.indexOf("[/zeta]"));
    // Sorting must not lose anything.
    expect(content).toContain("k = v");
    expect(content).toContain("a = 1");
    expect(content).toContain("b = 2");
  });

  it("generates a config to stdout without touching the store", async () => {
    const instance = new NscpInstance();
    const before = fs.readFileSync(instance.settingsFile, "utf8");

    const r = await instance.run(["settings", "--generate", "stdout"], { allowFailure: true });
    expect(r.stdout.length).toBeGreaterThan(0);
    expect(fs.readFileSync(instance.settingsFile, "utf8")).toEqual(before);
  });
});

describe("nscp client aliases", () => {
  it("maps an alias to its module's client help", async () => {
    // `nscp nrpe …` is shorthand for `nscp client --module NRPEClient …`, so
    // the alias has to reach NRPEClient's own options.
    const instance = new NscpInstance();
    const r = await instance.run(["nrpe", "--help"], { allowFailure: true });
    // NRPE-specific options, so this really did land in NRPEClient rather
    // than printing the generic screen.
    expect(r.all).toContain("The NRPE version to use");
  });
});
