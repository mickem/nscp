/**
 * `script root`, every spelling it takes, in every module that reads it.
 *
 * The key names the scripts folder: `${scripts}` by default, `fleet` for the
 * fleet tree, `shared` for `${shared-path}/scripts`, or a folder of one's
 * own. Python and Lua read its `python` and `lua` sub-folders; CheckMKClient
 * and CheckMKServer load `lua/default_check_mk.lua` from it; and an external
 * script is started in the folder *above* it, which is where `scripts/x.sh`
 * is spelled from. fleet-sync-scripts.test.ts proves the fleet case against a
 * bundle; this suite proves the setting itself, one tree per spelling, with
 * the same four probes - and one tree that holds nothing, so the probes are
 * shown to depend on the root rather than on the build's own scripts folder.
 *
 * Everything runs over the one-shot `nscp client --module <M> --boot` path:
 * the module loads from the ini, the query runs, and what the load logged is
 * in the output. The root is set once, under [/settings/default], the way a
 * bundle sets it for every script module at once.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance, itWithModules, moduleBuiltHere, nscpBin, onWindows } from "@fixtures/index";

jest.setTimeout(240_000);

const PY_HELPER = "python helper from the script root";
const LUA_HELPER = "lua helper from the script root";
const MK_LOADED = "root probe: default_check_mk loaded";

/** The probe tree: a script of each kind, each depending on a file beside it. */
function writeTree(scripts: string): void {
  const write = (rel: string, body: string) => {
    const file = path.join(scripts, rel);
    fs.mkdirSync(path.dirname(file), { recursive: true });
    fs.writeFileSync(file, body);
  };
  write("root_probe.sh", '#!/bin/sh\necho "ext probe ok cwd=$(pwd -P)"\n');
  write("root_probe.bat", "@echo off\r\necho ext probe ok cwd=%CD%\r\n");
  write(
    "python/root_probe.py",
    [
      "from NSCP import Registry, status",
      "import root_helper",
      "",
      "def init(plugin_id, plugin_alias, script_alias):",
      "    Registry.get(plugin_id).simple_function('root_probe_py', lambda a: (status.OK, root_helper.greeting()), 'root probe')",
      "",
    ].join("\n"),
  );
  write("python/lib/root_helper.py", `def greeting():\n    return "${PY_HELPER}"\n`);
  write(
    "lua/root_probe.lua",
    [
      'local helper = require("root_helper")',
      "Registry():simple_query('root_probe_lua', function(command, args)",
      "\treturn 'ok', helper.greeting(), ''",
      "end, 'root probe')",
      "",
    ].join("\n"),
  );
  write(
    "lua/lib/root_helper.lua",
    `local M = {}\nfunction M.greeting()\n\treturn "${LUA_HELPER}"\nend\nreturn M\n`,
  );
  // What the CheckMK modules auto-load when nothing else is configured: a
  // stand-in that only says where it was loaded from.
  write("lua/default_check_mk.lua", `nscp.print('${MK_LOADED}')\n`);
}

interface RootCase {
  name: string;
  /** The `script root` value; undefined leaves the key unset. */
  value?: string;
  /** Where the tree goes, and the path overrides that make the value name it. */
  place: (workDir: string) => { scripts: string; overrides: Record<string, string> };
}

const cases: RootCase[] = [
  {
    name: "unset",
    place: (w) => ({
      scripts: path.join(w, "scripts"),
      overrides: { scripts: path.join(w, "scripts") },
    }),
  },
  {
    name: "`default` spelled out",
    value: "default",
    place: (w) => ({
      scripts: path.join(w, "scripts"),
      overrides: { scripts: path.join(w, "scripts") },
    }),
  },
  {
    name: "`${scripts}` written out (the old default, as the docs once suggested)",
    value: "${scripts}",
    place: (w) => ({
      scripts: path.join(w, "scripts"),
      overrides: { scripts: path.join(w, "scripts") },
    }),
  },
  {
    name: "`shared`",
    value: "shared",
    place: (w) => ({
      scripts: path.join(w, "shared", "scripts"),
      overrides: { "shared-path": path.join(w, "shared") },
    }),
  },
  {
    name: "`fleet`",
    value: "fleet",
    // ${fleet-folder} is ${data-path}/fleet on unix and ${shared-path}/fleet on
    // Windows; the instance pins data-path to the work dir already.
    place: (w) => ({ scripts: path.join(w, "fleet", "scripts"), overrides: { "shared-path": w } }),
  },
  {
    name: "a folder of its own, not called scripts",
    value: "__FOLDER__",
    place: (w) => ({ scripts: path.join(w, "mine", "custom-name"), overrides: {} }),
  },
];

/** One agent per root spelling: the tree, the ini, and the one-shot runs. */
class Probe {
  readonly workDir: string;
  readonly scripts: string;
  readonly value?: string;
  readonly nscp: NscpInstance;

  constructor(c: RootCase, empty = false) {
    this.workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-script-root-"));
    const placed = c.place(this.workDir);
    this.scripts = placed.scripts;
    this.value = c.value === "__FOLDER__" ? this.scripts : c.value;
    fs.mkdirSync(this.scripts, { recursive: true });
    if (!empty) writeTree(this.scripts);
    this.nscp = new NscpInstance({ workDir: this.workDir, pathOverrides: placed.overrides });
  }

  /** Whether the key is set: the Windows built-in layout keeps the install directory as the start directory. */
  get configured(): boolean {
    return (
      this.value !== undefined &&
      this.value.trim() !== "" &&
      this.value.trim().toLowerCase() !== "default"
    );
  }

  /** The directory an external script is started in. */
  get startDir(): string {
    return onWindows && !this.configured ? path.dirname(nscpBin()) : path.dirname(this.scripts);
  }

  /** The command an external script is registered as: relative where the start directory holds `scripts`, absolute otherwise. */
  get extCommand(): string {
    const relative =
      path.basename(this.scripts) === "scripts" &&
      path.resolve(this.startDir, "scripts") === path.resolve(this.scripts);
    if (onWindows)
      return relative ? "scripts\\root_probe.bat" : path.join(this.scripts, "root_probe.bat");
    return relative ? "sh scripts/root_probe.sh" : `sh ${path.join(this.scripts, "root_probe.sh")}`;
  }

  async configure(extra: Record<string, Record<string, string>> = {}): Promise<void> {
    const tree: Record<string, Record<string, string>> = { ...extra };
    if (this.value !== undefined)
      tree["/settings/default"] = {
        ...(tree["/settings/default"] ?? {}),
        "script root": this.value,
      };
    await this.nscp.configure(tree);
  }

  /** Boot one module from the ini and run one query; the output holds what the load logged too. */
  async run(module: string, query: string, ...args: string[]): Promise<string> {
    const r = await this.nscp.run(
      ["client", "--module", module, "--boot", "--log", "debug", "--query", query, ...args],
      {
        allowFailure: true,
      },
    );
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }

  /** One of the module command-line verbs (`ext-scr list`, `lua show`, ...). */
  async cli(...argv: string[]): Promise<string> {
    const r = await this.nscp.run(argv, { allowFailure: true });
    return r.all ?? `${r.stdout}\n${r.stderr}`;
  }
}

const slashes = (s: string) => s.replace(/\\/g, "/");
const samePath = (a: string, b: string) => {
  const na = slashes(path.resolve(a));
  const nb = slashes(path.resolve(b));
  return onWindows ? na.toLowerCase() === nb.toLowerCase() : na === nb;
};

for (const c of cases) {
  describe(`script root ${c.name}`, () => {
    let p: Probe;
    let mkPort = 46000 + cases.indexOf(c);

    beforeAll(async () => {
      p = new Probe(c);
      await p.configure({
        "/modules": {
          CheckExternalScripts: "enabled",
          LUAScript: "enabled",
          CheckMKClient: "enabled",
          CheckMKServer: "enabled",
          ...(moduleBuiltHere("PythonScript") ? { PythonScript: "enabled" } : {}),
        },
        "/settings/external scripts/scripts": { root_probe_ext: p.extCommand },
        "/settings/lua/scripts": { root_probe: "root_probe.lua" },
        "/settings/python/scripts": { root_probe: "root_probe.py" },
        "/settings/check_mk/server": { port: String(mkPort) },
      });
    });

    it("starts an external script in the folder above the scripts folder", async () => {
      const out = await p.run("CheckExternalScripts", "root_probe_ext");
      expect(out).toContain("ext probe ok");
      const cwd = /cwd=(.+)/.exec(out)?.[1]?.trim() ?? "";
      expect(samePath(fs.realpathSync(cwd), fs.realpathSync(p.startDir))).toBe(true);
    });

    it("lists and shows external scripts from the scripts folder, and nothing above it", async () => {
      const listed = slashes(await p.cli("ext-scr", "list"));
      expect(listed).toContain("root_probe.sh");
      expect(
        await p.cli("ext-scr", "show", "--script", path.join(p.scripts, "root_probe.sh")),
      ).toContain("ext probe ok");
      // The ini sits above every tree this suite builds.
      const outside = await p.cli("ext-scr", "show", "--script", p.nscp.settingsFile);
      expect(outside).toContain("Not allowed outside");
    });

    it("loads a Lua script from its lua folder, with lib on the require path", async () => {
      const out = await p.run("LUAScript", "root_probe_lua");
      expect(out).toContain(LUA_HELPER);
      expect(slashes(await p.cli("lua", "list"))).toContain("lua/root_probe.lua");
      expect(await p.cli("lua", "show", "--script", "root_probe.lua")).toContain(
        'require("root_helper")',
      );
      expect(await p.cli("lua", "show", "--script", "../../nsclient.ini")).toContain(
        "Not allowed outside",
      );
    });

    itWithModules("PythonScript")(
      "loads a Python script from its python folder, with lib importable",
      async () => {
        const out = await p.run("PythonScript", "root_probe_py");
        expect(out).toContain(PY_HELPER);
        expect(slashes(await p.cli("py", "list"))).toContain("python/root_probe.py");
        expect(await p.cli("py", "show", "--script", "root_probe.py")).toContain(
          "import root_helper",
        );
      },
    );

    it("CheckMKClient loads default_check_mk.lua from the lua folder", async () => {
      const out = await p.run("CheckMKClient", "no_such_command");
      expect(out).toContain(MK_LOADED);
      expect(out).not.toContain("Failed to find script");
    });

    it("CheckMKServer loads default_check_mk.lua from the lua folder", async () => {
      const out = await p.run("CheckMKServer", "no_such_command");
      expect(out).toContain(MK_LOADED);
      expect(out).not.toContain("Failed to find script");
    });
  });
}

// The control: a root that holds nothing. Every probe above must fail here,
// or it was not reading the root at all - the build's own scripts folder has
// a default_check_mk.lua of its own, which is exactly what this rules out.
describe("script root pointing at a folder that holds nothing", () => {
  let p: Probe;

  beforeAll(async () => {
    p = new Probe(cases[cases.length - 1], true);
    await p.configure({
      "/modules": {
        CheckExternalScripts: "enabled",
        LUAScript: "enabled",
        CheckMKClient: "enabled",
      },
      "/settings/external scripts/scripts": { root_probe_ext: p.extCommand },
      "/settings/lua/scripts": { root_probe: "root_probe.lua" },
    });
  });

  it("runs no external script, finds no Lua script and no default_check_mk.lua", async () => {
    expect(await p.run("CheckExternalScripts", "root_probe_ext")).not.toContain("ext probe ok");
    const lua = await p.run("LUAScript", "root_probe_lua");
    expect(lua).not.toContain(LUA_HELPER);
    expect(lua).toContain("Failed to find script");
    const mk = await p.run("CheckMKClient", "no_such_command");
    expect(mk).not.toContain(MK_LOADED);
    expect(mk).toContain("Failed to find script");
  });
});
