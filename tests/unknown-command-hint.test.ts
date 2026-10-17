/**
 * What the core says about a command no loaded module registers.
 *
 * The build writes a command -> module map from every built module's
 * module.json, so `check_ping` without `CheckNet = enabled` (#698) no longer
 * answers a bare "No handler for command": the reply names the module and what
 * to do about it. The existing text stays the prefix, so scripts matching on it
 * still work.
 *
 * One-shot client queries (`nscp client --boot --query <cmd>`): no server, no
 * port. CheckNet is built on every platform, which is what makes check_ping a
 * deterministic subject.
 */
import * as fs from "fs";
import * as os from "os";
import * as path from "path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(120_000);

const onWindows = process.platform === "win32";

describe("unknown command hint", () => {
  let nscp: NscpInstance;

  async function query(
    command: string,
    extra: string[] = [],
  ): Promise<{ out: string; code: number }> {
    const r = await nscp.run(["client", ...extra, "--boot", "--query", command], {
      allowFailure: true,
    });
    return { out: r.all ?? `${r.stdout}\n${r.stderr}`, code: r.exitCode };
  }

  beforeAll(async () => {
    nscp = new NscpInstance();
    // Explicitly off, so the result does not depend on any default.
    await nscp.configure({ "/modules": { CheckNet: "disabled", CheckHelpers: "enabled" } });
  });

  it("names the module to enable for a command whose module is not enabled", async () => {
    const { out, code } = await query("check_ping");
    expect(out).toContain(
      'No handler for command: check_ping (provided by module CheckNet, which is not loaded: add "CheckNet = enabled" under [/modules] and restart)',
    );
    expect(code).toBe(3);
  });

  it("matches the command case-insensitively, like the registry", async () => {
    const { out } = await query("CHECK_PING");
    expect(out).toContain("(provided by module CheckNet, which is not loaded");
  });

  it("says nothing more for a command no built module declares", async () => {
    const { out, code } = await query("no_such_command_anywhere");
    expect(out).toContain("No handler for command: no_such_command_anywhere");
    expect(out).not.toContain("provided by module");
    expect(code).toBe(3);
  });

  it("tells an enabled module that was not loaded apart from one that is not enabled", async () => {
    // `--module CheckHelpers` loads only that module, so CheckNet - enabled
    // in the settings - is not there to answer.
    const enabled = new NscpInstance();
    await enabled.configure({ "/modules": { CheckNet: "enabled", CheckHelpers: "enabled" } });
    const r = await enabled.run(
      ["client", "--module", "CheckHelpers", "--boot", "--query", "check_ping"],
      { allowFailure: true },
    );
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toContain(
      "No handler for command: check_ping (provided by module CheckNet, which is enabled but not loaded)",
    );
  });

  it("says when the module is enabled but failed to load", async () => {
    // A module path holding a CheckNet that is not a library at all: the
    // load fails, deterministically, on every platform.
    const modules = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-broken-modules-"));
    fs.writeFileSync(
      path.join(modules, onWindows ? "CheckNet.dll" : "libCheckNet.so"),
      "not a library",
    );
    const broken = new NscpInstance({ pathOverrides: { "module-path": modules } });
    await broken.configure({ "/modules": { CheckNet: "enabled" } });
    const r = await broken.run(["client", "--boot", "--query", "check_ping"], {
      allowFailure: true,
    });
    const out = r.all ?? `${r.stdout}\n${r.stderr}`;
    expect(out).toContain(
      "No handler for command: check_ping (provided by module CheckNet, which is enabled but failed to load: see the log for why)",
    );
  });
});
