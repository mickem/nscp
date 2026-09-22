/**
 * Which folders a configured script is looked for in, and which it may be
 * loaded from.
 *
 * Three modules resolve scripts and none of them do it the same way, so this
 * pins the actual behaviour of each against a tree laid out like a real
 * install:
 *
 *   <root>/a.py                       above the script folder
 *   <root>/scripts/b.py               ${scripts}
 *   <root>/scripts/python/c.py        the module's own folder
 *   <root>/scripts/python/foo/d.py    nested below it
 *
 * The interesting part is not which spellings work but that the answer depends
 * on the process working directory: the search tries the configured value
 * as-is first, so the same nsclient.ini resolves differently depending on how
 * the agent was started. Both cases are covered - `rooted` runs with the
 * install root as the working directory (a Windows service, or the shipped
 * systemd unit), `neutral` runs from somewhere else entirely.
 *
 * A script is deliberately NOT confined to the script folder: an absolute path
 * anywhere is accepted, and so is one that climbs out with `..`. Both are
 * pinned below so that stops being an accident.
 *
 * Outcomes are read from the log rather than from a check result, because a
 * check result cannot distinguish "not found" from "found and failed".
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { NscpInstance } from "@fixtures/index";

jest.setTimeout(180_000);

type Outcome = "loaded" | "not-found";

/** Lay out the probe tree under `root` for the given module's extension. */
function buildTree(root: string, sub: string, ext: string): void {
  const write = (p: string) => {
    fs.mkdirSync(path.dirname(p), { recursive: true });
    fs.writeFileSync(p, ext === ".lua" ? "-- probe\n" : "# probe\n");
  };
  write(path.join(root, `a${ext}`));
  write(path.join(root, "scripts", `b${ext}`));
  write(path.join(root, "scripts", sub, `c${ext}`));
  write(path.join(root, "scripts", sub, "foo", `d${ext}`));
}

function classify(output: string): Outcome {
  if (/Adding script:/.test(output)) return "loaded";
  return "not-found";
}

describe("script folder resolution", () => {
  /**
   * One probe: configure `value` as the module's only script and report what
   * happened. `cwd` selects whether the agent runs from the install root.
   */
  async function probe(opts: {
    module: string;
    section: string;
    root: string;
    value: string;
    cwd: "rooted" | "neutral";
  }): Promise<Outcome> {
    // workDir is the process working directory, so pointing it at the install
    // root is what makes the CWD-first candidate fire.
    const workDir =
      opts.cwd === "rooted" ? opts.root : fs.mkdtempSync(path.join(os.tmpdir(), "nscp-neutral-"));
    const nscp = new NscpInstance({
      workDir,
      settingsFile: path.join(workDir, `probe-${Math.random().toString(36).slice(2)}.ini`),
      pathOverrides: { scripts: path.join(opts.root, "scripts") },
    });
    fs.writeFileSync(
      nscp.settingsFile,
      [
        "[/modules]",
        `${opts.module} = enabled`,
        "",
        `[/settings/${opts.section}/scripts]`,
        `probe = ${opts.value}`,
        "",
      ].join("\n"),
    );
    const r = await nscp.run(
      ["client", "--module", opts.module, "--boot", "--log", "debug", "--query", "no_such_command"],
      {
        allowFailure: true,
      },
    );
    return classify(r.all ?? `${r.stdout}\n${r.stderr}`);
  }

  // --- PythonScript / LUAScript share a shape, so drive them from one table --
  //
  // `python/c.py` and `foo/d.py` both work because the search tries the value
  // under the module's folder *and* directly under ${scripts}; the spellings
  // that include a literal "scripts" segment only work when the working
  // directory happens to be the install root, which is the point of the two
  // cwd columns.
  const matrix: Array<{
    value: (s: string, e: string) => string;
    rooted: Outcome;
    neutral: Outcome;
    why: string;
  }> = [
    {
      value: (_s, e) => `a${e}`,
      rooted: "loaded",
      neutral: "not-found",
      why: "above the script folder, reachable only via the working directory",
    },
    {
      value: (_s, e) => `../a${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "climbs out of the script folder, which is allowed",
    },
    {
      value: (_s, e) => `b${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "directly in ${scripts}",
    },
    {
      value: (_s, e) => `scripts/b${e}`,
      rooted: "loaded",
      neutral: "not-found",
      why: "only via the working directory",
    },
    {
      value: (_s, e) => `c${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "in the module's folder",
    },
    {
      value: (s, e) => `${s}/c${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "explicit module folder",
    },
    {
      value: (s, e) => `scripts/${s}/c${e}`,
      rooted: "loaded",
      neutral: "not-found",
      why: "only via the working directory",
    },
    {
      value: (_s, e) => `d${e}`,
      rooted: "not-found",
      neutral: "not-found",
      why: "nested, bare name does not reach it",
    },
    {
      value: (_s, e) => `foo/d${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "nested below the module's folder",
    },
    {
      value: (s, e) => `${s}/foo/d${e}`,
      rooted: "loaded",
      neutral: "loaded",
      why: "explicit nested path",
    },
  ];

  describe.each([
    { module: "PythonScript", section: "python", sub: "python", ext: ".py" },
    { module: "LUAScript", section: "lua", sub: "lua", ext: ".lua" },
  ])("$module", ({ module, section, sub, ext }) => {
    let root: string;
    let available = false;

    beforeAll(async () => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-scriptroot-"));
      buildTree(root, sub, ext);
      // The module is optional at build time (Python needs Boost.Python, Lua
      // needs liblua). Probe once: a build without it should skip, not fail.
      available =
        (await probe({ module, section, root, value: `c${ext}`, cwd: "neutral" })) === "loaded";
    });

    it("is built in this configuration", () => {
      if (!available) console.warn(`${module} not available in this build - folder matrix skipped`);
      expect(true).toBe(true);
    });

    for (const row of matrix) {
      for (const cwd of ["rooted", "neutral"] as const) {
        const value = row.value(sub, ext);
        const expected = cwd === "rooted" ? row.rooted : row.neutral;
        it(`${cwd}: '${value}' is ${expected} (${row.why})`, async () => {
          if (!available) return;
          expect(await probe({ module, section, root, value, cwd })).toBe(expected);
        });
      }
    }

    // --- a script may live anywhere ----------------------------------------

    it("loads an absolute path outside the script folder", async () => {
      if (!available) return;
      const vendor = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-vendor-"));
      fs.writeFileSync(path.join(vendor, `v${ext}`), ext === ".lua" ? "-- v\n" : "# v\n");
      // What an operator writes for a plugin the agent does not ship - a
      // monitoring-plugins check under libexec, a vendor drop in /opt.
      expect(
        await probe({ module, section, root, value: path.join(vendor, `v${ext}`), cwd: "neutral" }),
      ).toBe("loaded");
    });

    it("loads it from the rooted working directory too", async () => {
      if (!available) return;
      const vendor = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-vendor-"));
      fs.writeFileSync(path.join(vendor, `v${ext}`), ext === ".lua" ? "-- v\n" : "# v\n");
      expect(
        await probe({ module, section, root, value: path.join(vendor, `v${ext}`), cwd: "rooted" }),
      ).toBe("loaded");
    });
  });

  // --- CheckExternalScripts resolves nothing itself --------------------------
  //
  // Its value is a command line handed to the operating system, not a path the
  // agent resolves. So there is no search, no sandbox, and - the part that
  // catches people out - no ${...} expansion. Pinned here because it is the
  // opposite of the two modules above and the docs now promise it.
  (process.platform === "linux" ? describe : describe.skip)("CheckExternalScripts", () => {
    let root: string;

    /** Configure one command and return its output. */
    async function runScript(value: string, cwd: "rooted" | "neutral"): Promise<string> {
      const workDir =
        cwd === "rooted" ? root : fs.mkdtempSync(path.join(os.tmpdir(), "nscp-ext-neutral-"));
      const nscp = new NscpInstance({
        workDir,
        settingsFile: path.join(workDir, `ext-${Math.random().toString(36).slice(2)}.ini`),
        pathOverrides: { scripts: path.join(root, "scripts") },
      });
      fs.writeFileSync(
        nscp.settingsFile,
        [
          "[/modules]",
          "CheckExternalScripts = enabled",
          "",
          "[/settings/external scripts/scripts]",
          `probe = ${value}`,
          "",
        ].join("\n"),
      );
      const r = await nscp.run(
        ["client", "--module", "CheckExternalScripts", "--boot", "--query", "probe"],
        { allowFailure: true },
      );
      return r.all ?? `${r.stdout}\n${r.stderr}`;
    }

    beforeAll(() => {
      root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-extroot-"));
      const script = path.join(root, "scripts", "b.sh");
      fs.mkdirSync(path.dirname(script), { recursive: true });
      fs.writeFileSync(script, "#!/bin/sh\necho 'probe ok'\nexit 0\n");
      fs.chmodSync(script, 0o755);
    });

    it("runs a relative command against the working directory", async () => {
      expect(await runScript("scripts/b.sh", "rooted")).toMatch(/probe ok/);
    });

    it("cannot find that same command from another working directory", async () => {
      expect(await runScript("scripts/b.sh", "neutral")).not.toMatch(/probe ok/);
    });

    it("does not expand path tokens", async () => {
      // The documented trap: ${scripts}/b.sh reaches the shell literally.
      expect(await runScript("${scripts}/b.sh", "rooted")).not.toMatch(/probe ok/);
    });

    it("treats a name with no separator as a PATH lookup, not a local file", async () => {
      // Even standing in the folder that holds it - which is why a bare script
      // name never works here, unlike the Python and Lua sections above.
      const workDir = path.join(root, "scripts");
      const nscp = new NscpInstance({
        workDir,
        settingsFile: path.join(workDir, "bare.ini"),
        pathOverrides: { scripts: path.join(root, "scripts") },
      });
      fs.writeFileSync(
        nscp.settingsFile,
        [
          "[/modules]",
          "CheckExternalScripts = enabled",
          "",
          "[/settings/external scripts/scripts]",
          "probe = b.sh",
          "",
        ].join("\n"),
      );
      const r = await nscp.run(
        ["client", "--module", "CheckExternalScripts", "--boot", "--query", "probe"],
        { allowFailure: true },
      );
      expect(r.all ?? "").not.toMatch(/probe ok/);
    });

    it("runs an absolute command from anywhere", async () => {
      expect(await runScript(path.join(root, "scripts", "b.sh"), "neutral")).toMatch(/probe ok/);
    });
  });
  // --- add --import writes where the loader looks --------------------------
  //
  // The CLI copies a script into the module's folder and records a value for
  // it. Those two halves are written in different places and have drifted
  // apart before, so the round trip is asserted end to end: where the file
  // landed, what went into the configuration, and that a fresh boot resolves
  // that value back to the copy.
  describe("add --import", () => {
    /** Run one import into a pristine tree and report what it produced. */
    async function importScript(opts: {
      cli: string;
      module: string;
      section: string;
      name: string;
      body: string;
    }): Promise<{
      output: string;
      scripts: string[];
      configured?: string;
      scriptsDir: string;
      reload: string;
    }> {
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-import-"));
      const src = path.join(root, "src", opts.name);
      fs.mkdirSync(path.dirname(src), { recursive: true });
      fs.writeFileSync(src, opts.body);
      // Deliberately no `scripts` directory: on Windows it only materialises
      // when the sample scripts feature is selected, and a ${scripts}
      // override can point anywhere. The import has to create what it needs.
      const scriptsDir = path.join(root, "scripts");
      const overrides = { scripts: scriptsDir };
      const nscp = new NscpInstance({
        workDir: root,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: overrides,
      });
      const r = await nscp.run([opts.cli, "add", "--script", opts.name, "--import", src], {
        allowFailure: true,
      });

      const found: string[] = [];
      const walk = (dir: string) => {
        if (!fs.existsSync(dir)) return;
        for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
          const p = path.join(dir, e.name);
          if (e.isDirectory()) walk(p);
          else found.push(path.relative(scriptsDir, p).split(path.sep).join("/"));
        }
      };
      walk(scriptsDir);

      const ini = fs.readFileSync(nscp.settingsFile, "utf8");
      const section = ini.split(`[/settings/${opts.section}/scripts]`)[1] ?? "";
      const configured = section
        .split("\n")
        .map((l) => l.trim())
        .filter((l) => l && !l.startsWith(";") && !l.startsWith("["))
        .map((l) => l.slice(l.indexOf("=") + 1).trim())[0];

      // A second process, started from somewhere else entirely, so nothing
      // about the resolution can be riding on the importing process's state.
      const reader = new NscpInstance({
        workDir: fs.mkdtempSync(path.join(os.tmpdir(), "nscp-import-read-")),
        settingsFile: nscp.settingsFile,
        pathOverrides: overrides,
      });
      const boot = await reader.run(
        [
          "client",
          "--module",
          opts.module,
          "--boot",
          "--log",
          "debug",
          "--query",
          "no_such_command",
        ],
        { allowFailure: true },
      );

      return {
        output: r.all ?? `${r.stdout}\n${r.stderr}`,
        scripts: found,
        configured,
        scriptsDir,
        reload: boot.all ?? `${boot.stdout}\n${boot.stderr}`,
      };
    }

    it("PythonScript imports into ${scripts}/python and resolves it again", async () => {
      const r = await importScript({
        cli: "py",
        module: "PythonScript",
        section: "python",
        name: "imported.py",
        body: "# imported\n",
      });
      if (/No such (module|command)|not a valid/i.test(r.output)) return; // not built
      expect(r.scripts).toEqual(["python/imported.py"]);
      expect(r.configured).toBe("python/imported.py");
      expect(r.reload).toMatch(/Adding script:.*imported\.py/);
    });

    it("LUAScript imports into ${scripts}/lua and resolves it again", async () => {
      const r = await importScript({
        cli: "lua",
        module: "LUAScript",
        section: "lua",
        name: "imported.lua",
        body: "-- imported\n",
      });
      if (/No such (module|command)|not a valid/i.test(r.output)) return; // not built
      expect(r.scripts).toEqual(["lua/imported.lua"]);
      expect(r.configured).toBe("lua/imported.lua");
      expect(r.reload).toMatch(/Adding script:.*imported\.lua/);
    });

    it("CheckExternalScripts records a command that runs from any directory", async () => {
      const r = await importScript({
        cli: "ext-scr",
        module: "CheckExternalScripts",
        section: "external scripts",
        name: "imported.sh",
        body: "#!/bin/sh\necho 'import ok'\n",
      });
      expect(r.scripts).toEqual(["imported.sh"]);
      // This module expands nothing and searches nowhere, so whatever is
      // recorded has to name the file on its own. The fixture points `script
      // root` at a temp directory, which is not below ${base-path} on either
      // platform, so the short historic "scripts\\name" spelling is not
      // available here - and recording it anyway was the bug: the copy landed
      // in the temp directory while the command named <install>\scripts\, and
      // the check exited 127. On unix it was never even a path, the backslash
      // being an ordinary filename character there.
      //
      // Asserted on both platforms as "names the file that was actually
      // written", which is the property that matters and the one the old
      // Windows expectation quietly violated.
      const recorded = (r.configured ?? "").replace(/^"|"$/g, "");
      expect(path.isAbsolute(recorded)).toBe(true);
      expect(path.resolve(recorded)).toBe(path.resolve(path.join(r.scriptsDir, "imported.sh")));
      if (process.platform !== "win32") {
        fs.chmodSync(r.configured as string, 0o755);
        const elsewhere = new NscpInstance({
          workDir: fs.mkdtempSync(path.join(os.tmpdir(), "nscp-import-run-")),
          settingsFile: path.join(
            path.dirname(path.dirname(r.configured as string)),
            "nsclient.ini",
          ),
          pathOverrides: { scripts: path.dirname(r.configured as string) },
        });
        const run = await elsewhere.run(
          ["client", "--module", "CheckExternalScripts", "--boot", "--query", "imported"],
          { allowFailure: true },
        );
        expect(run.all ?? "").toMatch(/import ok/);
      }
    });

    it("lists the imported script at a path that show can resolve", async () => {
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-import-list-"));
      const src = path.join(root, "listed.sh");
      fs.writeFileSync(src, "#!/bin/sh\nexit 0\n");
      const nscp = new NscpInstance({
        workDir: root,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: { scripts: path.join(root, "scripts") },
      });
      await nscp.run(["ext-scr", "add", "--script", "listed.sh", "--import", src], {
        allowFailure: true,
      });
      const listed = await nscp.run(["ext-scr", "list"], { allowFailure: true });
      const entry = (listed.stdout ?? "")
        .split("\n")
        .map((l) => l.trim())
        .find((l) => l.endsWith("listed.sh"));
      expect(entry).toBeDefined();
      // The listing is what the web UI shows and what `show` is called back
      // with, so it has to name a real file. It used to come back with its
      // leading separator sliced off whenever ${scripts} was not below
      // ${base-path}, which on unix is always.
      const shown = await nscp.run(["ext-scr", "show", "--script", entry as string], {
        allowFailure: true,
      });
      expect(shown.stdout ?? "").toMatch(/#!\/bin\/sh/);
    });

    it("refuses to import without a destination name", async () => {
      const root = fs.mkdtempSync(path.join(os.tmpdir(), "nscp-import-bare-"));
      const src = path.join(root, "src.sh");
      fs.writeFileSync(src, "#!/bin/sh\nexit 0\n");
      const nscp = new NscpInstance({
        workDir: root,
        settingsFile: path.join(root, "nsclient.ini"),
        pathOverrides: { scripts: path.join(root, "scripts") },
      });
      const r = await nscp.run(["ext-scr", "add", "--import", src], { allowFailure: true });
      // Without --script the destination used to collapse onto the script root
      // itself, overwriting that path with a copy of the source file.
      expect(r.all ?? "").toMatch(/No script specified/);
      expect(fs.existsSync(path.join(root, "scripts"))).toBe(false);
    });
  });
});
